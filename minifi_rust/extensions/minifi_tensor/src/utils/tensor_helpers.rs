use image::{DynamicImage, ImageResult};
use minifi_native::{GetAttribute, InputStream, MinifiError};
use strum_macros::{Display, EnumString};
use tract::__ndarray_interop::TensorInterface;
use tract::Tensor;
use tract::prelude::DatumType;

tract::impl_ndarray_interop!();

fn parse_tensor_shape<Context: GetAttribute>(
    context: &Context,
    id: usize,
) -> Result<Vec<usize>, MinifiError> {
    let shape_str = context.get_required_attribute(&format!("tensor.{}.shape", id))?;

    if shape_str.trim().is_empty() {
        return Ok(Vec::new());
    }

    let shape = shape_str
        .split(',')
        .map(|s| s.trim().parse::<usize>())
        .collect::<Result<Vec<usize>, _>>()?;

    Ok(shape)
}

#[derive(Debug, Clone, Copy, PartialEq, Display, EnumString)]
#[strum(serialize_all = "PascalCase", const_into_str)]
pub(crate) enum MinifiDatumType {
    F32,
}

impl From<MinifiDatumType> for DatumType {
    fn from(value: MinifiDatumType) -> Self {
        match value {
            MinifiDatumType::F32 => DatumType::F32,
        }
    }
}

fn numeric_datum_type_from_str(s: &str) -> Option<DatumType> {
    Some(match s {
        "U8" => DatumType::U8,
        "U16" => DatumType::U16,
        "U32" => DatumType::U32,
        "U64" => DatumType::U64,
        "I8" => DatumType::I8,
        "I16" => DatumType::I16,
        "I32" => DatumType::I32,
        "I64" => DatumType::I64,
        "F16" => DatumType::F16,
        "F32" => DatumType::F32,
        "F64" => DatumType::F64,
        _ => return None,
    })
}

fn parse_tensor_dtype<Context: GetAttribute>(
    context: &Context,
    id: usize,
) -> Result<DatumType, MinifiError> {
    let dtype_str = context.get_required_attribute(&format!("tensor.{}.dtype", id))?;
    numeric_datum_type_from_str(&dtype_str).ok_or_else(|| {
        MinifiError::custom(format!(
            "Unsupported tensor.{}.dtype '{}': only numeric tensors can be read",
            id, dtype_str
        ))
    })
}

pub(crate) fn deserialize_tensors<Context: GetAttribute>(
    context: &Context,
    input_stream: &mut dyn InputStream,
) -> Result<Vec<Tensor>, MinifiError> {
    let mut result = vec![];

    let mut flow_file_contents = Vec::new();
    input_stream.read_to_end(&mut flow_file_contents)?;
    let number_of_tensors = context
        .get_required_attribute("tensors.len")?
        .parse::<usize>()?;

    let mut cursor = 0usize;
    for i in 0..number_of_tensors {
        let tensor_len = context
            .get_required_attribute(&format!("tensor.{}.bytes", i))?
            .parse::<usize>()?;
        let tensor_shape = parse_tensor_shape(context, i)?;
        let tensor_dtype = parse_tensor_dtype(context, i)?;
        let tensor_data = &flow_file_contents[cursor..cursor + tensor_len];
        result.push(Tensor::from_bytes(
            tensor_dtype,
            &tensor_shape,
            tensor_data,
        )?);
        cursor += tensor_len;
    }

    Ok(result)
}

pub(crate) fn tensor_as_f32(tensors: &[Tensor], index: usize) -> Result<Vec<f32>, MinifiError> {
    let tensor = tensors
        .get(index)
        .ok_or(MinifiError::custom("Invalid shape of tensors"))?;
    let casted = tensor.convert_to(DatumType::F32)?;
    Ok(casted.as_slice::<f32>()?.to_vec())
}

pub(crate) fn tensor_shape(tensors: &[Tensor], index: usize) -> Result<Vec<usize>, MinifiError> {
    let tensor = tensors
        .get(index)
        .ok_or(MinifiError::custom("Invalid shape of tensors"))?;
    Ok(tensor.shape()?.to_vec())
}

pub(crate) fn load_as_image(input_stream: &mut dyn InputStream) -> ImageResult<DynamicImage> {
    let mut raw_bytes = Vec::new();
    input_stream.read_to_end(&mut raw_bytes)?;

    image::load_from_memory(&raw_bytes)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_tensor_as_f32_reads_f32_tensor() {
        let t = Tensor::from_slice::<f32>(&[3], &[0.5, -1.5, 2.0]).unwrap();
        assert_eq!(tensor_as_f32(&[t], 0).unwrap(), vec![0.5, -1.5, 2.0]);
    }

    #[test]
    fn test_tensor_as_f32_casts_integer_tensor() {
        // i64 class-index tensor (e.g. TF-OD / EfficientNMS 'detection_classes')
        // must be cast to f32, not byte-reinterpreted.
        let t = Tensor::from_slice::<i64>(&[4], &[0, 1, 17, 80]).unwrap();
        assert_eq!(tensor_as_f32(&[t], 0).unwrap(), vec![0.0, 1.0, 17.0, 80.0]);
    }

    #[test]
    fn test_tensor_as_f32_index_out_of_range_errors() {
        assert!(tensor_as_f32(&[], 0).is_err());
    }

    #[test]
    fn test_numeric_datum_type_accepts_numeric_rejects_other() {
        assert_eq!(numeric_datum_type_from_str("I64"), Some(DatumType::I64));
        assert_eq!(numeric_datum_type_from_str("F32"), Some(DatumType::F32));
        assert_eq!(numeric_datum_type_from_str("U8"), Some(DatumType::U8));
        assert_eq!(numeric_datum_type_from_str("String"), None);
        assert_eq!(numeric_datum_type_from_str("Bool"), None);
    }
}
