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

#[cfg(feature = "low-level-processors")]
use crate::low_level_processors::{
    classify_output::ClassifyOutput, filter_bounding_boxes::FilterBoundingBoxes,
    image_to_tensor::ImageToTensor, invoke_tract_model::InvokeTractModel,
};
use crate::processors::classify_image::ClassifyImage;
use crate::processors::detect_object::DetectObject;
use crate::processors::draw_bounding_box::DrawBoundingBox;

use crate::services::tract_model_service::TractModelService;
use minifi_native::{FlowFileTransformProcessorType, MultiThreaded};

mod low_level_processors;
mod processors;
mod services;
mod utils;

minifi_native::declare_minifi_extension!(
    group_name: "org.apache.nifi.minifi.rust",
    processors: [
        (FlowFileTransformProcessorType, MultiThreaded, DetectObject),
        (FlowFileTransformProcessorType, MultiThreaded, ClassifyImage),
        (FlowFileTransformProcessorType, MultiThreaded, DrawBoundingBox),
        #[cfg(feature = "low-level-processors")]
        (FlowFileTransformProcessorType, MultiThreaded, ImageToTensor),
        #[cfg(feature = "low-level-processors")]
        (FlowFileTransformProcessorType, MultiThreaded, InvokeTractModel),
        #[cfg(feature = "low-level-processors")]
        (FlowFileTransformProcessorType, MultiThreaded, FilterBoundingBoxes),
        #[cfg(feature = "low-level-processors")]
        (FlowFileTransformProcessorType, MultiThreaded, ClassifyOutput),
    ],
    controllers: [
        TractModelService
    ]
);
