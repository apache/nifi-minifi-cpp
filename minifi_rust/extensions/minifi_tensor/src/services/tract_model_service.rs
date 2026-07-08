// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

use crate::services::tract_model_service::service_definition::{MODEL_FILE_PATH, MODEL_FORMAT};
use minifi_native::macros::{ComponentIdentifier, PropertyType};
use minifi_native::{EnableControllerService, GetProperty, Logger, MinifiError, trace};
use std::path::Path;
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};
use tract::__ndarray_interop::anyhow;
use tract::prelude::*;

mod service_definition;

#[derive(
    Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr, PropertyType,
)]
#[strum(serialize_all = "PascalCase", const_into_str)]
pub(crate) enum ModelFormat {
    Auto,
    Onnx,
    Nnef,
}

/// Resolved format after any auto-detection.
#[derive(Debug, Clone, Copy, PartialEq)]
enum ResolvedFormat {
    Onnx,
    Nnef,
}

impl ModelFormat {
    fn resolve(self, path: &Path) -> Result<ResolvedFormat, MinifiError> {
        match self {
            ModelFormat::Onnx => Ok(ResolvedFormat::Onnx),
            ModelFormat::Nnef => Ok(ResolvedFormat::Nnef),
            ModelFormat::Auto => {
                let path_str = path.to_string_lossy().to_ascii_lowercase();
                if path_str.ends_with(".onnx") {
                    Ok(ResolvedFormat::Onnx)
                } else if path_str.ends_with(".nnef")
                    || path_str.ends_with(".nnef.tgz")
                    || path_str.ends_with(".nnef.tar")
                    || path_str.ends_with(".nnef.tar.gz")
                    || path.is_dir()
                {
                    Ok(ResolvedFormat::Nnef)
                } else {
                    Err(MinifiError::custom(format!(
                        "Could not auto-detect model format from '{:?}'. Set 'Model format' to \
                         'Onnx' or 'Nnef' explicitly.",
                        path
                    )))
                }
            }
        }
    }
}

#[derive(ComponentIdentifier)]
pub(crate) struct TractModelService {
    runnable_model: Runnable,
}

impl EnableControllerService for TractModelService {
    fn enable<Ctx: GetProperty, L: Logger>(context: &Ctx, logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let model_path = context.get_property(&MODEL_FILE_PATH)?;
        let format = context.get_property(&MODEL_FORMAT)?;
        let resolved = format.resolve(&model_path)?;

        trace!(
            logger,
            "Loading Tract model ({:?}) from: {:?}", resolved, model_path
        );

        let model = match resolved {
            ResolvedFormat::Onnx => onnx()?.load(&model_path)?.into_model()?,
            ResolvedFormat::Nnef => nnef()?.load(&model_path)?,
        };

        let runtime = runtime_for_name("default")?;
        let runnable_model = runtime.prepare(model)?;

        trace!(logger, "Successfully loaded and compiled Tract model.");

        Ok(Self { runnable_model })
    }
}

impl TractModelService {
    pub fn run_inference(
        &self,
        inputs: impl IntoIterator<Item = Tensor>,
    ) -> anyhow::Result<Vec<Tensor>> {
        let vec_inputs: Vec<Tensor> = inputs.into_iter().collect();

        self.runnable_model.run(vec_inputs)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;
    use std::str::FromStr;

    #[test]
    fn test_resolve_explicit_formats_bypass_detection() {
        assert_eq!(
            ModelFormat::Onnx
                .resolve(&PathBuf::from_str("/tmp/no-extension").unwrap())
                .unwrap(),
            ResolvedFormat::Onnx
        );
        assert_eq!(
            ModelFormat::Nnef
                .resolve(&PathBuf::from_str("/tmp/no-extension").unwrap())
                .unwrap(),
            ResolvedFormat::Nnef
        );
    }

    #[test]
    fn test_resolve_auto_by_onnx_extension() {
        assert_eq!(
            ModelFormat::Auto
                .resolve(&PathBuf::from_str("/models/face.onnx").unwrap())
                .unwrap(),
            ResolvedFormat::Onnx
        );
        assert_eq!(
            ModelFormat::Auto
                .resolve(&PathBuf::from_str("/MODELS/FACE.ONNX").unwrap())
                .unwrap(),
            ResolvedFormat::Onnx
        );
    }

    #[test]
    fn test_resolve_auto_by_nnef_extension() {
        assert_eq!(
            ModelFormat::Auto
                .resolve(&PathBuf::from_str("/models/mobilenet.nnef.tgz").unwrap())
                .unwrap(),
            ResolvedFormat::Nnef
        );
        assert_eq!(
            ModelFormat::Auto
                .resolve(&PathBuf::from_str("/models/mobilenet.nnef").unwrap())
                .unwrap(),
            ResolvedFormat::Nnef
        );
    }

    #[test]
    fn test_resolve_auto_errors_on_unknown() {
        assert!(
            ModelFormat::Auto
                .resolve(&PathBuf::from_str("/tmp/no-hint").unwrap())
                .is_err()
        );
    }
}
