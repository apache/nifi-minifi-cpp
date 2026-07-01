use crate::api::RawControllerService;
use crate::c_ffi::c_ffi_controller_service_context::CffiControllerServiceContext;
use crate::c_ffi::c_ffi_property::CProperties;
use crate::c_ffi::{CffiLogger, StaticStrAsMinifiCStr};
use crate::{
    ComponentIdentifier, ControllerService, ControllerServiceDefinition, EnableControllerService,
    LogLevel, Property, ProvidedInterface,
};
use minifi_native_sys::{
    minifi_controller_service_callbacks, minifi_controller_service_class_definition,
    minifi_controller_service_context, minifi_controller_service_metadata, minifi_status,
    minifi_string_view,
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

pub struct CffiControllerServiceDefinition<T, P>
where
    T: RawControllerService + ComponentIdentifier,
    P: ControllerServiceDefinition,
{
    name: &'static str,
    description_text: &'static str,

    c_properties: CProperties,
    c_provided_interfaces: Vec<minifi_string_view>,

    _phantom: std::marker::PhantomData<T>,
    _phantom_2: std::marker::PhantomData<P>,
}

impl<T, P> CffiControllerServiceDefinition<T, P>
where
    T: RawControllerService<LoggerType = CffiLogger> + ComponentIdentifier,
    P: ControllerServiceDefinition,
{
    pub fn new(
        description_text: &'static str,
        properties: &'static [Property],
        provided_interfaces: &'static [ProvidedInterface<P>],
    ) -> Self {
        let c_properties = Property::create_c_properties(properties);
        let mut c_provided_apis = Vec::new();
        for provided_interface in provided_interfaces {
            c_provided_apis.push(provided_interface.name.as_minifi_c_type());
        }
        Self {
            name: T::CLASS_NAME,
            description_text,
            c_properties,
            c_provided_interfaces: c_provided_apis,
            _phantom: std::marker::PhantomData,
            _phantom_2: std::marker::PhantomData,
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

    extern "C" fn get_interface<
        C: ControllerServiceDefinition + EnableControllerService + ComponentIdentifier,
    >(
        self_ptr: *mut std::ffi::c_void,
        interface_name: minifi_native_sys::minifi_string_view,
    ) -> *mut std::ffi::c_void {
        let name = unsafe {
            let slice =
                std::slice::from_raw_parts(interface_name.data.cast::<u8>(), interface_name.length);
            match std::str::from_utf8(slice) {
                Ok(s) => s,
                Err(_) => return std::ptr::null_mut(),
            }
        };

        let wrapper = unsafe { &*(self_ptr as *const ControllerService<C, CffiLogger>) };

        if let Some(inner_instance) = wrapper.get_implementation() {
            for iface in C::PROVIDED_APIS {
                if iface.name == name {
                    return (iface.cast)(inner_instance);
                }
            }
        }

        std::ptr::null_mut()
    }
}

pub trait DynRawControllerServiceDefinition {
    fn class_description(&'_ self) -> ControllerServiceClassDefinition<'_>;
}

impl<Impl> DynRawControllerServiceDefinition
    for CffiControllerServiceDefinition<ControllerService<Impl, CffiLogger>, Impl>
where
    Impl: EnableControllerService + ComponentIdentifier + ControllerServiceDefinition + 'static,
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
                    get_interface: Some(Self::get_interface::<Impl>),
                },
                provided_interfaces_count: self.c_provided_interfaces.len(),
                provided_interfaces_ptr: self.c_provided_interfaces.as_ptr(),
            })
        }
    }
}

pub trait RegisterableControllerService {
    fn get_definition() -> Box<dyn DynRawControllerServiceDefinition>;
}

impl<Implementation> RegisterableControllerService for ControllerService<Implementation, CffiLogger>
where
    Implementation:
        EnableControllerService + ComponentIdentifier + ControllerServiceDefinition + 'static,
{
    fn get_definition() -> Box<dyn DynRawControllerServiceDefinition> {
        Box::new(CffiControllerServiceDefinition::<
            ControllerService<Implementation, CffiLogger>,
            Implementation,
        >::new(
            Implementation::DESCRIPTION,
            Implementation::PROPERTIES,
            Implementation::PROVIDED_APIS,
        ))
    }
}
