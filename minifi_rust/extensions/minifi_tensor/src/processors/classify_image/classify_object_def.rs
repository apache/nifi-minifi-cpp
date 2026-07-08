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

use crate::low_level_processors::{classify_output, image_to_tensor};
use crate::processors::classify_image::ClassifyImage;
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
    description: "Inference and post-processing completed. The flow file content is the original, \
                  unchanged image; the classifications are written to the configured output \
                  attribute as a JSON array (may be empty).",
};

pub(super) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "The image could not be decoded, the input tensor could not be built, the model \
                  failed to run, or the model outputs could not be interpreted as scores + boxes.",
};

impl ProcessorDefinition for ClassifyImage {
    const DESCRIPTION: &'static str = "Runs a full image-classification pass in a single processor: decodes the image from the \
         flow file content, resizes and normalises it into an input tensor, runs one inference \
         against the compiled model owned by the referenced TractModelService, and post-processes \
         the score vector (score activation, Top-K selection, confidence filtering, optional label \
         lookup) into predictions. Collapses the ImageToTensor -> InvokeTractModel -> \
         ClassifyOutput chain into one node. The flow file content is left unchanged (the original \
         image); the Top-K classifications are written as a JSON array to the configured output \
         attribute.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];

    const PROPERTIES: &'static [PropertyDefinition] = property_definitions![
        image_to_tensor::TARGET_WIDTH,
        image_to_tensor::TARGET_HEIGHT,
        image_to_tensor::RESIZE_FILTER,
        image_to_tensor::RESIZE_MODE,
        image_to_tensor::LETTERBOX_PAD_VALUE,
        image_to_tensor::COLOR_FORMAT,
        image_to_tensor::TENSOR_SHAPE_FORMAT,
        image_to_tensor::MEAN,
        image_to_tensor::STD_DEV,
        image_to_tensor::PIXEL_DIVISOR,
        TRACT_MODEL_SERVICE,
        classify_output::TOP_K,
        classify_output::SCORE_OUTPUT_INDEX,
        classify_output::SCORE_ACTIVATION,
        classify_output::CONFIDENCE_THRESHOLD,
        classify_output::LABELS_FILE_PATH,
        classify_output::LABEL_INDEX_OFFSET,
        classify_output::OUTPUT_ATTRIBUTE_NAME
    ];
}
