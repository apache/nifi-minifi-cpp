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

use crate::utils::tensor_helpers::deserialize_tensors;
pub(crate) use invoke_tract_model_def::TRACT_MODEL_SERVICE;
use invoke_tract_model_def::*;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    FlowFileTransform, GetAttribute, GetControllerService, GetId, GetProperty, InputStream, Logger,
    MinifiError, ProcessError, RouteErrorExt, Schedule, TransformedFlowFile,
};
use tract::__ndarray_interop::TensorInterface;
use tract::Tensor;
tract::impl_ndarray_interop!();

mod invoke_tract_model_def;

#[derive(ComponentIdentifier)]
pub(crate) struct InvokeTractModel {}

impl Schedule for InvokeTractModel {
    fn schedule<Ctx: GetProperty, L: Logger>(
        _context: &Ctx,
        _logger: &L,
    ) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self {})
    }
}

impl FlowFileTransform for InvokeTractModel {
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
        let controller_service = context.get_controller_service(&TRACT_MODEL_SERVICE)?;

        let input_tensors: Vec<Tensor> =
            deserialize_tensors(context, input_stream).route_err_to_failure()?;
        if input_tensors.len() != 1 {
            return Err(ProcessError::route_to_failure("Invalid input"));
        };

        let output_tensors = controller_service
            .run_inference(input_tensors)
            .route_err_to_failure()?;
        let mut output_bytes = Vec::new();
        let mut transformed = TransformedFlowFile::new(&SUCCESS, None)
            .with_attribute("tensors.len", output_tensors.len().to_string());

        for (i, tensor) in output_tensors.iter().enumerate() {
            let (datum_type, out_shape, raw_tensor_bytes) = tensor
                .as_bytes()
                .map_err(|e| MinifiError::custom(format!("Failed to read tensor bytes: {}", e)))?;

            output_bytes.extend_from_slice(raw_tensor_bytes);

            let out_shape_str = out_shape
                .iter()
                .map(|d| d.to_string())
                .collect::<Vec<_>>()
                .join(",");

            let tensor_bytes = raw_tensor_bytes.len().to_string();
            transformed = transformed.with_attributes([
                (format!("tensor.{}.shape", i), out_shape_str),
                (format!("tensor.{}.bytes", i), tensor_bytes),
                (format!("tensor.{}.dtype", i), format!("{:?}", datum_type)),
            ]);
        }

        Ok(transformed.with_content(output_bytes.into()))
    }
}

#[cfg(test)]
mod tests {
    use crate::InvokeTractModel;
    use minifi_native::FlowFileTransform;
    use minifi_native::{MockLogger, MockProcessContext};
    use std::io::Cursor;

    #[test]
    fn test_transform_missing_controller_service_throws_error() {
        let processor = InvokeTractModel {};
        let context = MockProcessContext::new();
        let mut stream = Cursor::new(vec![]);
        let logger = MockLogger::new();

        let result = processor.transform(&context, &mut stream, &logger);

        assert!(
            result.is_err(),
            "Should throw an error when TractModelService is missing"
        );
    }
}
