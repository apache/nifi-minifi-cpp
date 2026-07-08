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

use super::{ClassifyOutput, ScoreActivation};
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, PropertyDefinition,
    Relationship, property_definitions,
};
use std::path::PathBuf;

pub(crate) const TOP_K: Property<usize> = Property::new(
    "Top K",
    "Number of highest-scoring classes to include in the output JSON, in descending \
                  order of confidence. Values above the total class count are clamped. Set to 1 \
                  for pure top-1 classification.",
)
.with_default("5");

pub(crate) const SCORE_OUTPUT_INDEX: Property<usize> = Property::new(
    "Score output index",
    "Zero-based index of the model output tensor that holds classification scores. \
                  The processor slices the concatenated payload from InvokeTractModel according \
                  to the 'tensor.N.bytes' attributes. Almost always 0 for single-head \
                  classifiers.",
)
.with_default("0");

pub(crate) const SCORE_ACTIVATION: Property<ScoreActivation> = Property::new(
    "Score activation",
    "Activation applied to the raw score vector before ranking. \
                  Softmax = mutually-exclusive classes (ImageNet-trained ResNet/MobileNet/\
                  EfficientNet raw logits). \
                  Sigmoid = independent classes (multi-label classifiers). \
                  None = the model already emits probabilities/scores; rank the raw values.",
)
.with_default(ScoreActivation::Softmax.into_str());

pub(crate) const CONFIDENCE_THRESHOLD: Property<f32> = Property::new(
    "Confidence Threshold",
    "Minimum confidence a class must reach to be included in the output JSON. \
                  Applied AFTER activation, so the units match the chosen activation \
                  (0.0..=1.0 for Softmax/Sigmoid, model-native for None). Set to 0.0 to always \
                  emit exactly Top K predictions.",
)
.with_default("0.0")
.supports_expression_language();

pub(crate) const LABELS_FILE_PATH: Property<Option<PathBuf>> = Property::new(
    "Labels file path",
    "Optional path to a newline-separated labels file (line N = name of class N). \
                  Loaded once at service enable time. When set, each prediction in the output \
                  JSON gains a 'class_name' field and the 'class.top1.name' flow file attribute \
                  is populated. Leave empty to emit numeric class IDs only.",
);

pub(crate) const LABEL_INDEX_OFFSET: Property<usize> = Property::new(
    "Label index offset",
    "Offset added to the model's class ID when looking up a name in the labels file. \
                  Defaults to 0 (labels file line N = class N). Set to 1 for label files that \
                  start with a dummy/background entry — e.g. the ONNX MobileNetV2 model emits \
                  1000 class scores while 'imagenet_slim_labels.txt' has 1001 lines (line 0 = \
                  'dummy'), so class ID 653 maps to line 654 = 'military uniform'.",
)
.with_default("0");

pub(super) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "Classification completed. The flow file content is a JSON array of the Top K \
                  predictions (possibly fewer if the confidence threshold filtered some out).",
};

pub(super) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "The upstream output attributes were missing/invalid or the score tensor could \
                  not be interpreted as f32 values.",
};

const MIME_TYPE_ATTR: OutputAttribute = OutputAttribute {
    name: "mime.type",
    relationships: &["success"],
    description: "Always 'application/json' — the output payload is a JSON array of prediction \
                  objects with fields class_id, confidence, and optional class_name.",
};

const CLASS_COUNT_ATTR: OutputAttribute = OutputAttribute {
    name: "class.count",
    relationships: &["success"],
    description: "Number of predictions retained after Top K selection and confidence filtering.",
};

const CLASS_TOP1_ID_ATTR: OutputAttribute = OutputAttribute {
    name: "class.top1.id",
    relationships: &["success"],
    description: "Numeric class ID of the highest-confidence prediction, when at least one \
                  prediction cleared the confidence threshold.",
};

const CLASS_TOP1_CONFIDENCE_ATTR: OutputAttribute = OutputAttribute {
    name: "class.top1.confidence",
    relationships: &["success"],
    description: "Confidence (post-activation) of the highest-confidence prediction, when at \
                  least one prediction cleared the confidence threshold.",
};

const CLASS_TOP1_NAME_ATTR: OutputAttribute = OutputAttribute {
    name: "class.top1.name",
    relationships: &["success"],
    description: "Label of the highest-confidence prediction. Only present when 'Labels file \
                  path' was configured and at least one prediction cleared the threshold.",
};

pub(crate) const OUTPUT_ATTRIBUTE_NAME: Property<Option<String>> = Property::new(
    "Output attribute name",
    "Specify the attribute to use as output, if not provided, the content is overridden instead.",
)
.supports_expression_language();

impl ProcessorDefinition for ClassifyOutput {
    const DESCRIPTION: &'static str = "Post-processes the output of a classification model invoked via InvokeTractModel. Reads \
         the flattened score vector from the configured output tensor index, applies the chosen \
         activation (softmax / sigmoid / none), and emits the Top K classes as a JSON array \
         [{class_id, confidence, class_name?}, ...]. Optional labels file maps numeric class IDs \
         to human-readable names. Works with ImageNet-style ResNet/MobileNet/EfficientNet \
         checkpoints (Softmax over raw logits) as well as multi-label classifiers (Sigmoid) and \
         models that already emit probabilities (None). Assumes a single flattened score \
         vector — upstream ImageToTensor produces batch=1 tensors, so this is the common case.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[
        MIME_TYPE_ATTR,
        CLASS_COUNT_ATTR,
        CLASS_TOP1_ID_ATTR,
        CLASS_TOP1_CONFIDENCE_ATTR,
        CLASS_TOP1_NAME_ATTR,
    ];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];

    const PROPERTIES: &[PropertyDefinition] = property_definitions![
        TOP_K,
        SCORE_OUTPUT_INDEX,
        SCORE_ACTIVATION,
        CONFIDENCE_THRESHOLD,
        LABELS_FILE_PATH,
        LABEL_INDEX_OFFSET,
        OUTPUT_ATTRIBUTE_NAME
    ];
}
