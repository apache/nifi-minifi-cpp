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

use crate::utils::score_activation::ScoreActivation;
use crate::utils::tensor_helpers::{deserialize_tensors, tensor_as_f32, tensor_shape};
use classify_output_def::SUCCESS;
pub(crate) use classify_output_def::{
    CONFIDENCE_THRESHOLD, LABEL_INDEX_OFFSET, LABELS_FILE_PATH, OUTPUT_ATTRIBUTE_NAME,
    SCORE_ACTIVATION, SCORE_OUTPUT_INDEX, TOP_K,
};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    Content, FlowFileTransform, GetAttribute, GetId, GetProperty, InputStream, Logger, MinifiError,
    ProcessError, RouteErrorExt, Schedule, TransformedFlowFile,
};
use serde::Serialize;
use std::path::Path;
use tract::Tensor;

mod classify_output_def;

#[derive(Serialize, Clone, Debug, PartialEq)]
struct Prediction {
    class_id: usize,
    confidence: f32,
    #[serde(skip_serializing_if = "Option::is_none")]
    class_name: Option<String>,
}

/// Load a newline-separated labels file at schedule time. Trailing whitespace
/// is stripped; blank lines are preserved as empty strings so line-number ↔
/// class-id alignment stays exact.
fn load_labels(path: &Path) -> Result<Vec<String>, MinifiError> {
    let content = std::fs::read_to_string(path).map_err(|e| {
        MinifiError::custom(format!("Failed to read labels file '{:?}': {}", path, e))
    })?;
    Ok(content
        .lines()
        .map(|line| line.trim_end().to_string())
        .collect())
}

/// Sort `(class_id, score)` pairs by score descending — ties broken by lower
/// class id — and keep the top `k`. Callers pass an already-finite set, so the
/// `total_cmp` here never has to rank a NaN. `k` is clamped to the input length.
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
    labels: Vec<String>,
    label_index_offset: usize,
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
        // Labels file is optional — load and cache when configured; empty vec
        // means "emit class_id only".
        let labels = match context.get_property(&LABELS_FILE_PATH)? {
            Some(path) => load_labels(&path)?,
            _ => Vec::new(),
        };
        let label_index_offset = context.get_property(&LABEL_INDEX_OFFSET)?;

        Ok(Self {
            top_k,
            score_output_index,
            score_activation,
            confidence_threshold,
            labels,
            label_index_offset,
        })
    }
}

impl ClassifyOutput {
    fn label_for(&self, class_id: usize) -> Option<String> {
        self.labels
            .get(class_id.checked_add(self.label_index_offset)?)
            .cloned()
    }

    pub(crate) fn classify<'a, Context: GetProperty + GetAttribute + GetId>(
        &self,
        context: &Context,
        tensors: Vec<Tensor>,
    ) -> Result<TransformedFlowFile<'a>, ProcessError> {
        let score_floats =
            tensor_as_f32(&tensors, self.score_output_index).route_err_to_failure()?;
        if score_floats.is_empty() {
            return Err(MinifiError::custom("Score tensor is empty; nothing to classify").into());
        }

        // A classifier head is a single score vector: shape [num_classes] or
        // [1, .., num_classes]. We rank over the flattened class axis, so any
        // leading axis > 1 (a real batch) would silently mix rows and yield
        // class ids past num_classes. Reject it rather than produce garbage.
        // (`ImageToTensor` emits batch=1 today; this just enforces the contract.)
        let shape = tensor_shape(&tensors, self.score_output_index).route_err_to_failure()?;
        if shape.iter().rev().skip(1).any(|&d| d != 1) {
            return Err(MinifiError::custom(format!(
                "ClassifyOutput expects a single score vector (shape [num_classes] or \
                 [1, .., num_classes]); got {shape:?}. A batch dimension > 1 is not supported."
            )))
            .route_err_to_failure();
        }

        let finite: Vec<(usize, f32)> = score_floats
            .iter()
            .copied()
            .enumerate()
            .filter(|&(_, s)| s.is_finite())
            .collect();

        let (max_logit, sum_exp) = match self.score_activation {
            ScoreActivation::Softmax => {
                let max = finite
                    .iter()
                    .map(|&(_, s)| s)
                    .reduce(f32::max)
                    .unwrap_or(f32::NEG_INFINITY);
                let sum = finite.iter().map(|&(_, s)| (s - max).exp()).sum::<f32>();
                (max, sum)
            }
            _ => (0.0, 1.0),
        };

        let predictions: Vec<Prediction> = top_k(finite, self.top_k)
            .into_iter()
            .filter_map(|(class_id, raw)| {
                let confidence = match self.score_activation {
                    ScoreActivation::Softmax => (raw - max_logit).exp() / sum_exp,
                    ScoreActivation::Sigmoid => 1.0 / (1.0 + (-raw).exp()),
                    ScoreActivation::None => raw,
                };

                if confidence >= self.confidence_threshold {
                    Some(Prediction {
                        class_id,
                        confidence,
                        class_name: self.label_for(class_id),
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
                Some((output_attr, serde_json::to_string(&predictions).unwrap())),
            ),
        };

        let mut transformed = TransformedFlowFile::new(&SUCCESS, content)
            .with_attribute("mime.type", "application/json")
            .with_attribute("class.count", predictions.len().to_string());

        if let Some(top) = predictions.first() {
            transformed = transformed
                .with_attribute("class.top1.id", top.class_id.to_string())
                .with_attribute("class.top1.confidence", top.confidence.to_string());
            if let Some(name) = &top.class_name {
                transformed = transformed.with_attribute("class.top1.name", name.clone());
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
        _logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, ProcessError> {
        let tensors = deserialize_tensors(context, input_stream).route_err_to_failure()?;
        self.classify(context, tensors)
    }
}

#[cfg(test)]
mod tests {
    use super::classify_output_def::FAILURE;
    use super::*;
    use minifi_native::{MockLogger, MockProcessContext};
    use std::io::Cursor;

    fn make_processor(top_k: usize, activation: ScoreActivation) -> ClassifyOutput {
        ClassifyOutput {
            top_k,
            score_output_index: 0,
            score_activation: activation,
            confidence_threshold: 0.0,
            labels: Vec::new(),
            label_index_offset: 0,
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
        processor.labels = vec![
            "tench".into(),
            "goldfish".into(),
            "great_white_shark".into(),
        ];
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
    fn test_label_index_offset_shifts_lookup() {
        // Mirrors the ImageNet slim labels layout: line 0 is a dummy entry, so
        // model class 0 should resolve to labels[1] = "tench".
        let mut processor = make_processor(1, ScoreActivation::None);
        processor.labels = vec![
            "dummy".into(),
            "tench".into(),
            "goldfish".into(),
            "great_white_shark".into(),
        ];
        processor.label_index_offset = 1;
        let scores = vec![0.9f32, 0.1, 0.0]; // model class 0 wins
        let context = context_with_scores(&scores);
        let mut stream = Cursor::new(build_payload(&scores));
        let result = processor
            .transform(&context, &mut stream, &MockLogger::new())
            .unwrap();
        assert_eq!(
            result.attribute("class.top1.name").unwrap(),
            "tench",
            "class id 0 with offset 1 should land on labels[1]"
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
            ProcessError::Route(route) => assert_eq!(route.relationship.as_ref(), FAILURE.name),
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
                assert_eq!(route.relationship.as_ref(), FAILURE.name)
            }
            other => panic!("expected route to failure, got {other:?}"),
        }
    }
}
