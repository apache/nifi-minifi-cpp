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

use crate::services::tract_model_service::TractModelService;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, PropertyDefinition,
    Relationship, property_definitions,
};

pub(crate) const TRACT_MODEL_SERVICE: Property<TractModelService> = Property::new(
    "Tract model service",
    "Reference to a TractModelService controller service. The referenced service \
                  owns the compiled model (ONNX or NNEF) that will be evaluated for each \
                  incoming flow file.",
);

pub(super) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "Inference completed. The flow file's content is the concatenation of every \
                  output tensor's raw bytes in model output order.",
};

pub(super) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "The input tensor could not be built (missing/invalid tensor.shape, unsupported \
                  tensor.dtype, malformed payload) or the model failed to run.",
};

const OUTPUT_COUNT_ATTR: OutputAttribute = OutputAttribute {
    name: "tensors.len",
    relationships: &["success"],
    description: "Number of output tensors produced by the model. Downstream processors can loop \
                  from 0 up to this count when reading per-output attributes.",
};

const OUTPUT_SHAPE_ATTR: OutputAttribute = OutputAttribute {
    name: "tensor.{i}.shape",
    relationships: &["success"],
    description: "Comma-separated dimensions of output tensor at index i.",
};

const OUTPUT_BYTES_ATTR: OutputAttribute = OutputAttribute {
    name: "tensor.{i}.bytes",
    relationships: &["success"],
    description: "Byte length of output tensor at index i within the concatenated payload. \
                  Consumers slice the payload sequentially using these lengths.",
};

const OUTPUT_DTYPE_ATTR: OutputAttribute = OutputAttribute {
    name: "tensor.{i}.dtype",
    relationships: &["success"],
    description: "Element type of output tensor at index i",
};

impl ProcessorDefinition for super::InvokeTractModel {
    const DESCRIPTION: &'static str = "Runs a single inference against the compiled model owned by the referenced \
         TractModelService. Reads the input tensor from the flow file content plus the \
         'tensor.shape' and (optionally) 'tensor.dtype' attributes produced by an upstream \
         processor such as ImageToTensor. The flow file's new content is every output tensor's \
         raw bytes concatenated in model order; per-tensor shape, byte length, and dtype are \
         written to attributes. Only a single input tensor and only f32 input dtype are \
         supported in this pass.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[
        OUTPUT_COUNT_ATTR,
        OUTPUT_SHAPE_ATTR,
        OUTPUT_BYTES_ATTR,
        OUTPUT_DTYPE_ATTR,
    ];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];
    const PROPERTIES: &[PropertyDefinition] = property_definitions![TRACT_MODEL_SERVICE];
}
