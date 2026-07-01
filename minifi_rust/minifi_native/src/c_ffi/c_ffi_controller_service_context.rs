use crate::c_ffi::c_ffi_primitives::StringView;
use crate::{GetProperty, MinifiError, Property};
use minifi_native_sys::{
    minifi_controller_service_context, minifi_controller_service_context_get_property,
    minifi_status_MINIFI_STATUS_PROPERTY_NOT_SET, minifi_status_MINIFI_STATUS_SUCCESS,
    minifi_string_view,
};
use std::borrow::Cow;
use std::ffi::c_void;
use std::num::NonZeroU32;

pub struct CffiControllerServiceContext<'a> {
    ptr: *mut minifi_controller_service_context,
    _lifetime: std::marker::PhantomData<&'a ()>,
}

impl<'a> CffiControllerServiceContext<'a> {
    pub fn new(ptr: *mut minifi_controller_service_context) -> Self {
        Self {
            ptr,
            _lifetime: std::marker::PhantomData,
        }
    }
}

unsafe extern "C" fn property_callback(
    output_option: *mut c_void,
    property_c_value: minifi_string_view,
) {
    unsafe {
        let result_target = &mut *(output_option as *mut Option<String>);

        if property_c_value.data.is_null() {
            *result_target = None;
            return;
        }

        let value_slice =
            std::slice::from_raw_parts(property_c_value.data.cast::<u8>(), property_c_value.length);
        if let Ok(string_value) = String::from_utf8(value_slice.to_vec()) {
            *result_target = Some(string_value);
        }
    }
}

impl<'a> GetProperty for CffiControllerServiceContext<'a> {
    fn get_property(&self, property: &Property) -> Result<Option<String>, MinifiError> {
        let mut result: Option<String> = None;
        let property_name: StringView = StringView::new(property.name);

        let status = unsafe {
            minifi_controller_service_context_get_property(
                self.ptr,
                property_name.as_raw(),
                Some(property_callback),
                &mut result as *mut _ as *mut c_void,
            )
        };

        #[allow(non_upper_case_globals)]
        match status {
            minifi_status_MINIFI_STATUS_SUCCESS => Ok(result),
            minifi_status_MINIFI_STATUS_PROPERTY_NOT_SET => match property.is_required {
                true => Err(MinifiError::MissingRequiredProperty(Cow::from(
                    property.name,
                ))),
                false => Ok(None),
            },
            err_code => Err(MinifiError::StatusError((
                format!(
                    "minifi_controller_service_context_get_property({:?})",
                    property.name
                )
                .into(),
                // SAFETY: err_code is non-zero because SUCCESS is handled above.
                unsafe { NonZeroU32::new_unchecked(err_code) },
            ))),
        }
    }
}
