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

mod detect_object_def;

use crate::low_level_processors::filter_bounding_boxes::FilterBoundingBoxes;
use crate::low_level_processors::image_to_tensor::ImageToTensor;
use crate::utils::dimensions::Dimensions;
use crate::utils::tensor_helpers::load_as_image;
use detect_object_def::TRACT_MODEL_SERVICE;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    FlowFileTransform, GetAttribute, GetControllerService, GetId, GetProperty, InputStream, Logger,
    MinifiError, ProcessError, RouteErrorExt, Schedule, TransformedFlowFile,
};
use tract::Tensor;

tract::impl_ndarray_interop!();

#[derive(ComponentIdentifier)]
pub(crate) struct DetectObject {
    image_to_tensor: ImageToTensor,
    filter_bounding_boxes: FilterBoundingBoxes,
}

impl Schedule for DetectObject {
    fn schedule<Ctx: GetProperty, L: Logger>(context: &Ctx, logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let image_to_tensor = ImageToTensor::schedule(context, logger)?;
        let filter_bounding_boxes = FilterBoundingBoxes::schedule(context, logger)?;
        Ok(Self {
            image_to_tensor,
            filter_bounding_boxes,
        })
    }
}

impl FlowFileTransform for DetectObject {
    fn transform<
        'a,
        Context: GetProperty + GetControllerService + GetAttribute + GetId,
        LoggerImpl: Logger,
    >(
        &self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, ProcessError> {
        let tract_model_service = context.get_controller_service(&TRACT_MODEL_SERVICE)?;
        let img = load_as_image(input_stream).route_err_to_failure()?;
        let orig_dim = Dimensions::from_image(&img);
        let target_dim = self.image_to_tensor.get_target_dim();

        // ImageToTensor
        let input_tensor: Tensor = self
            .image_to_tensor
            .get_tensor(img)
            .route_err_to_failure()?;

        // InvokeTract
        let output_tensors = tract_model_service
            .run_inference(vec![input_tensor])
            .route_err_to_failure()?;

        // FilterBoundingBox
        self.filter_bounding_boxes
            .filter(context, logger, output_tensors, orig_dim, target_dim)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::low_level_processors::image_to_tensor::{
        PIXEL_DIVISOR, TARGET_HEIGHT, TARGET_WIDTH,
    };
    use minifi_native::{MockLogger, MockProcessContext};
    use std::io::Cursor;

    #[test]
    fn test_missing_controller_service_errors() {
        let mut context = MockProcessContext::new();
        context.properties.insert(TARGET_HEIGHT.name(), "100");
        context.properties.insert(TARGET_WIDTH.name(), "100");
        context.properties.insert(PIXEL_DIVISOR.name(), "1.0");
        let processor =
            DetectObject::schedule(&context, &MockLogger::new()).expect("Expected to schedule");
        let mut input_stream = Cursor::new(vec![]);

        let result = processor.transform(&context, &mut input_stream, &MockLogger::new());
        assert!(
            result.is_err(),
            "Should error when TractModelService is missing"
        );
    }
}
