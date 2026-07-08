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

mod classify_object_def;

use crate::low_level_processors::classify_output::ClassifyOutput;
use crate::low_level_processors::image_to_tensor::ImageToTensor;
use crate::utils::tensor_helpers::load_as_image;
use classify_object_def::TRACT_MODEL_SERVICE;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    FlowFileTransform, GetAttribute, GetControllerService, GetId, GetProperty, InputStream, Logger,
    MinifiError, ProcessError, RouteErrorExt, Schedule, TransformedFlowFile,
};
use tract::Tensor;

#[derive(ComponentIdentifier)]
pub(crate) struct ClassifyImage {
    image_to_tensor: ImageToTensor,
    classify_output: ClassifyOutput,
}

impl Schedule for ClassifyImage {
    fn schedule<Ctx: GetProperty, L: Logger>(context: &Ctx, logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let image_to_tensor = ImageToTensor::schedule(context, logger)?;
        let classify_output = ClassifyOutput::schedule(context, logger)?;
        Ok(Self {
            image_to_tensor,
            classify_output,
        })
    }
}

impl FlowFileTransform for ClassifyImage {
    fn transform<
        'a,
        Context: GetProperty + GetControllerService + GetAttribute + GetId,
        LoggerImpl: Logger,
    >(
        &self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        _logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, ProcessError> {
        let tract_model_service = context.get_controller_service(&TRACT_MODEL_SERVICE)?;
        let img = load_as_image(input_stream).route_err_to_failure()?;

        // ImageToTensor
        let input_tensor: Tensor = self
            .image_to_tensor
            .get_tensor(img)
            .route_err_to_failure()?;

        // InvokeTract
        let output_tensors = tract_model_service
            .run_inference(vec![input_tensor])
            .route_err_to_failure()?;

        // ClassifyOutput
        self.classify_output.classify(context, output_tensors)
    }
}
