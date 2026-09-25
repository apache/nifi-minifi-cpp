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

use minifi_native::macros::PropertyType;
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};

#[derive(
    Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr, PropertyType,
)]
#[strum(serialize_all = "PascalCase", const_into_str)]
pub(crate) enum ScoreActivation {
    /// Cross-class softmax; classes are mutually exclusive (typical for
    /// ImageNet-trained ResNet/MobileNet/EfficientNet ONNX exports).
    Softmax,
    /// Per-class sigmoid; classes are independent (multi-label classifiers).
    Sigmoid,
    /// Pass-through — the model already emits probabilities.
    None,
}

/// The `(max_logit, sum_exp)` denominator of a numerically-stable softmax.
///
/// Subtracting the max before exponentiating keeps `exp` in range for large
/// logits. Non-finite logits are skipped so one NaN cannot poison the whole
/// distribution.
#[derive(Debug, Clone, Copy, PartialEq)]
pub(crate) struct SoftmaxTerms {
    max_logit: f32,
    sum_exp: f32,
}

impl SoftmaxTerms {
    pub(crate) fn over(logits: impl IntoIterator<Item = f32> + Clone) -> Self {
        let max_logit = logits
            .clone()
            .into_iter()
            .filter(|l| l.is_finite())
            .reduce(f32::max)
            .unwrap_or(f32::NEG_INFINITY);
        let sum_exp = logits
            .into_iter()
            .filter(|l| l.is_finite())
            .map(|l| (l - max_logit).exp())
            .sum();
        Self { max_logit, sum_exp }
    }
}

impl ScoreActivation {
    /// Confidence for one logit drawn from a full score vector.
    ///
    /// `terms` must be computed over that same vector, so `Softmax` normalises
    /// against the distribution the logit came from.
    pub(crate) fn confidence(self, logit: f32, terms: SoftmaxTerms) -> f32 {
        match self {
            ScoreActivation::Softmax => (logit - terms.max_logit).exp() / terms.sum_exp,
            ScoreActivation::Sigmoid => sigmoid(logit),
            ScoreActivation::None => logit,
        }
    }

    /// Confidence for a standalone score, with no surrounding vector to
    /// normalise against — the "separate class-id tensor" detector layout.
    ///
    /// Sigmoid maps a raw logit to a probability; softmax has no meaning over a
    /// single scalar, so it passes through, as does None.
    pub(crate) fn confidence_of_scalar(self, score: f32) -> f32 {
        match self {
            ScoreActivation::Sigmoid => sigmoid(score),
            ScoreActivation::Softmax | ScoreActivation::None => score,
        }
    }
}

fn sigmoid(x: f32) -> f32 {
    1.0 / (1.0 + (-x).exp())
}
