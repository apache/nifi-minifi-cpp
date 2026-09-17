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

use super::{BoxFormat, FilterBoundingBoxes, ScoreActivation};
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, PropertyDefinition,
    Relationship, property_definitions,
};

pub(crate) const CONFIDENCE_THRESHOLD: Property<f32> = Property::new(
    "Confidence Threshold",
    "Minimum per-box class probability (0.0 to 1.0) required to keep a bounding box \
                  after applying the chosen 'Score activation'. Boxes below the threshold are \
                  discarded before NMS.",
)
.with_default("0.7")
.supports_expression_language();

pub(crate) const IOU_THRESHOLD: Property<f32> = Property::new(
    "IoU Threshold",
    "Intersection-over-union cutoff used during non-maximum suppression. Boxes of \
                  the same class whose IoU with a higher-confidence peer exceeds this value are \
                  suppressed. Typical values: 0.45 (SSD/YOLO default), 0.5, 0.3 for stricter \
                  deduplication.",
)
.with_default("0.45");

pub(crate) const SCORE_OUTPUT_INDEX: Property<usize> = Property::new(
    "Score output index",
    "Zero-based index of the model output tensor that holds classification scores. \
                  The processor slices the concatenated payload from InvokeTractModel according \
                  to the 'tensor.N.bytes' attributes.",
)
.with_default("0");

pub(crate) const BOX_OUTPUT_INDEX: Property<usize> = Property::new(
    "Box output index",
    "Zero-based index of the model output tensor that holds box coordinates. Must \
                  differ from 'Score output index'.",
)
.with_default("1");

pub(crate) const CLASS_OUTPUT_INDEX: Property<Option<usize>> = Property::new(
    "Class output index",
    "Zero-based index of a model output tensor that holds one class id per box. Set this \
                  for detectors that emit boxes, per-box scores, and class ids as three separate \
                  parallel tensors, with NMS already folded into the graph (TensorFlow Object \
                  Detection API; YOLO / EfficientNMS 'end2end' exports). When set, 'Score output \
                  index' is read as one score per box (not a [boxes, classes] matrix) and no \
                  argmax is performed; the class id tensor may be integer- or float-typed. Leave \
                  empty for models that emit a per-class score matrix.",
);

pub(crate) const BOX_FORMAT: Property<BoxFormat> = Property::new(
    "Box format",
    "Layout of the four floats per box in the box output tensor. Xyxy = \
                  [x_min, y_min, x_max, y_max] (SSD, MobileNet-SSD, most PyTorch exports). \
                  Yxyx = [y_min, x_min, y_max, x_max] (TensorFlow Object Detection API). \
                  Cxcywh = [cx, cy, w, h] (YOLOv3/5/8 raw output).",
)
.with_default(BoxFormat::Xyxy.into_str());

pub(crate) const SCORE_ACTIVATION: Property<ScoreActivation> = Property::new(
    "Score activation",
    "Activation applied to raw per-class scores before selecting the winning class. \
                  Softmax = mutually-exclusive classes (SSD/MobileNet-SSD raw logits). \
                  Sigmoid = independent classes (YOLOv5/v8 style). \
                  None = the model already emits probabilities/scores; use raw argmax with the \
                  raw score as confidence.",
)
.with_default(ScoreActivation::Softmax.into_str());

pub(crate) const BACKGROUND_CLASS_INDEX: Property<Option<usize>> = Property::new(
    "Background class index",
    "Index of the 'background / no-object' class. Boxes whose winning class equals this \
                  index are dropped. In score-matrix mode this is only honoured when the score \
                  tensor has more than one class per box; in 'Class output index' mode it is \
                  matched against each box's class id.",
);

pub(crate) const OUTPUT_ATTRIBUTE_NAME: Property<Option<String>> = Property::new(
    "Output attribute name",
    "Specify the attribute to use as output, if not provided, the content is overridden instead.",
)
.supports_expression_language();

pub const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "Filtering completed. The flow file content is a JSON array of the surviving \
                  bounding boxes (may be empty).",
};

pub const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "The upstream output attributes were missing/invalid, the payload was truncated, \
                  or the tensors could not be interpreted as scores + boxes.",
};

const OBJECT_COUNT_ATTR: OutputAttribute = OutputAttribute {
    name: "object.count",
    relationships: &["success"],
    description: "Number of bounding boxes retained after confidence filtering and NMS.",
};

const MIME_TYPE_ATTR: OutputAttribute = OutputAttribute {
    name: "mime.type",
    relationships: &["success"],
    description: "Always 'application/json' — the output payload is a JSON array of objects with \
                  fields class_id, confidence, x_min, y_min, x_max, y_max.",
};

impl ProcessorDefinition for FilterBoundingBoxes {
    const DESCRIPTION: &'static str = "Post-processes the concatenated output of InvokeTractModel for object-detection models. \
         Reads the classification score tensor and the box coordinate tensor from the flow file \
         payload (indices configurable), applies the configured score activation, filters by \
         confidence, decodes box coordinates from the configured layout, and applies per-class \
         non-maximum suppression at the configured IoU threshold. Handles both per-class score \
         matrices (argmax per box) and detectors that emit boxes / per-box scores / class ids as \
         separate tensors with NMS folded into the graph (set 'Class output index'). Works with \
         SSD-, YOLO-, and TensorFlow-style detectors by tuning properties — no code changes needed \
         for common model families. Emits a JSON array of the surviving boxes.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[OBJECT_COUNT_ATTR, MIME_TYPE_ATTR];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];

    const PROPERTIES: &[PropertyDefinition] = property_definitions![
        CONFIDENCE_THRESHOLD,
        IOU_THRESHOLD,
        SCORE_OUTPUT_INDEX,
        BOX_OUTPUT_INDEX,
        CLASS_OUTPUT_INDEX,
        BOX_FORMAT,
        SCORE_ACTIVATION,
        BACKGROUND_CLASS_INDEX,
        OUTPUT_ATTRIBUTE_NAME,
    ];
}
