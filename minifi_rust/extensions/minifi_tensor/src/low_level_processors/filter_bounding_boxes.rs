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

mod filter_bounding_boxes_def;

use crate::low_level_processors::image_to_tensor::ResizeMode;
use crate::utils::bounding_box::BoundingBox;
use crate::utils::dimensions::Dimensions;
use crate::utils::score_activation::ScoreActivation;
use crate::utils::tensor_helpers::{deserialize_tensors, tensor_as_f32};
use filter_bounding_boxes_def::SUCCESS;
pub(crate) use filter_bounding_boxes_def::{
    BACKGROUND_CLASS_INDEX, BOX_FORMAT, BOX_OUTPUT_INDEX, CLASS_OUTPUT_INDEX, CONFIDENCE_THRESHOLD,
    IOU_THRESHOLD, OUTPUT_ATTRIBUTE_NAME, SCORE_ACTIVATION, SCORE_OUTPUT_INDEX,
};
use minifi_native::macros::{ComponentIdentifier, PropertyType};
use minifi_native::{
    Content, FlowFileTransform, GetAttribute, GetId, GetProperty, InputStream, Logger, MinifiError,
    ProcessError, RouteErrorExt, Schedule, TransformedFlowFile, debug, trace,
};
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};
use tract::Tensor;

#[derive(
    Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr, PropertyType,
)]
#[strum(serialize_all = "PascalCase", const_into_str)]
pub(crate) enum BoxFormat {
    /// `[x_min, y_min, x_max, y_max]` — SSD, MobileNet-SSD, most PyTorch models.
    Xyxy,
    /// `[y_min, x_min, y_max, x_max]` — TensorFlow Object Detection API.
    Yxyx,
    /// `[cx, cy, w, h]` — YOLOv3/5/8 raw output (center + size).
    Cxcywh,
}

/// Convert the four floats at `box_floats[offset..offset+4]` into a canonical
/// `(x_min, y_min, x_max, y_max)` tuple, regardless of the source layout.
fn decode_box(box_floats: &[f32], offset: usize, format: BoxFormat) -> (f32, f32, f32, f32) {
    let a = box_floats[offset];
    let b = box_floats[offset + 1];
    let c = box_floats[offset + 2];
    let d = box_floats[offset + 3];
    match format {
        BoxFormat::Xyxy => (a, b, c, d),
        BoxFormat::Yxyx => (b, a, d, c),
        BoxFormat::Cxcywh => {
            let (cx, cy, w, h) = (a, b, c, d);
            (cx - w / 2.0, cy - h / 2.0, cx + w / 2.0, cy + h / 2.0)
        }
    }
}

struct ScoredClass {
    class_id: usize,
    confidence: f32,
}

fn score_box(
    logits: &[f32],
    activation: ScoreActivation,
    background_class_index: Option<usize>,
) -> ScoredClass {
    let num_classes = logits.len();

    let best_valid = logits
        .iter()
        .enumerate()
        .filter(|&(_, &logit)| logit.is_finite())
        .filter(|&(id, _)| match background_class_index {
            Some(bg_idx) => !(num_classes > 1 && id == bg_idx),
            None => true,
        })
        .max_by(|a, b| a.1.total_cmp(b.1));

    let (class_id, &best_logit) = match best_valid {
        Some(val) => val,
        None => {
            return ScoredClass {
                class_id: 0,
                confidence: f32::NEG_INFINITY,
            };
        }
    };

    let confidence = match activation {
        ScoreActivation::Softmax => {
            let max_logit = logits
                .iter()
                .copied()
                .filter(|l| l.is_finite())
                .reduce(f32::max)
                .unwrap_or(f32::NEG_INFINITY);
            let sum_exp: f32 = logits
                .iter()
                .filter(|l| l.is_finite())
                .map(|&l| (l - max_logit).exp())
                .sum();

            (best_logit - max_logit).exp() / sum_exp
        }
        ScoreActivation::Sigmoid => 1.0 / (1.0 + (-best_logit).exp()),
        ScoreActivation::None => best_logit,
    };

    ScoredClass {
        class_id,
        confidence,
    }
}

/// Turn a single per-box score into a confidence for the "separate class-id
/// tensor" path.
/// Sigmoid maps a raw logit to a probability;
/// Softmax has no meaning over a single scalar, thus pass-through.
/// None passes the score through.
fn activate_scalar(score: f32, activation: ScoreActivation) -> f32 {
    match activation {
        ScoreActivation::Sigmoid => 1.0 / (1.0 + (-score).exp()),
        ScoreActivation::Softmax | ScoreActivation::None => score,
    }
}

#[derive(ComponentIdentifier)]
pub(crate) struct FilterBoundingBoxes {
    confidence_threshold: f32,
    iou_threshold: f32,
    score_output_index: usize,
    box_output_index: usize,
    box_format: BoxFormat,
    score_activation: ScoreActivation,
    background_class_index: Option<usize>,
    class_output_index: Option<usize>,
}

impl Schedule for FilterBoundingBoxes {
    fn schedule<Ctx: GetProperty, L: Logger>(
        context: &Ctx,
        _logger: &L,
    ) -> Result<Self, MinifiError> {
        let confidence_threshold = context.get_property(&CONFIDENCE_THRESHOLD)?;
        let iou_threshold = context.get_property(&IOU_THRESHOLD)?;
        let score_output_index = context.get_property(&SCORE_OUTPUT_INDEX)?;
        let box_output_index = context.get_property(&BOX_OUTPUT_INDEX)?;
        let box_format = context.get_property(&BOX_FORMAT)?;
        let score_activation = context.get_property(&SCORE_ACTIVATION)?;
        let background_class_index = context.get_property(&BACKGROUND_CLASS_INDEX)?;
        let class_output_index = context.get_property(&CLASS_OUTPUT_INDEX)?;

        Ok(Self {
            confidence_threshold,
            iou_threshold,
            score_output_index,
            box_output_index,
            box_format,
            score_activation,
            background_class_index,
            class_output_index,
        })
    }
}

impl FilterBoundingBoxes {
    fn result_via_output_attribute<'a, Context: GetProperty>(
        &self,
        context: &Context,
        filtered_boxes: Vec<BoundingBox>,
    ) -> Result<TransformedFlowFile<'a>, MinifiError> {
        let output_attr = context.get_property(&OUTPUT_ATTRIBUTE_NAME)?;
        let content = if output_attr.is_some() {
            None
        } else {
            Some(Content::Buffer(
                serde_json::to_vec(&filtered_boxes).map_err(MinifiError::other)?,
            ))
        };

        let mut transformed = TransformedFlowFile::new(&SUCCESS, content)
            .with_attribute("object.count", filtered_boxes.len().to_string());
        if let Some(attr) = output_attr {
            transformed = transformed.with_attribute(
                attr,
                serde_json::to_string(&filtered_boxes).map_err(MinifiError::other)?,
            )
        } else {
            transformed = transformed.with_attribute("mime.type", "application/json");
        }
        Ok(transformed)
    }

    pub(crate) fn filter<'a, Context: GetProperty, LoggerImpl: Logger>(
        &self,
        context: &Context,
        logger: &LoggerImpl,
        tensors: Vec<Tensor>,
        orig_dim: Dimensions,
        target_dim: Dimensions,
        resize_mode: ResizeMode,
    ) -> Result<TransformedFlowFile<'a>, ProcessError> {
        let score_floats =
            tensor_as_f32(&tensors, self.score_output_index).route_err_to_failure()?;
        let box_floats = tensor_as_f32(&tensors, self.box_output_index).route_err_to_failure()?;

        let (scale_x, scale_y, pad_x, pad_y) = match resize_mode {
            ResizeMode::Letterbox => {
                let scale =
                    (target_dim.width / orig_dim.width).min(target_dim.height / orig_dim.height);
                let pad_x = (target_dim.width - (orig_dim.width * scale)) / 2.0;
                let pad_y = (target_dim.height - (orig_dim.height * scale)) / 2.0;
                (scale, scale, pad_x, pad_y)
            }
            ResizeMode::Stretch => (
                target_dim.width / orig_dim.width,
                target_dim.height / orig_dim.height,
                0.0,
                0.0,
            ),
        };

        if !box_floats.len().is_multiple_of(4) {
            return Err(MinifiError::custom(
                "Box tensor byte length is not a multiple of 16 (4 f32 per box)",
            )
            .into());
        }
        let num_boxes = box_floats.len() / 4;
        if num_boxes == 0 {
            debug!(logger, "No boxes to filter; emitting empty array");
            return self
                .result_via_output_attribute(context, vec![])
                .route_err_to_failure();
        }

        let make_box = |i: usize, class_id: usize, confidence: f32| -> BoundingBox {
            let (raw_x_min, raw_y_min, raw_x_max, raw_y_max) =
                decode_box(&box_floats, i * 4, self.box_format);
            let true_x_min = (((raw_x_min * target_dim.width) - pad_x) / scale_x) / orig_dim.width;
            let true_y_min =
                (((raw_y_min * target_dim.height) - pad_y) / scale_y) / orig_dim.height;
            let true_x_max = (((raw_x_max * target_dim.width) - pad_x) / scale_x) / orig_dim.width;
            let true_y_max =
                (((raw_y_max * target_dim.height) - pad_y) / scale_y) / orig_dim.height;
            BoundingBox {
                class_id,
                confidence,
                x_min: true_x_min.clamp(0.0, 1.0),
                y_min: true_y_min.clamp(0.0, 1.0),
                x_max: true_x_max.clamp(0.0, 1.0),
                y_max: true_y_max.clamp(0.0, 1.0),
            }
        };

        let mut valid_boxes = Vec::new();

        match self.class_output_index {
            // Separate class-id tensor: one score and one class id per box
            Some(class_index) => {
                let class_floats = tensor_as_f32(&tensors, class_index).route_err_to_failure()?;
                if score_floats.len() != num_boxes || class_floats.len() != num_boxes {
                    return Err(MinifiError::custom(format!(
                        "'Class output index' mode expects one score and one class id per box \
                         (num_boxes={}, scores={}, classes={})",
                        num_boxes,
                        score_floats.len(),
                        class_floats.len()
                    ))
                    .into());
                }
                trace!(
                    logger,
                    "Filtering {} boxes with separate class-id tensor (activation={:?}, \
                     box_format={:?})...",
                    num_boxes,
                    self.score_activation,
                    self.box_format
                );
                for i in 0..num_boxes {
                    let confidence = activate_scalar(score_floats[i], self.score_activation);
                    if confidence < self.confidence_threshold {
                        continue;
                    }
                    let raw_class = class_floats[i];
                    if raw_class < 0.0 {
                        continue;
                    }
                    if !confidence.is_finite() || !raw_class.is_finite() {
                        continue;
                    }
                    let class_id = raw_class.round() as usize;
                    if self.background_class_index == Some(class_id) {
                        continue;
                    }
                    valid_boxes.push(make_box(i, class_id, confidence));
                }
            }
            // Per-class score matrix: argmax over classes per box.
            None => {
                if !score_floats.len().is_multiple_of(num_boxes) {
                    return Err(MinifiError::custom(format!(
                        "Scores length ({}) not divisible by number of boxes ({})",
                        score_floats.len(),
                        num_boxes
                    ))
                    .into());
                }
                let num_classes = score_floats.len() / num_boxes;
                trace!(
                    logger,
                    "Filtering {} boxes across {} potential classes (activation={:?}, \
                     box_format={:?})...",
                    num_boxes,
                    num_classes,
                    self.score_activation,
                    self.box_format
                );
                for i in 0..num_boxes {
                    let logits = &score_floats[i * num_classes..(i + 1) * num_classes];
                    let scored =
                        score_box(logits, self.score_activation, self.background_class_index);
                    if scored.confidence >= self.confidence_threshold {
                        valid_boxes.push(make_box(i, scored.class_id, scored.confidence));
                    }
                }
            }
        }

        trace!(
            logger,
            "Found {} boxes exceeding the {} threshold.",
            valid_boxes.len(),
            self.confidence_threshold
        );

        let filtered_boxes =
            BoundingBox::apply_non_maximum_suppression(valid_boxes, self.iou_threshold);

        self.result_via_output_attribute(context, filtered_boxes)
            .route_err_to_failure()
    }
}

fn resize_mode_from_attributes<Context: GetAttribute>(context: &Context) -> ResizeMode {
    context
        .get_attribute("image.resize.mode")
        .ok()
        .flatten()
        .and_then(|raw| raw.parse::<ResizeMode>().ok())
        .unwrap_or(ResizeMode::Letterbox)
}

impl FlowFileTransform for FilterBoundingBoxes {
    fn transform<'a, Context: GetProperty + GetAttribute + GetId, LoggerImpl: Logger>(
        &self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, ProcessError> {
        let orig_dim = Dimensions::original_from_attributes(context).route_err_to_failure()?;
        let target_dim = Dimensions::target_from_attributes(context).route_err_to_failure()?;
        let resize_mode = resize_mode_from_attributes(context);

        let tensors = deserialize_tensors(context, input_stream).route_err_to_failure()?;

        self.filter(context, logger, tensors, orig_dim, target_dim, resize_mode)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_decode_box_xyxy_is_identity() {
        let raw = [10.0, 20.0, 30.0, 40.0];
        let (x0, y0, x1, y1) = decode_box(&raw, 0, BoxFormat::Xyxy);
        assert_eq!((x0, y0, x1, y1), (10.0, 20.0, 30.0, 40.0));
    }

    #[test]
    fn test_decode_box_yxyx_swaps_axes() {
        let raw = [20.0, 10.0, 40.0, 30.0]; // (y_min, x_min, y_max, x_max)
        let (x0, y0, x1, y1) = decode_box(&raw, 0, BoxFormat::Yxyx);
        assert_eq!((x0, y0, x1, y1), (10.0, 20.0, 30.0, 40.0));
    }

    #[test]
    fn test_decode_box_cxcywh_converts_to_corners() {
        // Center (100, 200), width 20, height 40 → corners (90,180)-(110,220)
        let raw = [100.0, 200.0, 20.0, 40.0];
        let (x0, y0, x1, y1) = decode_box(&raw, 0, BoxFormat::Cxcywh);
        assert!((x0 - 90.0).abs() < 1e-6);
        assert!((y0 - 180.0).abs() < 1e-6);
        assert!((x1 - 110.0).abs() < 1e-6);
        assert!((y1 - 220.0).abs() < 1e-6);
    }

    #[test]
    fn test_score_box_softmax_picks_argmax_excluding_background() {
        // 3 classes: bg strongly favored, but bg is class 0 which we skip.
        let logits = [5.0, 0.5, 2.0];
        let scored = score_box(&logits, ScoreActivation::Softmax, Some(0));
        assert_eq!(scored.class_id, 2);
        assert!(scored.confidence > 0.0 && scored.confidence < 1.0);
    }

    #[test]
    fn test_score_box_sigmoid_treats_classes_independently() {
        // Sigmoid(2.0) ≈ 0.881, so class 1 wins over class 0 regardless of
        // relative magnitudes.
        let logits = [-3.0, 2.0];
        let scored = score_box(&logits, ScoreActivation::Sigmoid, None);
        assert_eq!(scored.class_id, 1);
        assert!((scored.confidence - 0.8807).abs() < 1e-3);
    }

    #[test]
    fn test_score_box_none_takes_raw_argmax() {
        let logits = [0.1, 0.7, 0.2];
        let scored = score_box(&logits, ScoreActivation::None, None);
        assert_eq!(scored.class_id, 1);
        assert!((scored.confidence - 0.7).abs() < 1e-6);
    }

    #[test]
    fn test_score_box_background_disabled_keeps_class_zero() {
        let logits = [5.0, 0.5];
        let scored = score_box(&logits, ScoreActivation::Softmax, None);
        assert_eq!(scored.class_id, 0);
    }

    #[test]
    fn test_activate_scalar_sigmoid_and_passthrough() {
        assert!((activate_scalar(0.0, ScoreActivation::Sigmoid) - 0.5).abs() < 1e-6);
        assert_eq!(activate_scalar(0.42, ScoreActivation::None), 0.42);
        // Softmax over a scalar has no meaning → pass-through.
        assert_eq!(activate_scalar(0.42, ScoreActivation::Softmax), 0.42);
    }

    fn class_index_processor() -> FilterBoundingBoxes {
        FilterBoundingBoxes {
            confidence_threshold: 0.5,
            iou_threshold: 0.45,
            score_output_index: 0,
            box_output_index: 1,
            box_format: BoxFormat::Xyxy,
            score_activation: ScoreActivation::None,
            background_class_index: None,
            class_output_index: Some(2),
        }
    }

    /// TF-OD / EfficientNMS style: separate scores, boxes, and (integer) class-id
    /// tensors, one entry per box, NMS already applied by the model.
    #[test]
    fn test_filter_with_separate_class_index_tensor() {
        use minifi_native::{MockLogger, MockProcessContext};
        use tract::__ndarray_interop::TensorInterface;

        let scores = Tensor::from_slice::<f32>(&[2], &[0.9, 0.2]).unwrap();
        let boxes =
            Tensor::from_slice::<f32>(&[2, 4], &[0.1, 0.1, 0.2, 0.2, 0.5, 0.5, 0.9, 0.9]).unwrap();
        // class ids arrive as i64, exercising the cast path too.
        let classes = Tensor::from_slice::<i64>(&[2], &[5, 3]).unwrap();

        let processor = class_index_processor();
        let dim = Dimensions {
            width: 100.0,
            height: 100.0,
        };
        let result = processor
            .filter(
                &MockProcessContext::new(),
                &MockLogger::new(),
                vec![scores, boxes, classes],
                dim,
                dim,
                ResizeMode::Letterbox,
            )
            .expect("filter should succeed");

        // Only box 0 (score 0.9) clears the 0.5 threshold; box 1 (0.2) is dropped.
        assert_eq!(result.attribute("object.count").unwrap(), "1");
        let json = String::from_utf8(result.into_bytes().unwrap().unwrap()).unwrap();
        assert!(json.contains("\"class_id\":5"));
        assert!(!json.contains("\"class_id\":3"));
    }

    #[test]
    fn test_resize_mode_changes_coordinate_un_mapping() {
        use minifi_native::{MockLogger, MockProcessContext};
        use tract::__ndarray_interop::TensorInterface;

        let run = |mode: ResizeMode| -> BoundingBox {
            // One interior box (avoids clamping) in normalised Xyxy.
            let scores = Tensor::from_slice::<f32>(&[1], &[0.9]).unwrap();
            let boxes = Tensor::from_slice::<f32>(&[1, 4], &[0.4, 0.4, 0.6, 0.6]).unwrap();
            let classes = Tensor::from_slice::<i64>(&[1], &[5]).unwrap();

            let result = class_index_processor()
                .filter(
                    &MockProcessContext::new(),
                    &MockLogger::new(),
                    vec![scores, boxes, classes],
                    Dimensions {
                        width: 200.0,
                        height: 100.0,
                    },
                    Dimensions {
                        width: 100.0,
                        height: 100.0,
                    },
                    mode,
                )
                .expect("filter should succeed");
            let json = result.into_bytes().unwrap().unwrap();
            let boxes: Vec<BoundingBox> = serde_json::from_slice(&json).unwrap();
            boxes.into_iter().next().expect("one box expected")
        };

        // Stretch: scale_x = 100/200, scale_y = 100/100, no padding → identity in
        // normalised space.
        let stretched = run(ResizeMode::Stretch);
        assert!((stretched.y_min - 0.4).abs() < 1e-5);
        assert!((stretched.y_max - 0.6).abs() < 1e-5);

        // Letterbox: uniform scale 0.5, symmetric vertical pad of 25px removed →
        // y expands to 0.3..0.7. x is unchanged (pad_x = 0, same scale on x).
        let letterboxed = run(ResizeMode::Letterbox);
        assert!((letterboxed.x_min - 0.4).abs() < 1e-5);
        assert!((letterboxed.x_max - 0.6).abs() < 1e-5);
        assert!((letterboxed.y_min - 0.3).abs() < 1e-5);
        assert!((letterboxed.y_max - 0.7).abs() < 1e-5);
    }

    #[test]
    fn test_filter_class_index_mode_rejects_mismatched_lengths() {
        use minifi_native::{MockLogger, MockProcessContext};
        use tract::__ndarray_interop::TensorInterface;

        // 2 boxes but only 1 score → the parallel-tensor contract is violated.
        let scores = Tensor::from_slice::<f32>(&[1], &[0.9]).unwrap();
        let boxes =
            Tensor::from_slice::<f32>(&[2, 4], &[0.1, 0.1, 0.2, 0.2, 0.5, 0.5, 0.9, 0.9]).unwrap();
        let classes = Tensor::from_slice::<i64>(&[2], &[5, 3]).unwrap();

        let dim = Dimensions {
            width: 100.0,
            height: 100.0,
        };
        let result = class_index_processor().filter(
            &MockProcessContext::new(),
            &MockLogger::new(),
            vec![scores, boxes, classes],
            dim,
            dim,
            ResizeMode::Letterbox,
        );
        assert!(result.is_err(), "mismatched score/box counts should error");
    }
}
