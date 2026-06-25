pub trait ControllerServiceApi {
    const INTERFACE_NAME: &'static str;
}

#[macro_export]
macro_rules! impl_interface_fqn {
    ($trait_name:ident) => {
        impl ControllerServiceApi for dyn $trait_name {
            const INTERFACE_NAME: &'static str =
                concat!(module_path!(), "::", stringify!($trait_name));
        }
    };
}

#[derive(Debug)]
pub struct ProvidedInterface<T> {
    pub name: &'static str,
    pub cast: fn(&T) -> *mut std::ffi::c_void,
}

#[macro_export]
macro_rules! create_provided_interface {
    ($trait_type:ty) => {
        ProvidedInterface {
            name: <$trait_type as ControllerServiceApi>::INTERFACE_NAME,
            cast: |instance| {
                let trait_ref: &$trait_type = instance;
                let boxed_ref: Box<&$trait_type> = Box::new(trait_ref);
                Box::into_raw(boxed_ref) as *mut std::ffi::c_void
            },
        }
    };
}
