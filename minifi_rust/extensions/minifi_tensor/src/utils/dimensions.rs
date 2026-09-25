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

use minifi_native::{GetAttribute, MinifiError};

/// The exact placement of an aspect-preserving resize inside a target canvas.
///
/// `ImageToTensor` applies this when resizing, and `FilterBoundingBoxes` inverts
/// it when un-mapping model coordinates back to the original image. Both must
/// agree down to the pixel, so the arithmetic lives here and nowhere else:
/// deriving the padding from the *unrounded* scaled size instead of `new_w`/
/// `new_h` drifts by up to half a target pixel, which is several pixels once
/// divided back through `scale`.
#[derive(Debug, Clone, Copy, PartialEq)]
pub(crate) struct LetterboxGeometry {
    pub(crate) scale: f32,
    pub(crate) new_width: u32,
    pub(crate) new_height: u32,
    pub(crate) pad_x: u32,
    pub(crate) pad_y: u32,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub(crate) struct Dimensions {
    pub(crate) width: f32,
    pub(crate) height: f32,
}

impl Dimensions {
    /// Fit `self` into `target` preserving aspect ratio, centring the result.
    ///
    /// Assumes both dimensions are non-zero; `ImageToTensor::schedule` rejects a
    /// zero 'Target width'/'Target height', and a decoded image always has at
    /// least one pixel per axis.
    pub(crate) fn letterbox_into(&self, target: Dimensions) -> LetterboxGeometry {
        let scale = (target.width / self.width).min(target.height / self.height);
        let new_width = (self.width * scale).round().max(1.0) as u32;
        let new_height = (self.height * scale).round().max(1.0) as u32;
        LetterboxGeometry {
            scale,
            new_width,
            new_height,
            // Saturating: `new_*` is clamped up to 1, so it can exceed a target
            // axis of 0. Callers reject that config, but wrapping here would
            // turn a misconfiguration into a panic or a garbage offset.
            pad_x: (target.width as u32).saturating_sub(new_width) / 2,
            pad_y: (target.height as u32).saturating_sub(new_height) / 2,
        }
    }

    pub(crate) fn from_image(img: &image::DynamicImage) -> Self {
        Self {
            width: img.width() as f32,
            height: img.height() as f32,
        }
    }

    pub(crate) fn original_from_attributes<Context: GetAttribute>(
        context: &Context,
    ) -> Result<Dimensions, MinifiError> {
        let orig_w = context
            .get_required_attribute("image.original.width")?
            .parse::<f32>()?;

        let orig_h = context
            .get_required_attribute("image.original.height")?
            .parse::<f32>()?;

        Ok(Dimensions {
            width: orig_w,
            height: orig_h,
        })
    }

    pub(crate) fn target_from_attributes<Context: GetAttribute>(
        context: &Context,
    ) -> Result<Dimensions, MinifiError> {
        let orig_w = context
            .get_required_attribute("image.target.width")?
            .parse::<f32>()?;

        let orig_h = context
            .get_required_attribute("image.target.height")?
            .parse::<f32>()?;

        Ok(Dimensions {
            width: orig_w,
            height: orig_h,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn dim(width: f32, height: f32) -> Dimensions {
        Dimensions { width, height }
    }

    #[test]
    fn letterbox_pads_from_the_rounded_size() {
        // 1080p into SSD300: 1080 * 0.15625 = 168.75 rounds to 169, so the pad is
        // (300 - 169) / 2 = 65 — not the 65.625 the unrounded size would give.
        let geometry = dim(1920.0, 1080.0).letterbox_into(dim(300.0, 300.0));
        assert_eq!(geometry.scale, 0.15625);
        assert_eq!(geometry.new_width, 300);
        assert_eq!(geometry.new_height, 169);
        assert_eq!(geometry.pad_x, 0);
        assert_eq!(geometry.pad_y, 65);
    }

    #[test]
    fn letterbox_is_exact_when_the_scaled_size_is_integral() {
        let geometry = dim(200.0, 100.0).letterbox_into(dim(100.0, 100.0));
        assert_eq!(geometry.scale, 0.5);
        assert_eq!(geometry.new_width, 100);
        assert_eq!(geometry.new_height, 50);
        assert_eq!(geometry.pad_x, 0);
        assert_eq!(geometry.pad_y, 25);
    }

    #[test]
    fn letterbox_keeps_a_degenerate_axis_at_one_pixel() {
        // A very wide source against a small target rounds the short axis to 0;
        // it is clamped to 1 so the resize stays valid.
        let geometry = dim(1000.0, 3.0).letterbox_into(dim(10.0, 10.0));
        assert_eq!(geometry.new_width, 10);
        assert_eq!(geometry.new_height, 1);
        assert_eq!(geometry.pad_y, 4);
    }
}
