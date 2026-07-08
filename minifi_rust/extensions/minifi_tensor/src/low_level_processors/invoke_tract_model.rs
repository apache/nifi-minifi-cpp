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

use crate::utils::tensor_helpers::MinifiDatumType;
pub(crate) use invoke_tract_model_def::TRACT_MODEL_SERVICE;
use invoke_tract_model_def::*;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    FlowFileTransform, GetAttribute, GetControllerService, GetId, GetProperty, InputStream, Logger,
    MinifiError, ProcessError, RouteErrorExt, Schedule, TransformedFlowFile,
};
use std::error::Error;
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

impl InvokeTractModel {
    fn get_payload_as_f32_array(
        input_stream: &mut dyn InputStream,
    ) -> Result<Vec<f32>, Box<dyn Error>> {
        let mut raw_bytes = Vec::new();
        input_stream.read_to_end(&mut raw_bytes)?;
        if raw_bytes.len() % 4 != 0 {
            return Err(MinifiError::custom("Input bytes length is not a multiple of 4").into());
        }
        let mut f32_data = Vec::with_capacity(raw_bytes.len() / 4);
        for chunk in raw_bytes.as_chunks::<4>().0.iter() {
            let val = f32::from_le_bytes(*chunk);
            f32_data.push(val);
        }
        Ok(f32_data)
    }

    fn get_shape_from_attribute<Context: GetAttribute>(
        context: &Context,
    ) -> Result<Vec<usize>, Box<dyn Error>> {
        let shape_str = context
            .get_attribute("tensor.0.shape")
            .and_then(|opt| opt.ok_or(MinifiError::custom("Missing tensor.0.shape")))?;
        shape_str
            .split(',')
            .map(|s| s.trim().parse::<usize>())
            .collect::<Result<Vec<_>, _>>()
            .map_err(|e| e.into())
    }

    fn check_input_dtype<Context: GetAttribute>(context: &Context) -> Result<(), Box<dyn Error>> {
        match context.get_attribute("tensor.0.dtype")? {
            None => Ok(()),
            Some(dtype) => dtype.parse::<MinifiDatumType>().map(|_| ()).map_err(|_| {
                MinifiError::custom(format!(
                    "Unsupported tensor.0.dtype '{}' (only '{}' is supported)",
                    dtype,
                    MinifiDatumType::F32
                ))
                .into()
            }),
        }
    }

    fn get_input_tensor<Context: GetAttribute>(
        context: &Context,
        input_stream: &mut dyn InputStream,
    ) -> Result<Tensor, Box<dyn Error>> {
        Self::check_input_dtype(context)?;
        let shape = Self::get_shape_from_attribute(context)?;
        let f32_data = Self::get_payload_as_f32_array(input_stream)?;

        let array = ndarray::Array::from_shape_vec(shape, f32_data)
            .map_err(|e| MinifiError::custom(format!("Failed to create array: {}", e)))?;

        Ok(array.tract()?)
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

        let input_tensor: Tensor = Self::get_input_tensor(context, input_stream)
            .map_err(|e| MinifiError::custom(e.to_string()))
            .route_err_to_failure()?;

        let output_tensors = controller_service
            .run_inference(vec![input_tensor])
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
    use super::*;
    use minifi_native::{MockLogger, MockProcessContext};
    use std::io::Cursor;

    #[test]
    fn test_get_shape_parses_correctly() {
        let mut context = MockProcessContext::new();
        // Insert a valid shape with spaces
        context
            .attributes
            .insert("tensor.0.shape".to_string(), "1, 3, 224, 224".to_string());

        let shape =
            InvokeTractModel::get_shape_from_attribute(&context).expect("Should parse valid shape");
        assert_eq!(shape, vec![1, 3, 224, 224]);
    }

    #[test]
    fn test_get_shape_missing_attribute_fails() {
        let context = MockProcessContext::new(); // Empty attributes
        let result = InvokeTractModel::get_shape_from_attribute(&context);
        assert!(result.is_err(), "Should fail when tensor.shape is missing");
    }

    #[test]
    fn test_get_shape_invalid_data_fails() {
        let mut context = MockProcessContext::new();
        context
            .attributes
            .insert("tensor.0.shape".to_string(), "1, apple, 224".to_string());

        let result = InvokeTractModel::get_shape_from_attribute(&context);
        assert!(
            result.is_err(),
            "Should fail when shape contains non-integers"
        );
    }

    #[test]
    fn test_get_payload_as_f32_array_success() {
        let expected_floats: [f32; 3] = [1.0, 2.5, -3.5];
        let mut bytes = Vec::new();
        for f in expected_floats.iter() {
            bytes.extend_from_slice(&f.to_le_bytes());
        }
        let mut stream = Cursor::new(bytes);
        let result = InvokeTractModel::get_payload_as_f32_array(&mut stream)
            .expect("Should parse valid byte stream");
        assert_eq!(result, vec![1.0, 2.5, -3.5]);
    }

    #[test]
    fn test_get_payload_invalid_length_fails() {
        let bytes = vec![0u8, 1, 2, 3, 4];
        let mut stream = Cursor::new(bytes);

        let result = InvokeTractModel::get_payload_as_f32_array(&mut stream);
        assert!(
            result.is_err(),
            "Should error out if byte length is not a multiple of 4"
        );
    }

    #[test]
    fn test_check_input_dtype_accepts_missing_and_f32() {
        let mut context = MockProcessContext::new();
        assert!(InvokeTractModel::check_input_dtype(&context).is_ok());
        context
            .attributes
            .insert("tensor.0.dtype".to_string(), "F32".to_string());
        assert!(InvokeTractModel::check_input_dtype(&context).is_ok());
    }

    #[test]
    fn test_check_input_dtype_rejects_other() {
        let mut context = MockProcessContext::new();
        context
            .attributes
            .insert("tensor.0.dtype".to_string(), "u8".to_string());
        assert!(InvokeTractModel::check_input_dtype(&context).is_err());
    }

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
