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

use crate::low_level_processors::classify_output::classify_output_def::{
    CLASS_COUNT_ATTR, CLASS_TOP1_CONFIDENCE_ATTR, CLASS_TOP1_ID_ATTR, CLASS_TOP1_NAME_ATTR,
};
use crate::utils::score_activation::{ScoreActivation, SoftmaxTerms};
use crate::utils::tensor_helpers::{deserialize_tensors, tensor_as_f32, tensor_shape};
use classify_output_def::SUCCESS;
pub(crate) use classify_output_def::{
    CLASSIFY_OUTPUT_ATTRIBUTES, CONFIDENCE_THRESHOLD, LABEL_INDEX_OFFSET, LABELS_FILE_PATH,
    MIME_TYPE_ATTR, OUTPUT_ATTRIBUTE_NAME, SCORE_ACTIVATION, SCORE_OUTPUT_INDEX, TOP_K,
};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    Content, FlowFileTransform, GetAttribute, GetId, GetProperty, InputStream, Logger, MinifiError,
    ProcessError, PropertyConstraints, PropertySchema, PropertyType, RouteErrorExt, Schedule,
    TransformedFlowFile, warn,
};
use serde::Serialize;
use tract::Tensor;

mod classify_output_def;

#[derive(Serialize, Clone, Debug, PartialEq)]
struct Prediction {
    class_id: usize,
    confidence: f32,
    #[serde(skip_serializing_if = "Option::is_none")]
    class_name: Option<String>,
}

pub(crate) struct LabelsProperty {}

impl PropertySchema for LabelsProperty {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = false;
}

impl PropertyType for LabelsProperty {
    type Output = Vec<String>;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        let content = std::fs::read_to_string(s).map_err(|e| {
            MinifiError::custom(format!("Failed to read labels file '{:?}': {}", s, e))
        })?;
        Ok(content
            .lines()
            .map(|line| line.trim().to_string())
            .collect())
    }
}

fn top_k(mut scored: Vec<(usize, f32)>, k: usize) -> Vec<(usize, f32)> {
    scored.sort_by(|&(ai, a), &(bi, b)| b.total_cmp(&a).then(ai.cmp(&bi)));
    scored.truncate(k);
    scored
}

#[derive(ComponentIdentifier)]
pub(crate) struct ClassifyOutput {
    top_k: usize,
    score_output_index: usize,
    score_activation: ScoreActivation,
    confidence_threshold: f32,
    labels: Option<Vec<String>>,
}

impl Schedule for ClassifyOutput {
    fn schedule<Ctx: GetProperty, L: Logger>(
        context: &Ctx,
        _logger: &L,
    ) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let top_k = context.get_property(&TOP_K)?;
        if top_k == 0 {
            return Err(MinifiError::validation("Top K must be >= 1"));
        }
        let score_output_index = context.get_property(&SCORE_OUTPUT_INDEX)?;
        let score_activation = context.get_property(&SCORE_ACTIVATION)?;
        let confidence_threshold = context.get_property(&CONFIDENCE_THRESHOLD)?;

        let label_index_offset = context.get_property(&LABEL_INDEX_OFFSET)?;
        let labels = if let Some(mut labels) = context.get_property(&LABELS_FILE_PATH)? {
            if let Some(dummy_indices) = label_index_offset {
                if dummy_indices >= labels.len() {
                    return Err(MinifiError::validation(format!(
                        "Label index offset ({}) must be smaller than the number of labels ({})",
                        dummy_indices,
                        labels.len()
                    )));
                }
                labels.drain(0..dummy_indices);
            }
            Some(labels)
        } else {
            if label_index_offset.is_some() {
                return Err(MinifiError::validation(
                    "Label index offset is set without valid labels, either unset label index offset or provide valid labels file",
                ));
            }
            None
        };

        Ok(Self {
            top_k,
            score_output_index,
            score_activation,
            confidence_threshold,
            labels,
        })
    }
}

impl ClassifyOutput {
    pub(crate) fn classify<'a, Context: GetProperty + GetAttribute + GetId, LoggerImpl: Logger>(
        &self,
        context: &Context,
        logger: &LoggerImpl,
        tensors: Vec<Tensor>,
    ) -> Result<TransformedFlowFile<'a>, ProcessError> {
        let score_floats =
            tensor_as_f32(&tensors, self.score_output_index).route_err_to_failure()?;
        if score_floats.is_empty() {
            return Err(ProcessError::route_to_failure(
                "Score tensor is empty; nothing to classify",
            ));
        }

        // A classifier head is a single score vector: shape [num_classes] or
        // [1, .., num_classes]. We rank over the flattened class axis, so any
        // leading axis > 1 (a real batch) would silently mix rows and yield
        // class ids past num_classes. Reject it rather than produce garbage.
        // (`ImageToTensor` emits batch=1 today; this just enforces the contract.)
        let shape = tensor_shape(&tensors, self.score_output_index).route_err_to_failure()?;
        if shape.iter().rev().skip(1).any(|&d| d != 1) {
            return Err(ProcessError::route_to_failure(format!(
                "ClassifyOutput expects a single score vector (shape [num_classes] or \
                 [1, .., num_classes]); got {shape:?}. A batch dimension > 1 is not supported."
            )));
        }

        let finite: Vec<(usize, f32)> = score_floats
            .iter()
            .copied()
            .enumerate()
            .filter(|&(_, s)| s.is_finite())
            .collect();

        let softmax_terms = SoftmaxTerms::over(finite.iter().map(|&(_, s)| s));

        let predictions: Vec<Prediction> = top_k(finite, self.top_k)
            .into_iter()
            .filter_map(|(class_id, raw)| {
                let confidence = self.score_activation.confidence(raw, softmax_terms);

                if confidence >= self.confidence_threshold {
                    let class_name = self.labels.as_ref().and_then(|l| l.get(class_id).cloned());
                    if class_name.is_none()
                        && let Some(l) = &self.labels
                    {
                        warn!(
                            logger,
                            "No label for class id {} ({} labels loaded); \
                             the labels file does not match the model's classes",
                            class_id,
                            l.len()
                        );
                    }
                    Some(Prediction {
                        class_id,
                        confidence,
                        class_name,
                    })
                } else {
                    None
                }
            })
            .collect();

        let (content, extra_attribute) = match context.get_property(&OUTPUT_ATTRIBUTE_NAME)? {
            None => (
                Some(Content::Buffer(
                    serde_json::to_vec(&predictions).route_err_to_failure()?,
                )),
                None,
            ),
            Some(output_attr) => (
                None,
                Some((
                    output_attr,
                    serde_json::to_string(&predictions).route_err_to_failure()?,
                )),
            ),
        };

        let mut transformed = TransformedFlowFile::new(&SUCCESS, content)
            .with_attribute(MIME_TYPE_ATTR.name, "application/json")
            .with_attribute(CLASS_COUNT_ATTR.name, predictions.len().to_string());

        if let Some(top) = predictions.first() {
            transformed = transformed
                .with_attribute(CLASS_TOP1_ID_ATTR.name, top.class_id.to_string())
                .with_attribute(CLASS_TOP1_CONFIDENCE_ATTR.name, top.confidence.to_string());
            if let Some(name) = &top.class_name {
                transformed = transformed.with_attribute(CLASS_TOP1_NAME_ATTR.name, name.clone());
            }
        }

        if let Some((key, value)) = extra_attribute {
            transformed = transformed.with_attribute(key, value);
        }
        Ok(transformed)
    }
}

impl FlowFileTransform for ClassifyOutput {
    fn transform<'a, Context: GetProperty + GetAttribute + GetId, LoggerImpl: Logger>(
        &self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, ProcessError> {
        let tensors = deserialize_tensors(context, input_stream).route_err_to_failure()?;
        self.classify(context, logger, tensors)
    }
}

#[cfg(test)]
mod tests {
    use super::classify_output_def::FAILURE;
    use super::*;
    use minifi_native::{LogLevel, MockLogger, MockProcessContext};
    use std::io::Cursor;
    use std::io::Write;
    use tempfile::NamedTempFile;

    fn make_processor(top_k: usize, activation: ScoreActivation) -> ClassifyOutput {
        ClassifyOutput {
            top_k,
            score_output_index: 0,
            score_activation: activation,
            confidence_threshold: 0.0,
            labels: None,
        }
    }

    #[test]
    fn test_top_k_descending_and_clamped() {
        let scored = vec![(0, 0.1), (1, 0.9), (2, 0.5), (3, 0.3)];
        assert_eq!(top_k(scored.clone(), 2), vec![(1, 0.9), (2, 0.5)]);
        assert_eq!(
            top_k(scored, 10),
            vec![(1, 0.9), (2, 0.5), (3, 0.3), (0, 0.1)]
        );
    }

    #[test]
    fn test_top_k_tiebreak_by_lower_index() {
        let scored = vec![(0, 0.5), (1, 0.5), (2, 0.5)];
        assert_eq!(top_k(scored, 3), vec![(0, 0.5), (1, 0.5), (2, 0.5)]);
    }

    fn build_payload(scores: &[f32]) -> Vec<u8> {
        let mut bytes = Vec::with_capacity(scores.len() * 4);
        for s in scores {
            bytes.extend_from_slice(&s.to_le_bytes());
        }
        bytes
    }

    fn context_with_scores(scores: &[f32]) -> MockProcessContext {
        let mut ctx = MockProcessContext::new();
        ctx.attributes.insert("tensors.len".into(), "1".into());
        ctx.attributes
            .insert("tensor.0.bytes".to_string(), (scores.len() * 4).to_string());
        ctx.attributes
            .insert("tensor.0.shape".to_string(), format!("1,{}", scores.len()));
        ctx.attributes
            .insert("tensor.0.dtype".to_string(), "F32".to_string());
        ctx
    }

    #[test]
    fn test_transform_returns_top_k_json() {
        let processor = make_processor(3, ScoreActivation::Softmax);
        let logits = vec![1.0f32, 4.0, 2.0, 0.5, 3.0];
        let context = context_with_scores(&logits);
        let payload = build_payload(&logits);
        let mut stream = Cursor::new(payload);
        let result = processor
            .transform(&context, &mut stream, &MockLogger::new())
            .expect("transform succeeds");

        assert_eq!(result.target_relationship(), SUCCESS.name);
        assert_eq!(result.attribute("class.top1.id").unwrap(), "1");
        assert_eq!(result.attribute("class.count").unwrap(), "3");

        let json_bytes = result.into_bytes().unwrap().unwrap();
        let json = String::from_utf8(json_bytes).unwrap();
        // Expect three entries, ranked class_id 1, then 4, then 2.
        assert!(json.contains("\"class_id\":1"));
        assert!(json.contains("\"class_id\":4"));
        assert!(json.contains("\"class_id\":2"));
    }

    #[test]
    fn test_sigmoid_activation_scores_classes_independently() {
        // Sigmoid squashes every logit on its own, so the confidences are not a
        // distribution: here they sum to ~1.5. A softmax regression would force
        // that sum to exactly 1.0.
        let processor = make_processor(3, ScoreActivation::Sigmoid);
        let logits = vec![0.0f32, 2.0, -2.0];
        let context = context_with_scores(&logits);
        let mut stream = Cursor::new(build_payload(&logits));
        let result = processor
            .transform(&context, &mut stream, &MockLogger::new())
            .expect("transform succeeds");

        let json = result.into_bytes().unwrap().unwrap();
        let predictions: Vec<serde_json::Value> = serde_json::from_slice(&json).unwrap();
        assert_eq!(predictions.len(), 3);

        let ids: Vec<u64> = predictions
            .iter()
            .map(|p| p["class_id"].as_u64().unwrap())
            .collect();
        assert_eq!(
            ids,
            vec![1, 0, 2],
            "sigmoid is monotonic, so the ranking follows the raw logits"
        );

        let confidences: Vec<f32> = predictions
            .iter()
            .map(|p| p["confidence"].as_f64().unwrap() as f32)
            .collect();
        let sigmoid_of = |logit: f32| 1.0f32 / (1.0 + (-logit).exp());
        assert_eq!(confidences[0], sigmoid_of(2.0));
        assert_eq!(confidences[1], 0.5, "sigmoid(0.0) is exactly one half");
        assert_eq!(confidences[2], sigmoid_of(-2.0));

        let sum: f32 = confidences.iter().sum();
        assert!(
            sum > 1.4,
            "per-class sigmoid must not normalise across classes, got {sum}"
        );
    }

    #[test]
    fn test_sigmoid_activation_threshold_is_inclusive_at_one_half() {
        // The threshold is applied after top_k, so top_k = 3 admits all three
        // candidates and only the filter decides. sigmoid(0.0) == 0.5 passes the
        // `>=` comparison; the negative logit falls below it.
        let mut processor = make_processor(3, ScoreActivation::Sigmoid);
        processor.confidence_threshold = 0.5;
        let logits = vec![2.0f32, -0.5, 0.0];
        let context = context_with_scores(&logits);
        let mut stream = Cursor::new(build_payload(&logits));
        let result = processor
            .transform(&context, &mut stream, &MockLogger::new())
            .unwrap();

        assert_eq!(
            result.attribute("class.count").unwrap(),
            "2",
            "only the -0.5 logit should be filtered out"
        );
        assert_eq!(result.attribute("class.top1.id").unwrap(), "0");
    }

    #[test]
    fn test_transform_omits_class_name_when_labels_absent() {
        let processor = make_processor(1, ScoreActivation::None);
        let scores = vec![0.1f32, 0.9];
        let context = context_with_scores(&scores);
        let mut stream = Cursor::new(build_payload(&scores));
        let result = processor
            .transform(&context, &mut stream, &MockLogger::new())
            .unwrap();
        // Snapshot the top1.name absence before into_bytes consumes `result`.
        let has_top1_name = result.attribute("class.top1.name").is_some();
        let json = String::from_utf8(result.into_bytes().unwrap().unwrap()).unwrap();
        assert!(!json.contains("class_name"));
        assert!(!has_top1_name);
    }

    #[test]
    fn test_transform_looks_up_labels() {
        let mut processor = make_processor(1, ScoreActivation::None);
        processor.labels = Some(vec![
            "tench".into(),
            "goldfish".into(),
            "great_white_shark".into(),
        ]);
        let scores = vec![0.1f32, 0.9, 0.5];
        let context = context_with_scores(&scores);
        let mut stream = Cursor::new(build_payload(&scores));
        let result = processor
            .transform(&context, &mut stream, &MockLogger::new())
            .unwrap();
        assert_eq!(result.attribute("class.top1.name").unwrap(), "goldfish");
        let json = String::from_utf8(result.into_bytes().unwrap().unwrap()).unwrap();
        assert!(json.contains("\"class_name\":\"goldfish\""));
    }

    #[test]
    fn test_label_offset() {
        let mut file = NamedTempFile::new().expect("Failed to create temp file");
        writeln!(file, "dummy").unwrap();
        writeln!(file, "tench").unwrap();
        writeln!(file, "goldfish").unwrap();
        writeln!(file, "great_white_shark").unwrap();
        let mut mock_context = MockProcessContext::default();
        mock_context.properties.insert(
            LABELS_FILE_PATH.name().to_string(),
            file.path().to_string_lossy(),
        );
        mock_context
            .properties
            .insert(LABEL_INDEX_OFFSET.name().to_string(), "1");
        let scheduled = ClassifyOutput::schedule(&mock_context, &MockLogger::new()).unwrap();
        assert_eq!(3, scheduled.labels.unwrap().len());
    }

    #[test]
    fn test_label_lookup_miss_warns_when_labels_are_configured() {
        let mut processor = make_processor(1, ScoreActivation::None);
        processor.labels = Some(vec!["abc".into(), "tench".into()]);
        let scores = vec![0.1f32, 0.2, 0.9]; // model class 2 wins
        let context = context_with_scores(&scores);
        let mut stream = Cursor::new(build_payload(&scores));
        let logger = MockLogger::new();

        let result = processor.transform(&context, &mut stream, &logger).unwrap();

        assert!(
            result.attribute("class.top1.name").is_none(),
            "an out-of-range label index still yields no name"
        );
        let logs = logger.logs.lock().unwrap();
        assert!(
            logs.iter()
                .any(|(level, msg)| *level == LogLevel::Warn && msg.contains("No label for class")),
            "expected a warning about the labels/model mismatch, got: {logs:?}"
        );
    }

    #[test]
    fn test_label_lookup_stays_silent_without_a_labels_file() {
        let processor = make_processor(1, ScoreActivation::None);
        let scores = vec![0.1f32, 0.9, 0.5];
        let context = context_with_scores(&scores);
        let mut stream = Cursor::new(build_payload(&scores));
        let logger = MockLogger::new();

        processor.transform(&context, &mut stream, &logger).unwrap();

        let logs = logger.logs.lock().unwrap();
        assert!(
            !logs
                .iter()
                .any(|(_, msg)| msg.contains("No label for class")),
            "should not warn when no labels file is configured, got: {logs:?}"
        );
    }

    #[test]
    fn test_transform_filters_below_confidence_threshold() {
        let mut processor = make_processor(3, ScoreActivation::None);
        processor.confidence_threshold = 0.6;
        let scores = vec![0.1f32, 0.9, 0.5];
        let context = context_with_scores(&scores);
        let mut stream = Cursor::new(build_payload(&scores));
        let result = processor
            .transform(&context, &mut stream, &MockLogger::new())
            .unwrap();
        assert_eq!(
            result.attribute("class.count").unwrap(),
            "1",
            "only the 0.9-scored class should survive"
        );
    }

    #[test]
    fn test_transform_ignores_non_finite_scores() {
        // A NaN score must not steal the top slot: with top_k = 1 the sole
        // prediction should be the finite class 1, not the NaN class 0.
        let processor = make_processor(1, ScoreActivation::None);
        let scores = vec![f32::NAN, 0.9];
        let context = context_with_scores(&scores);
        let mut stream = Cursor::new(build_payload(&scores));
        let result = processor
            .transform(&context, &mut stream, &MockLogger::new())
            .unwrap();
        assert_eq!(result.attribute("class.count").unwrap(), "1");
        assert_eq!(result.attribute("class.top1.id").unwrap(), "1");
    }

    #[test]
    fn test_transform_batched_scores_route_to_failure() {
        // A real batch dimension ([2, 3]) can't be flattened into one score
        // vector without mixing rows. It's input-dependent, so it must route to
        // failure (not raise a fatal/rollback error).
        let processor = make_processor(1, ScoreActivation::None);
        let scores = vec![0.1f32, 0.9, 0.2, 0.8, 0.3, 0.7];
        let mut context = context_with_scores(&scores);
        context
            .attributes
            .insert("tensor.0.shape".into(), "2,3".into());
        let mut stream = Cursor::new(build_payload(&scores));
        let err = processor
            .transform(&context, &mut stream, &MockLogger::new())
            .expect_err("batched scores should be rejected");
        match err {
            ProcessError::Route(route) => assert_eq!(route.relationship, FAILURE.name),
            other => panic!("expected route to failure, got {other:?}"),
        }
    }

    #[test]
    fn test_transform_missing_bytes_attribute_routes_to_failure() {
        let processor = make_processor(1, ScoreActivation::Softmax);
        let context = MockProcessContext::new(); // no tensor.0.bytes
        let mut stream = Cursor::new(vec![0u8; 4]);
        let err = processor
            .transform(&context, &mut stream, &MockLogger::new())
            .expect_err("missing attribute should route to failure via a Route error");
        match err {
            ProcessError::Route(route) => {
                assert_eq!(route.relationship, FAILURE.name)
            }
            other => panic!("expected route to failure, got {other:?}"),
        }
    }
}
