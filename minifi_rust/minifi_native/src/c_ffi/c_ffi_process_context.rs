use super::c_ffi_flow_file::CffiFlowFile;
use super::c_ffi_primitives::StringView;
use crate::api::ProcessContext;
use crate::api::controller_service::ControllerService;
use crate::c_ffi::{CffiLogger, StaticStrAsMinifiCStr};
use crate::{
    ComponentIdentifier, ControllerServiceApi, EnableControllerService, MinifiError, Property,
};
use minifi_native_sys::*;
use std::borrow::Cow;
use std::ffi::c_void;
use std::num::NonZeroU32;

/// A safe wrapper around a `minifi_process_context` pointer.
pub struct CffiProcessContext<'a> {
    ptr: *mut minifi_process_context,
    _lifetime: std::marker::PhantomData<&'a ()>,
}

impl<'a> CffiProcessContext<'a> {
    pub fn new(ptr: *mut minifi_process_context) -> Self {
        Self {
            ptr,
            _lifetime: std::marker::PhantomData,
        }
    }
}

unsafe extern "C" fn get_property_callback(
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
            match minifi_process_context_get_property(
                self.ptr,
                property_name.as_raw(),
                ff_ptr,
                Some(get_property_callback),
                &mut result as *mut _ as *mut c_void,
            ) {
                minifi_status_MINIFI_STATUS_SUCCESS => Ok(result),
                minifi_status_MINIFI_STATUS_PROPERTY_NOT_SET => match property.is_required {
                    true => Err(MinifiError::MissingRequiredProperty(Cow::from(
                        property.name,
                    ))),
                    false => Ok(None),
                },
                err_code => Err(MinifiError::StatusError((
                    format!("minifi_process_context_get_property({:?})", property.name).into(),
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
        let str_view = StringView::new(property.name);

        let mut controller_service_ptr: *mut minifi_controller_service = std::ptr::null_mut();

        unsafe {
            let get_cs_status = minifi_process_context_get_controller_service_from_property(
                self.ptr,
                str_view.as_raw(),
                Cs::CLASS_NAME.as_minifi_c_type(),
                &mut controller_service_ptr,
            );
            if get_cs_status != minifi_status_MINIFI_STATUS_SUCCESS {
                return Err(MinifiError::StatusError((
                    format!(
                        "minifi_process_context_get_controller_service_from_property::<{:?}>({:?})",
                        Cs::CLASS_NAME,
                        property
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

    fn get_controller_service_api<Trait: ?Sized + ControllerServiceApi>(
        &self,
        property: &Property,
    ) -> Result<Option<Box<&Trait>>, MinifiError> {
        let str_view = StringView::new(property.name);
        let interface_view = StringView::new(Trait::INTERFACE_NAME);

        let mut interface_ptr: *mut minifi_controller_service = std::ptr::null_mut();

        unsafe {
            let get_cs_status = minifi_process_context_get_controller_service_from_property(
                self.ptr,
                str_view.as_raw(),
                interface_view.as_raw(),
                &mut interface_ptr,
            );

            if get_cs_status == minifi_status_MINIFI_STATUS_PROPERTY_NOT_SET {
                return Ok(None);
            }

            if get_cs_status != minifi_status_MINIFI_STATUS_SUCCESS {
                return Err(MinifiError::StatusError((
                    format!(
                        "Failed to get controller service interface <{}> for property {:?}",
                        Trait::INTERFACE_NAME,
                        property.name
                    )
                    .into(),
                    NonZeroU32::new_unchecked(get_cs_status),
                )));
            }

            if interface_ptr.is_null() {
                return Ok(None);
            }

            // MAGIC: We receive the thin void pointer from the C API,
            // cast it back to a Box of a Fat Pointer, and unbox it!
            let boxed_ref = Box::from_raw(interface_ptr as *mut &Trait);

            Ok(Some(boxed_ref))
        }
    }

    fn report_metrics(&self, metrics: Vec<(String, f64)>) -> Result<(), MinifiError> {
        let mut keys = Vec::new();
        let mut values = Vec::new();
        for (key, value) in &metrics {
            keys.push(StringView::new(key));
            values.push(*value);
        }
        unsafe {
            let err_code = minifi_process_context_report_metrics(
                self.ptr,
                keys.len(),
                keys.as_ptr() as *const minifi_string_view,
                values.as_ptr(),
            );
            if err_code != minifi_status_MINIFI_STATUS_SUCCESS {
                return Err(MinifiError::StatusError((
                    "report_metrics".into(),
                    NonZeroU32::new_unchecked(err_code),
                )));
            }
        }
        Ok(())
    }
}
