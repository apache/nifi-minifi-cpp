use crate::ProcessorInputRequirement;
use minifi_native_sys::{
    MinifiInputRequirement, MinifiInputRequirement_MINIFI_INPUT_ALLOWED,
    MinifiInputRequirement_MINIFI_INPUT_FORBIDDEN, MinifiInputRequirement_MINIFI_INPUT_REQUIRED,
    MinifiStringView,
};
use std::os::raw::c_char;

#[derive(Debug)]
pub enum FfiConversionError {
    NullPointer,
    InvalidUtf8,
}

#[derive(Debug)]
pub(crate) struct StringView<'a> {
    inner: MinifiStringView,
    _marker: std::marker::PhantomData<&'a ()>,
}

impl<'a> StringView<'a> {
    pub(crate) fn new(str: &'a str) -> Self {
        Self {
            inner: MinifiStringView {
                data: str.as_ptr() as *const c_char,
                length: str.len(),
            },
            _marker: std::marker::PhantomData,
        }
    }

    pub unsafe fn as_raw(&self) -> MinifiStringView {
        self.inner
    }
}

pub trait StaticStrAsMinifiCStr {
    fn as_minifi_c_type(&self) -> MinifiStringView;
}

impl StaticStrAsMinifiCStr for &'static str {
    fn as_minifi_c_type(&self) -> MinifiStringView {
        MinifiStringView {
            data: self.as_ptr() as *const c_char,
            length: self.len(),
        }
    }
}

pub trait ConvertMinifiStringView {
    unsafe fn as_string(&self) -> Result<String, FfiConversionError>;
    unsafe fn as_str(&self) -> Result<&str, FfiConversionError>;
}

impl ConvertMinifiStringView for MinifiStringView {
    unsafe fn as_string(&self) -> Result<String, FfiConversionError> {
        if self.data.is_null() {
            return Err(FfiConversionError::NullPointer);
        }
        unsafe {
            let slice = std::slice::from_raw_parts(self.data as *const u8, self.length);
            String::from_utf8(slice.to_vec()).map_err(|_| FfiConversionError::InvalidUtf8)
        }
    }

    unsafe fn as_str(&self) -> Result<&str, FfiConversionError> {
        if self.data.is_null() {
            return Err(FfiConversionError::NullPointer);
        }
        unsafe {
            let slice = std::slice::from_raw_parts(self.data as *const u8, self.length);
            str::from_utf8(slice).map_err(|_| FfiConversionError::InvalidUtf8)
        }
    }
}

impl ProcessorInputRequirement {
    pub fn as_minifi_c_type(&self) -> MinifiInputRequirement {
        match self {
            ProcessorInputRequirement::Required => MinifiInputRequirement_MINIFI_INPUT_REQUIRED,
            ProcessorInputRequirement::Allowed => MinifiInputRequirement_MINIFI_INPUT_ALLOWED,
            ProcessorInputRequirement::Forbidden => MinifiInputRequirement_MINIFI_INPUT_FORBIDDEN,
        }
    }
}
