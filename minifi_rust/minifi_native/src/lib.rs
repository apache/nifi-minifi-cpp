mod api;
pub mod c_ffi;
pub mod mock;

pub use api::errors::MinifiError;

pub use api::component_definition_traits::{
    ComponentIdentifier, ControllerServiceDefinition, ProcessorDefinition,
};
pub use api::controller_service::{ControllerService, EnableControllerService};
pub use api::processor_wrappers::complex_processor::{ComplexProcessorType, MutTrigger, Trigger};
pub use api::processor_wrappers::flow_file_source::{
    FlowFileSource, FlowFileSourceProcessorType, GeneratedFlowFile,
};
pub use api::processor_wrappers::flow_file_stream_transform::{
    FlowFileStreamTransform, FlowFileStreamTransformProcessorType, MutFlowFileStreamTransform,
    TransformStreamResult,
};
pub use api::processor_wrappers::flow_file_transform::{
    FlowFileTransform, FlowFileTransformProcessorType, TransformedFlowFile,
};

pub use api::processor_wrappers::utils::flow_file_content::Content;

pub use api::processor::{Processor, Schedule};

pub use api::raw_processor::{Concurrent, Exclusive};

pub use api::logger::{LogLevel, Logger};

pub use api::property::{GetControllerService, GetProperty, Property};

pub use api::provided_interface::{ControllerServiceApi, ProvidedInterface};

pub use api::process_session::IoState;

pub use api::attribute::{GetAttribute, OutputAttribute};

pub use api::{
    FlowFile, GetId, InputStream, OnTriggerResult, OutputStream, ProcessContext, ProcessSession,
    ProcessorInputRequirement, Relationship, StandardPropertyValidator,
};

pub use minifi_native_macros as macros;
pub use minifi_native_sys as sys;
pub use mock::{
    MockControllerServiceContext, MockFlowFile, MockLogger, MockProcessContext, MockProcessSession,
    StdLogger,
};

#[unsafe(no_mangle)]
#[allow(non_upper_case_globals)]
#[cfg_attr(target_os = "linux", unsafe(link_section = ".rodata"))]
#[cfg_attr(target_os = "macos", unsafe(link_section = "__DATA,__const"))]
#[cfg_attr(target_os = "windows", unsafe(link_section = ".rdata"))]
pub static minifi_api_version: u32 = minifi_native_sys::MINIFI_API_VERSION;

/// Defines the required MinifiInitExtension C function to register the listed processors and controller services
#[macro_export]
macro_rules! declare_minifi_extension {
    (
        // Match a tuple of three types for each processor
        processors: [ $( ($kind:ty, $thread:ty, $impl:ty) ),* $(,)? ],
        // Match a single type for each controller service
        controllers: [ $( $ctrl:ty ),* $(,)? ]
    ) => {

        #[unsafe(no_mangle)]
        pub extern "C" fn minifi_init_extension(
            extension_context: *mut minifi_native::sys::minifi_extension_context
        ) {

            use minifi_native::c_ffi::StaticStrAsMinifiCStr;
            use minifi_native::c_ffi::RawRegisterableProcessor;
            use minifi_native::c_ffi::RegisterableControllerService;
            unsafe {
                let extension_definition = minifi_native::sys::minifi_extension_definition {
                    name: env!("CARGO_PKG_NAME").as_minifi_c_type(),
                    version: env!("CARGO_PKG_VERSION").as_minifi_c_type(),
                    deinit: None,
                    user_data: std::ptr::null_mut(),
                };

                let extension = minifi_native::sys::minifi_register_extension(extension_context, &extension_definition);


                $(
                    {
                        let processor_def = minifi_native::Processor::<
                                $impl,
                                $kind,
                                $thread,
                                minifi_native::c_ffi::CffiLogger
                        >::get_definition();

                        let proc_desc = processor_def.class_description();

                        minifi_native::sys::minifi_register_processor(extension, &proc_desc.as_raw());
                    }
                )*

                $(
                    {
                        let controller_def =
                            minifi_native::ControllerService::<
                                $ctrl,
                                minifi_native::c_ffi::CffiLogger
                        >::get_definition();

                        let controller_desc = controller_def.class_description();

                        minifi_native::sys::minifi_register_controller_service(extension, &controller_desc.as_raw());
                    }
                )*
            }
        }
    };
}
