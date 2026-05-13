fn main() {
    println!("cargo:rerun-if-env-changed=DEP_MINIFI_BEHAVE_PATH");
    let behave_path =
        std::env::var("DEP_MINIFI_BEHAVE_PATH").expect("DEP_MINIFI_BEHAVE_PATH is required");
    println!("cargo:rustc-env=MINIFI_BEHAVE_PATH={}", behave_path);
}
