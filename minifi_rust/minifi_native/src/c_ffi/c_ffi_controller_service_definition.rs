use crate::api::RawControllerService;
use crate::c_ffi::c_ffi_controller_service_context::CffiControllerServiceContext;
use crate::c_ffi::c_ffi_property::CProperties;
use crate::c_ffi::{CffiLogger, StaticStrAsMinifiCStr};
use crate::{
    ComponentIdentifier, ControllerService, ControllerServiceDefinition, EnableControllerService,
    LogLevel, Property,
};
use minifi_native_sys::{
    minifi_controller_service_callbacks, minifi_controller_service_class_definition,
    minifi_controller_service_context, minifi_controller_service_metadata, minifi_status,
};
use std::ffi::c_void;

#[derive(Debug)]
pub struct ControllerServiceClassDefinition<'a> {
    inner: minifi_controller_service_class_definition,
    _lifetime: std::marker::PhantomData<&'a ()>,
}

impl<'a> ControllerServiceClassDefinition<'a> {
    pub(crate) fn new(inner: minifi_controller_service_class_definition) -> Self {
        Self {
            inner,
            _lifetime: std::marker::PhantomData,
        }
    }

    pub unsafe fn as_raw(&self) -> minifi_controller_service_class_definition {
        self.inner
    }
}

pub struct CffiControllerServiceDefinition<T>
where
    T: RawControllerService + ComponentIdentifier,
{
    name: &'static str,
    description_text: &'static str,

    c_properties: CProperties,

    _phantom: std::marker::PhantomData<T>,
}

impl<T> CffiControllerServiceDefinition<T>
where
    T: RawControllerService<LoggerType = CffiLogger> + ComponentIdentifier,
{
    pub fn new(description_text: &'static str, properties: &'static [Property]) -> Self {
        let c_properties = Property::create_c_properties(properties);

        Self {
            name: T::CLASS_NAME,
            description_text,
            c_properties,
            _phantom: std::marker::PhantomData,
        }
    }

    unsafe extern "C" fn create_controller_service(
        metadata: minifi_controller_service_metadata,
    ) -> *mut c_void {
        let logger = CffiLogger::new(metadata.logger);
        let controller_service = Box::new(T::new(logger));
        Box::into_raw(controller_service) as *mut c_void
    }

    unsafe extern "C" fn destroy_controller_service(controller_service_ptr: *mut c_void) {
        unsafe {
            if !controller_service_ptr.is_null() {
                let _ = Box::from_raw(controller_service_ptr as *mut T);
            }
        }
    }

    unsafe extern "C" fn enable_controller_service(
        controller_service_ptr: *mut c_void,
        context_ptr: *mut minifi_controller_service_context,
    ) -> minifi_status {
        unsafe {
            let controller_service = &mut *(controller_service_ptr as *mut T);
            let context = CffiControllerServiceContext::new(context_ptr);
            match controller_service.enable(&context) {
                Ok(_) => 0,
                Err(err) => {
                    controller_service.log(LogLevel::Error, format_args!("{:?}", err));
                    err.to_status()
                }
            }
        }
    }

    unsafe extern "C" fn disable_controller_service(controller_service_ptr: *mut c_void) {
        unsafe {
            let controller_service = &mut *(controller_service_ptr as *mut T);
            controller_service.disable()
        }
    }
}

pub trait DynRawControllerServiceDefinition {
    fn class_description(&'_ self) -> ControllerServiceClassDefinition<'_>;
}

impl<T> DynRawControllerServiceDefinition for CffiControllerServiceDefinition<T>
where
    T: RawControllerService<LoggerType = CffiLogger> + ComponentIdentifier,
{
    fn class_description(&'_ self) -> ControllerServiceClassDefinition<'_> {
        unsafe {
            ControllerServiceClassDefinition::new(minifi_controller_service_class_definition {
                full_name: self.name.as_minifi_c_type(),
                description: self.description_text.as_minifi_c_type(),
                properties_count: self.c_properties.len(),
                properties_ptr: self.c_properties.get_ptr(),
                callbacks: minifi_controller_service_callbacks {
                    create: Some(Self::create_controller_service),
                    destroy: Some(Self::destroy_controller_service),
                    enable: Some(Self::enable_controller_service),
                    disable: Some(Self::disable_controller_service),
                    get_interface: None, // TODO(mzink)
                },
                provided_interfaces_count: 0,
                provided_interfaces_ptr: std::ptr::null_mut()  // TODO(mzink)
            })
        }
    }
}

pub trait RegisterableControllerService {
    fn get_definition() -> Box<dyn DynRawControllerServiceDefinition>;
}

impl<T> RegisterableControllerService for T
where
    T: ComponentIdentifier
        + ControllerServiceDefinition
        + RawControllerService<LoggerType = CffiLogger>
        + 'static,
{
    fn get_definition() -> Box<dyn DynRawControllerServiceDefinition> {
        Box::new(CffiControllerServiceDefinition::<T>::new(
            T::DESCRIPTION,
            T::PROPERTIES,
        ))
    }
}

impl<Implementation> RegisterableControllerService for ControllerService<Implementation, CffiLogger>
where
    Implementation:
        EnableControllerService + ComponentIdentifier + ControllerServiceDefinition + 'static,
{
    fn get_definition() -> Box<dyn DynRawControllerServiceDefinition> {
        Box::new(CffiControllerServiceDefinition::<
            ControllerService<Implementation, CffiLogger>,
        >::new(
            Implementation::DESCRIPTION, Implementation::PROPERTIES
        ))
    }
}
