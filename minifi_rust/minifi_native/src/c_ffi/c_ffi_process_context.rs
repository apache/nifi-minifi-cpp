use super::c_ffi_flow_file::CffiFlowFile;
use super::c_ffi_primitives::StringView;
use crate::api::ProcessContext;
use crate::api::controller_service::ControllerService;
use crate::c_ffi::{CffiLogger, StaticStrAsMinifiCStr};
use crate::{ComponentIdentifier, EnableControllerService, MinifiError, Property};
use minifi_native_sys::*;
use std::borrow::Cow;
use std::ffi::c_void;
use std::num::NonZeroU32;

/// A safe wrapper around a `MinifiProcessContext` pointer.
pub struct CffiProcessContext<'a> {
    ptr: *mut MinifiProcessContext,
    _lifetime: std::marker::PhantomData<&'a ()>,
}

impl<'a> CffiProcessContext<'a> {
    pub fn new(ptr: *mut MinifiProcessContext) -> Self {
        Self {
            ptr,
            _lifetime: std::marker::PhantomData,
        }
    }
}

unsafe extern "C" fn get_property_callback(
    output_option: *mut c_void,
    property_c_value: MinifiStringView,
) {
    unsafe {
        let result_target = &mut *(output_option as *mut Option<String>);

        if property_c_value.data.is_null() || property_c_value.length == 0 {
            *result_target = None;
            return;
        }

        let value_slice =
            std::slice::from_raw_parts(property_c_value.data as *const u8, property_c_value.length);
        if let Ok(string_value) = String::from_utf8(value_slice.to_vec()) {
            *result_target = Some(string_value);
        }
    }
}

impl<'a> ProcessContext for CffiProcessContext<'a> {
    type FlowFile = CffiFlowFile<'a>; // FlowFile shouldn't outlive the ProcessContext
    fn get_property(
        &self,
        property: &Property,
        flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<String>, MinifiError> {
        let ff_ptr = flow_file.map_or(std::ptr::null_mut(), |ff| ff.get_ptr());

        let mut result: Option<String> = None;
        let property_name: StringView = StringView::new(property.name);

        #[allow(non_upper_case_globals)]
        unsafe {
            match MinifiProcessContextGetProperty(
                self.ptr,
                property_name.as_raw(),
                ff_ptr,
                Some(get_property_callback),
                &mut result as *mut _ as *mut c_void,
            ) {
                MinifiStatus_MINIFI_STATUS_SUCCESS => Ok(result),
                MinifiStatus_MINIFI_STATUS_PROPERTY_NOT_SET => match property.is_required {
                    true => Err(MinifiError::MissingRequiredProperty(Cow::from(
                        property.name,
                    ))),
                    false => Ok(None),
                },
                err_code => Err(MinifiError::StatusError((
                    format!("MinifiProcessContextGetProperty({:?})", property.name).into(),
                    NonZeroU32::new_unchecked(err_code),
                ))),
            }
        }
    }

    fn get_raw_controller_service<Cs>(
        &self,
        property: &Property,
    ) -> Result<Option<&'a Cs>, MinifiError>
    where
        Cs: ComponentIdentifier + 'static,
    {
        if let Some(service_name) = self.get_property(property, None)? {
            let str_view = StringView::new(service_name.as_str());

            let mut controller_service_ptr: *mut MinifiControllerService = std::ptr::null_mut();

            unsafe {
                let get_cs_status = MinifiProcessContextGetControllerService(
                    self.ptr,
                    str_view.as_raw(),
                    Cs::CLASS_NAME.as_minifi_c_type(),
                    &mut controller_service_ptr,
                );
                if get_cs_status != MinifiStatus_MINIFI_STATUS_SUCCESS {
                    return Err(MinifiError::StatusError((
                        format!(
                            "MinifiProcessContextGetControllerService::<{:?}>({:?})",
                            Cs::CLASS_NAME,
                            service_name
                        )
                        .into(),
                        NonZeroU32::new_unchecked(get_cs_status),
                    )));
                }
                let cs_ref: &Cs = {
                    (controller_service_ptr as *const Cs)
                        .as_ref()
                        .expect("C returned a null pointer")
                };
                Ok(Some(cs_ref))
            }
        } else {
            Ok(None)
        }
    }

    fn get_controller_service<Cs>(&self, property: &Property) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + 'static,
    {
        match self.get_raw_controller_service::<ControllerService<Cs, CffiLogger>>(property)? {
            None => Ok(None),
            Some(f) => Ok(f.get_implementation()),
        }
    }
}
