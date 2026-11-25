// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use super::c_ffi_primitives::StringView;
use crate::api::ScheduleContext;
use crate::api::controller_service::ControllerService;
use crate::api::property::{GetProperty, PropertySchema};
use crate::c_ffi::{CffiLogger, StaticStrAsMinifiCStr};
use crate::{
    ComponentIdentifier, ControllerServiceApi, EnableControllerService, MinifiError, Property,
};
use minifi_native_sys::*;
use std::ffi::c_void;
use std::num::NonZeroU32;

/// A safe wrapper around a `minifi_process_context` pointer.
pub struct CffiScheduleContext<'a> {
    ptr: *mut minifi_process_context,
    _lifetime: std::marker::PhantomData<&'a ()>,
}

impl<'a> CffiScheduleContext<'a> {
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

pub(crate) trait RawContextAccess {
    unsafe fn raw_get_property(
        &self,
        property_name: minifi_string_view,
        cb: Option<unsafe extern "C" fn(*mut c_void, minifi_string_view)>,
        user_ctx: *mut c_void,
    ) -> minifi_status;

    unsafe fn raw_get_controller_service_from_property(
        &self,
        property_name: minifi_string_view,
        controller_service_type: minifi_string_view,
        controller_service_out: *mut *mut minifi_controller_service,
    ) -> minifi_status;
}

impl RawContextAccess for CffiScheduleContext<'_> {
    unsafe fn raw_get_property(
        &self,
        property_name: minifi_string_view,
        cb: Option<unsafe extern "C" fn(*mut c_void, minifi_string_view)>,
        user_ctx: *mut c_void,
    ) -> minifi_status {
        unsafe {
            minifi_process_context_get_property(
                self.ptr,
                property_name,
                std::ptr::null_mut(),
                cb,
                user_ctx,
            )
        }
    }

    unsafe fn raw_get_controller_service_from_property(
        &self,
        property_name: minifi_string_view,
        controller_service_type: minifi_string_view,
        controller_service_out: *mut *mut minifi_controller_service,
    ) -> minifi_status {
        unsafe {
            minifi_process_context_get_controller_service_from_property(
                self.ptr,
                property_name,
                controller_service_type,
                controller_service_out,
            )
        }
    }
}

pub(crate) fn get_raw_property<A: RawContextAccess, P: PropertySchema + ?Sized>(
    access: &A,
    property: &Property<P>,
) -> Result<Option<String>, MinifiError> {
    read_property(property, |name, cb, user_ctx| unsafe {
        access.raw_get_property(name, cb, user_ctx)
    })
}

pub(crate) fn read_property<P: PropertySchema + ?Sized>(
    property: &Property<P>,
    call_c_get_property: impl FnOnce(
        minifi_string_view,
        Option<unsafe extern "C" fn(*mut c_void, minifi_string_view)>,
        *mut c_void,
    ) -> minifi_status,
) -> Result<Option<String>, MinifiError> {
    let mut result: Option<String> = None;
    let property_name: StringView = StringView::new(property.name);

    let status = call_c_get_property(
        unsafe { property_name.as_raw() },
        Some(get_property_callback),
        &mut result as *mut _ as *mut c_void,
    );

    #[allow(non_upper_case_globals)]
    match status {
        minifi_status_MINIFI_STATUS_SUCCESS => Ok(result),
        minifi_status_MINIFI_STATUS_PROPERTY_NOT_SET => Ok(None),
        err_code => Err(MinifiError::StatusError((
            format!("get_property({:?})", property.name).into(),
            unsafe { NonZeroU32::new_unchecked(err_code) },
        ))),
    }
}

pub(crate) fn get_raw_controller_service<'a, A, Cs, P>(
    access: &A,
    property: &Property<P>,
) -> Result<Option<&'a Cs>, MinifiError>
where
    A: RawContextAccess,
    Cs: ComponentIdentifier + 'static,
    P: PropertySchema + ?Sized,
{
    let str_view = StringView::new(property.name);

    let mut controller_service_ptr: *mut minifi_controller_service = std::ptr::null_mut();

    unsafe {
        let get_cs_status = access.raw_get_controller_service_from_property(
            str_view.as_raw(),
            Cs::CLASS_NAME.as_minifi_c_type(),
            &mut controller_service_ptr,
        );
        #[allow(non_upper_case_globals)]
        match get_cs_status {
            minifi_status_MINIFI_STATUS_PROPERTY_NOT_SET => Ok(None),
            minifi_status_MINIFI_STATUS_SUCCESS => Ok(Some(
                (controller_service_ptr as *const Cs)
                    .as_ref()
                    .expect("C returned a null pointer"),
            )),
            err => Err(MinifiError::StatusError((
                format!(
                    "get_controller_service_from_property::<{:?}>({:?})",
                    Cs::CLASS_NAME,
                    property.name
                )
                .into(),
                NonZeroU32::new_unchecked(err),
            ))),
        }
    }
}

pub(crate) fn get_controller_service<'a, A, Cs>(
    access: &A,
    property: &Property<Cs>,
) -> Result<Option<&'a Cs>, MinifiError>
where
    A: RawContextAccess,
    Cs: EnableControllerService + ComponentIdentifier + PropertySchema + 'static,
{
    match get_raw_controller_service::<A, ControllerService<Cs, CffiLogger>, Cs>(access, property)?
    {
        None => Ok(None),
        Some(f) => Ok(f.get_implementation()),
    }
}

#[allow(clippy::redundant_allocation)] // the Box owns the fat pointer rebuilt from the thin C pointer
pub(crate) fn get_controller_service_api<'a, A, Trait>(
    access: &A,
    property: &Property<Trait>,
) -> Result<Option<Box<&'a Trait>>, MinifiError>
where
    A: RawContextAccess,
    Trait: ?Sized + ControllerServiceApi + PropertySchema,
{
    let str_view = StringView::new(property.name);
    let interface_view = StringView::new(Trait::INTERFACE_NAME);

    let mut interface_ptr: *mut minifi_controller_service = std::ptr::null_mut();

    unsafe {
        let get_cs_status = access.raw_get_controller_service_from_property(
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

impl GetProperty for CffiScheduleContext<'_> {
    fn get_raw_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<String>, MinifiError> {
        get_raw_property(self, property)
    }
}

impl<'a> ScheduleContext for CffiScheduleContext<'a> {
    fn get_raw_controller_service<Cs, P>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<&'a Cs>, MinifiError>
    where
        Cs: ComponentIdentifier + 'static,
        P: PropertySchema + ?Sized,
    {
        get_raw_controller_service(self, property)
    }

    fn get_controller_service<Cs>(
        &self,
        property: &Property<Cs>,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + PropertySchema + 'static,
    {
        get_controller_service(self, property)
    }

    fn get_controller_service_api<Trait: ?Sized + ControllerServiceApi + PropertySchema>(
        &self,
        property: &Property<Trait>,
    ) -> Result<Option<Box<&Trait>>, MinifiError> {
        get_controller_service_api(self, property)
    }
}
