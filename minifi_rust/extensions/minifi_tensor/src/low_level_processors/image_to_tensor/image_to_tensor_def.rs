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

use super::{ColorFormat, ImageToTensor, ResizeFilter, ResizeMode, TensorShapeFormat};
use crate::utils::per_channel_f32::PerChannelF32;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, PropertyDefinition,
    Relationship, property_definitions,
};

pub(crate) const TARGET_WIDTH: Property<u32> = Property::new(
    "Target width",
    "Width in pixels the decoded image is resized to before normalisation and \
                  inference.",
);

pub(crate) const TARGET_HEIGHT: Property<u32> = Property::new(
    "Target height",
    "Height in pixels the decoded image is resized to before normalisation and \
                  inference.",
);

pub(crate) const RESIZE_FILTER: Property<ResizeFilter> = Property::new(
    "Resize filter",
    "Interpolation filter applied when resizing the decoded image. Nearest is fastest \
                  but blocky; Bilinear is a good default; Bicubic and Lanczos3 are higher-quality \
                  but slower.",
)
.with_default(ResizeFilter::Bilinear.into_str());

pub(crate) const RESIZE_MODE: Property<ResizeMode> = Property::new(
    "Resize mode",
    "How the source image is fitted into the target dimensions. 'Stretch' scales each \
                  axis independently, distorting aspect ratio. 'Letterbox' preserves aspect ratio \
                  and pads the remaining border with 'Letterbox pad value' (applied in normalised \
                  output space).",
)
.with_default(ResizeMode::Stretch.into_str());

pub(crate) const LETTERBOX_PAD_VALUE: Property<f32> = Property::new(
    "Letterbox pad value",
    "Value written for padding pixels when 'Resize mode' is 'Letterbox'. This is a \
                  normalised value (post mean/std), so 0.0 corresponds to a neutral input for most \
                  networks. Ignored when 'Resize mode' is 'Stretch'.",
)
.with_default("0.0");

pub(crate) const COLOR_FORMAT: Property<ColorFormat> = Property::new(
    "Color format",
    "Colour space of the tensor fed to the model. RGB and BGR produce three-channel \
                  tensors (channel order determined by the format); Grayscale produces a \
                  single-channel luma tensor.",
)
.with_default(ColorFormat::Rgb.into_str());

pub(crate) const TENSOR_SHAPE_FORMAT: Property<TensorShapeFormat> = Property::new(
    "Tensor shape format",
    "Memory layout of the tensor fed to the model. CHW (channels-first) is typical \
                  for PyTorch/ONNX detectors. HWC (channels-last) matches TensorFlow/TFLite. \
                  Ignored for Grayscale (always effectively 1xHxW).",
)
.with_default(TensorShapeFormat::Chw.into_str());

pub(crate) const MEAN: Property<PerChannelF32> = Property::new(
    "Mean",
    "Mean subtracted from each pixel before dividing by 'Standard Deviation'. Accepts \
                  either a single value (broadcast to all channels) or three comma-separated \
                  values applied per channel in the order dictated by 'Color format'. Example: \
                  '0.485, 0.456, 0.406' for ImageNet-style RGB normalisation.",
)
.with_default("0.0");

pub(crate) const STD_DEV: Property<PerChannelF32> = Property::new(
    "Standard Deviation",
    "Divisor applied after subtracting 'Mean'. Accepts a single value (broadcast) or \
                  three comma-separated values (per channel). Must be non-zero. Example: '255.0' \
                  to scale u8 pixels into [0.0, 1.0]; '0.229, 0.224, 0.225' for ImageNet.",
)
.with_default("255.0");

pub(crate) const PIXEL_DIVISOR: Property<f32> = Property::new(
    "Pixel divisor",
    "Divisor applied to raw u8 pixel values before subtracting 'Mean' and dividing \
                  by 'Standard Deviation'. Defaults to 1.0 (mean/std interpreted in [0, 255] pixel \
                  space, e.g. UltraFace's mean=127, std=128). Set to 255 to bring pixels into \
                  [0.0, 1.0] first so ImageNet-style mean/std values like '0.485, 0.456, 0.406' / \
                  '0.229, 0.224, 0.225' can be used directly, matching the PyTorch / torchvision / \
                  ONNX MobileNet convention. Must be non-zero.",
)
.with_default("1.0");

pub(super) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "The input image was decoded and converted to a tensor.",
};

pub(super) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "The input flow file could not be decoded as an image.",
};

pub(super) const TENSORS_LEN_ATTR: OutputAttribute = OutputAttribute {
    name: "tensors.len",
    relationships: &["success"],
    description: "Number of tensors in the output FlowFile. Currently always '1'",
};

pub(super) const TENSOR_SHAPE_ATTR: OutputAttribute = OutputAttribute {
    name: "tensor.0.shape",
    relationships: &["success"],
    description: "Comma-separated dimensions of the output tensor in the chosen layout, always \
                  including a leading batch dimension of 1 (e.g. '1,3,224,224' for RGB CHW).",
};

pub(super) const TENSOR_DTYPE_ATTR: OutputAttribute = OutputAttribute {
    name: "tensor.0.dtype",
    relationships: &["success"],
    description: "Element type of the values in the output tensor. Currently always 'F32'.",
};

pub(super) const IMG_ORG_HEIGHT_ATTR: OutputAttribute = OutputAttribute {
    name: "image.original.height",
    relationships: &["success"],
    description: "The height of the original image before the resizing.",
};

pub(super) const IMG_ORG_WIDTH_ATTR: OutputAttribute = OutputAttribute {
    name: "image.original.width",
    relationships: &["success"],
    description: "The width of the original image before the resizing.",
};

pub(super) const IMG_TRG_HEIGHT_ATTR: OutputAttribute = OutputAttribute {
    name: "image.target.height",
    relationships: &["success"],
    description: "The height of the image after the resizing.",
};

pub(super) const IMG_TRG_WIDTH_ATTR: OutputAttribute = OutputAttribute {
    name: "image.target.width",
    relationships: &["success"],
    description: "The width of the image after the resizing.",
};

impl ProcessorDefinition for ImageToTensor {
    const DESCRIPTION: &'static str = "Decodes an image from the flow file content and converts it into a normalised numeric \
         tensor suitable for feeding into a downstream inference processor such as \
         InvokeTractModel. Supports RGB / BGR / Grayscale, CHW / HWC layouts, stretch or \
         letterbox resizing, and scalar or per-channel mean/std normalisation. The output payload \
         is the raw little-endian f32 tensor; the 'tensor.shape' and 'tensor.dtype' attributes \
         describe its layout.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[
        TENSORS_LEN_ATTR,
        TENSOR_SHAPE_ATTR,
        TENSOR_DTYPE_ATTR,
        IMG_ORG_WIDTH_ATTR,
        IMG_ORG_HEIGHT_ATTR,
        IMG_TRG_WIDTH_ATTR,
        IMG_TRG_HEIGHT_ATTR,
    ];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];
    const PROPERTIES: &[PropertyDefinition] = property_definitions![
        TARGET_WIDTH,
        TARGET_HEIGHT,
        RESIZE_FILTER,
        RESIZE_MODE,
        LETTERBOX_PAD_VALUE,
        COLOR_FORMAT,
        TENSOR_SHAPE_FORMAT,
        MEAN,
        STD_DEV,
        PIXEL_DIVISOR,
    ];
}
