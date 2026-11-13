// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::services::tract_model_service::ModelFormat;
use minifi_native::{
    ControllerServiceDefinition, Property, PropertyDefinition, ProvidedInterface,
    property_definitions,
};
use std::path::PathBuf;

pub(crate) const MODEL_FILE_PATH: Property<PathBuf> = Property::new(
    "Model File Path",
    "Absolute path to the model on the edge device. For ONNX this is a `.onnx` \
                  file; for NNEF this is a `.nnef.tgz` archive, a `.nnef` tarball, or the root \
                  directory of an unpacked NNEF model. The model is loaded, parsed, and compiled \
                  for the host CPU once when the controller service is enabled; subsequent \
                  inference calls reuse the compiled runnable.",
);

pub(crate) const MODEL_FORMAT: Property<ModelFormat> = Property::new(
    "Model format",
    "Format of the file/directory referenced by 'Model File Path'. 'Auto' picks \
                  Onnx when the path ends in `.onnx` and Nnef when it ends in `.nnef`, \
                  `.nnef.tgz`, `.nnef.tar`, `.nnef.tar.gz`, or points at a directory. Set \
                  explicitly when the path uses a non-standard extension.",
)
.with_default(ModelFormat::Auto.into_str());

impl ControllerServiceDefinition for super::TractModelService {
    const DESCRIPTION: &'static str = "Provides a shared, CPU-optimized neural network for inference. Supports ONNX (`.onnx`) \
         and NNEF (directory or tarball) models; the format can be auto-detected from the file \
         extension or set explicitly.";
    const PROPERTIES: &'static [PropertyDefinition] =
        property_definitions![MODEL_FILE_PATH, MODEL_FORMAT];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}
