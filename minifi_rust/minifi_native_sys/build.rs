use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

struct SDK {
    header_path: PathBuf,
    #[cfg_attr(not(windows), allow(dead_code))]
    def_path: PathBuf,
    behave_path: PathBuf,
}

impl SDK {
    fn from_local_repo(repo_root: &Path) -> Option<Self> {
        let canon = std::fs::canonicalize(&repo_root).ok()?;
        println!(
            "cargo:info=MINIFI_SDK_PATH from from_local_repo: {:?} -> {:?}",
            repo_root, canon
        );

        let header_path = canon
            .to_path_buf()
            .join("minifi-api/include/minifi-api.h");
        let def_path = canon.to_path_buf().join("minifi-api/minifi-api.def");
        let behave_path = canon.to_path_buf().join("behave_framework");

        println!(
            "cargo:info=header_path {:?}\n def_path {:?}\n behave_path {:?}",
            header_path, def_path, behave_path
        );

        if !header_path.exists() || !def_path.exists() {
            return None;
        }

        Some(Self {
            header_path,
            def_path,
            behave_path,
        })
    }

    fn from_local_sdk_path(sdk_path: &Path) -> Option<Self> {
        let base_path = if sdk_path.join("minifi-api.h").exists() {
            sdk_path.to_path_buf()
        } else if sdk_path.join("minifi-native-sdk/minifi-api.h").exists() {
            sdk_path.join("minifi-native-sdk")
        } else {
            return None;
        };

        let header_path = base_path.join("minifi-api.h");
        let def_path = base_path.join("minifi-api.def");
        let behave_path = std::fs::read_dir(&base_path)
            .ok()?
            .filter_map(|e| e.ok())
            .find(|e| e.path().extension().unwrap_or_default() == "whl")
            .map(|e| e.path())?;

        if !header_path.exists() || !def_path.exists() || !behave_path.exists() {
            return None;
        }

        Some(Self {
            header_path,
            def_path,
            behave_path,
        })
    }

    fn new_from_env_variable() -> Option<Self> {
        let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
        let sdk_path = env::var("MINIFI_SDK_PATH").expect(
            "MINIFI_SDK_PATH must be set. Please define it in your environment or .cargo/config.toml"
        );

        println!("cargo:info=Using MINIFI_SDK_PATH: {}", sdk_path);

        if sdk_path.starts_with("https://") {
            let zip_path = out_dir.join("downloaded-sdk.zip");
            let extract_dir = out_dir.join("extracted-sdk");
            if !extract_dir.exists() {
                Self::download_file(&sdk_path, &zip_path);
                Self::extract_zip(&zip_path, &extract_dir);
            }
            Self::from_local_sdk_path(&extract_dir)
        } else {
            let local_path = if sdk_path.starts_with(".") {
                PathBuf::from(format!("../{}", sdk_path))
            } else {
                PathBuf::from(&sdk_path)
            };
            if local_path.is_file() && local_path.extension().unwrap_or_default() == "zip" {
                let extract_dir = out_dir.join("extracted-local-sdk");
                if !extract_dir.exists() {
                    Self::extract_zip(&local_path, &extract_dir);
                }
                Self::from_local_sdk_path(&extract_dir)
            } else {
                Self::from_local_sdk_path(&local_path)
                    .or_else(|| Self::from_local_repo(&local_path))
            }
        }
    }

    fn download_file(url: &str, dest: &Path) {
        println!("cargo:warning=Downloading SDK from {}...", url);
        let status = Command::new("curl")
            .args(["-fSL", "-o", dest.to_str().unwrap(), url])
            .status()
            .expect("Failed to execute curl to download SDK");
        assert!(status.success(), "Failed to download SDK from {}", url);
    }

    fn extract_zip(archive: &Path, dest_dir: &Path) {
        println!("cargo:warning=Extracting SDK archive...");
        std::fs::create_dir_all(dest_dir).unwrap();
        let status = Command::new("tar")
            .args([
                "-xf",
                archive.to_str().unwrap(),
                "-C",
                dest_dir.to_str().unwrap(),
            ])
            .status()
            .expect("Failed to execute tar to extract SDK zip");
        assert!(status.success(), "Failed to extract SDK zip file");
    }
}

#[cfg(windows)]
fn generate_minifi_c_api_lib(def_path: &Path) {
    let tool = cc::Build::new()
        .try_get_compiler()
        .expect("Failed to find the MSVC toolchain. Is Visual Studio or the C++ Build Tools workload installed?");

    let lib_exe_path = tool
        .path()
        .parent()
        .expect("Compiler path is expected to be in a 'bin' directory.")
        .join("lib.exe");

    if !lib_exe_path.exists() {
        panic!(
            "Could not find lib.exe at the expected path: {}",
            lib_exe_path.display()
        );
    }

    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let lib_out_path = out_dir.join("minifi-api.lib");

    let status = Command::new(&lib_exe_path)
        .arg(format!("/def:{}", def_path.display()))
        .arg(format!("/out:{}", lib_out_path.display()))
        .arg(format!(
            "/machine:{}",
            env::var("CARGO_CFG_TARGET_ARCH").unwrap().to_uppercase()
        ))
        .status()
        .expect("Failed to execute lib.exe.");

    if !status.success() {
        panic!("lib.exe failed to create the import library from the .def file.");
    }

    println!("cargo:rustc-link-lib=dylib=minifi-api");
    println!("cargo:rustc-link-search=native={}", out_dir.display());
}

fn main() {
    println!("cargo:rerun-if-env-changed=MINIFI_SDK_PATH");

    let sdk = SDK::new_from_env_variable()
        .expect("Couldn't find valid SDK. Ensure MINIFI_SDK_PATH is set correctly.");

    #[cfg(windows)]
    generate_minifi_c_api_lib(&sdk.def_path);

    println!("cargo:rerun-if-changed={}", sdk.header_path.display());
    println!(
        "cargo:behave_path={}",
        sdk.behave_path.canonicalize().unwrap().display()
    );

    let bindings = bindgen::Builder::default()
        .header(sdk.header_path.to_str().unwrap())
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
