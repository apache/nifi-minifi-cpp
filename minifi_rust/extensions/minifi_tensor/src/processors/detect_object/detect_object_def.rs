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

use crate::low_level_processors::{filter_bounding_boxes, image_to_tensor};
use crate::processors::detect_object::DetectObject;
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
                  unchanged image; the detected boxes are written to the configured output \
                  attribute as a JSON array (may be empty).",
};

pub(super) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "The image could not be decoded, the input tensor could not be built, the model \
                  failed to run, or the model outputs could not be interpreted as scores + boxes.",
};

const OBJECT_COUNT_ATTR: OutputAttribute = OutputAttribute {
    name: "object.count",
    relationships: &["success"],
    description: "Number of bounding boxes retained after confidence filtering and NMS.",
};

const DETECTED_OBJECTS_ATTR: OutputAttribute = OutputAttribute {
    name: "<Output attribute name>",
    relationships: &["success"],
    description: "JSON array of the surviving bounding boxes (fields class_id, confidence, x_min, \
                  y_min, x_max, y_max; coordinates normalised to [0,1] against the original \
                  image). The attribute name is configurable via the 'Output attribute name' \
                  property.",
};

impl ProcessorDefinition for DetectObject {
    const DESCRIPTION: &'static str = "Runs a full object-detection pass in a single processor: decodes the image from the flow \
         file content, resizes and normalises it into an input tensor, runs one inference against \
         the compiled model owned by the referenced TractModelService, and post-processes the \
         model outputs (score activation, confidence filtering, box decoding, per-class \
         non-maximum suppression) into bounding boxes. Collapses the ImageToTensor -> \
         InvokeTractModel -> FilterBoundingBoxes chain into one node. The flow file content is \
         left unchanged (the original image); the detected boxes are written as a JSON array to \
         the configured output attribute so a downstream DrawBoundingBox can annotate the image.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] =
        &[OBJECT_COUNT_ATTR, DETECTED_OBJECTS_ATTR];
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
        filter_bounding_boxes::CONFIDENCE_THRESHOLD,
        filter_bounding_boxes::IOU_THRESHOLD,
        filter_bounding_boxes::SCORE_OUTPUT_INDEX,
        filter_bounding_boxes::BOX_OUTPUT_INDEX,
        filter_bounding_boxes::CLASS_OUTPUT_INDEX,
        filter_bounding_boxes::BOX_FORMAT,
        filter_bounding_boxes::SCORE_ACTIVATION,
        filter_bounding_boxes::BACKGROUND_CLASS_INDEX,
        filter_bounding_boxes::OUTPUT_ATTRIBUTE_NAME,
    ];
}
