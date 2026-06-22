use std::ffi::c_void;
use std::ptr;

use super::c_ffi_primitives::{StaticStrAsMinifiCStr};
use super::c_ffi_process_context::CffiProcessContext;
use super::c_ffi_process_session::CffiProcessSession;
use crate::api::raw_processor::{MultiThreadedTrigger, SingleThreadedTrigger};
use crate::api::{ProcessorInputRequirement, RawProcessor, ThreadingModel};
use crate::c_ffi::CffiLogger;
use crate::c_ffi::c_ffi_output_attribute::COutputAttributes;
use crate::c_ffi::c_ffi_property::CProperties;
use crate::{
    ComponentIdentifier, Concurrent, Exclusive,
    LogLevel, OutputAttribute, Processor, ProcessorDefinition, Property, Schedule,
};
use crate::{OnTriggerResult, Relationship};
use minifi_native_sys::*;

pub trait DispatchOnTrigger<M: ThreadingModel> {
    unsafe fn dispatch_on_trigger(
        processor: *mut c_void,
        context: *mut minifi_process_context,
        session: *mut minifi_process_session,
    ) -> minifi_status;
}

impl<T> DispatchOnTrigger<Concurrent> for T
where
    T: MultiThreadedTrigger,
{
    unsafe fn dispatch_on_trigger(
        processor_ptr: *mut c_void,
        context_ptr: *mut minifi_process_context,
        session_ptr: *mut minifi_process_session,
    ) -> minifi_status {
        unsafe {
            let processor = &*(processor_ptr as *const T);
            let mut context = CffiProcessContext::new(context_ptr);
            let mut session = CffiProcessSession::new(session_ptr);
            match processor.trigger(&mut context, &mut session) {
                Ok(OnTriggerResult::Ok) => minifi_status_MINIFI_STATUS_SUCCESS,
                Ok(OnTriggerResult::Yield) => minifi_status_MINIFI_STATUS_PROCESSOR_YIELD,
                Err(minifi_error) => {
                    processor.log(
                        LogLevel::Error,
                        format_args!("Error during trigger {}", minifi_error),
                    );
                    minifi_error.to_status()
                }
            }
        }
    }
}

impl<T> DispatchOnTrigger<Exclusive> for T
where
    T: SingleThreadedTrigger,
{
    unsafe fn dispatch_on_trigger(
        processor_ptr: *mut c_void,
        context_ptr: *mut minifi_process_context,
        session_ptr: *mut minifi_process_session,
    ) -> minifi_status {
        unsafe {
            let processor = &mut *(processor_ptr as *mut T);
            let mut context = CffiProcessContext::new(context_ptr);
            let mut session = CffiProcessSession::new(session_ptr);
            match processor.trigger(&mut context, &mut session) {
                Ok(OnTriggerResult::Ok) => minifi_status_MINIFI_STATUS_SUCCESS,
                Ok(OnTriggerResult::Yield) => minifi_status_MINIFI_STATUS_PROCESSOR_YIELD,
                Err(error_code) => error_code.to_status(),
            }
        }
    }
}

pub struct RawProcessorDefinition<T>
where
    T: RawProcessor + DispatchOnTrigger<T::Threading>,
{
    name: &'static str,
    description_text: &'static str,
    input_requirement: ProcessorInputRequirement,
    supports_dynamic_properties: bool,
    supports_dynamic_relationships: bool,

    c_output_attributes: COutputAttributes,
    c_relationships: Vec<minifi_relationship_definition>,
    c_properties: CProperties,

    _phantom: std::marker::PhantomData<T>,
}

impl<T> RawProcessorDefinition<T>
where
    T: RawProcessor<LoggerType = CffiLogger> + DispatchOnTrigger<T::Threading>,
{
    pub fn new(
        name: &'static str,
        description_text: &'static str,
        input_requirement: ProcessorInputRequirement,
        supports_dynamic_properties: bool,
        supports_dynamic_relationships: bool,
        output_attributes: &'static [OutputAttribute],
        relationships: &'static [Relationship],
        properties: &'static [Property],
    ) -> Self {
        let c_relationships = Relationship::create_c_vec(relationships);
        let c_properties = Property::create_c_properties(properties);
        let c_output_attributes = COutputAttributes::new(output_attributes);

        Self {
            name,
            description_text,
            input_requirement,
            supports_dynamic_properties,
            supports_dynamic_relationships,
            c_output_attributes,
            c_relationships,
            c_properties,
            _phantom: std::marker::PhantomData,
        }
    }

    unsafe extern "C" fn create_processor(metadata: minifi_processor_metadata) -> *mut c_void {
        let logger = CffiLogger::new(metadata.logger);
        let processor = Box::new(T::new(logger));
        Box::into_raw(processor) as *mut c_void
    }

    unsafe extern "C" fn destroy_processor(processor_ptr: *mut c_void) {
        unsafe {
            if !processor_ptr.is_null() {
                let _ = Box::from_raw(processor_ptr as *mut T);
            }
        }
    }

    unsafe extern "C" fn trigger_processor(
        processor_ptr: *mut c_void,
        context_ptr: *mut minifi_process_context,
        session_ptr: *mut minifi_process_session,
    ) -> minifi_status {
        unsafe {
            <T as DispatchOnTrigger<T::Threading>>::dispatch_on_trigger(
                processor_ptr,
                context_ptr,
                session_ptr,
            )
        }
    }

    unsafe extern "C" fn schedule_processor(
        processor_ptr: *mut c_void,
        context_ptr: *mut minifi_process_context,
    ) -> minifi_status {
        unsafe {
            let processor = &mut *(processor_ptr as *mut T);
            let context = CffiProcessContext::new(context_ptr);
            match processor.schedule(&context) {
                Ok(_) => 0,
                Err(error_code) => {
                    processor.log(
                        LogLevel::Error,
                        format_args!("Error during schedule: {}", error_code),
                    );
                    error_code.to_status()
                }
            }
        }
    }

    unsafe extern "C" fn unschedule_processor(processor_ptr: *mut c_void) {
        unsafe {
            let processor = &mut *(processor_ptr as *mut T);
            processor.unschedule();
        }
    }
}

#[derive(Debug)]
pub struct ProcessorClassDefinition<'a> {
    inner: minifi_processor_class_definition,
    _marker: std::marker::PhantomData<&'a ()>,
}

impl<'a> ProcessorClassDefinition<'a> {
    pub(crate) fn new(inner: minifi_processor_class_definition) -> Self {
        Self {
            inner,
            _marker: std::marker::PhantomData,
        }
    }

    pub unsafe fn as_raw(&self) -> minifi_processor_class_definition {
        self.inner
    }
}

pub trait DynRawProcessorDefinition {
    fn class_description(&'_ self) -> ProcessorClassDefinition<'_>;
}

impl<T> DynRawProcessorDefinition for RawProcessorDefinition<T>
where
    T: RawProcessor<LoggerType = CffiLogger> + DispatchOnTrigger<T::Threading>,
{
    fn class_description(&'_ self) -> ProcessorClassDefinition<'_> {
        unsafe {
            ProcessorClassDefinition::new(minifi_processor_class_definition {
                full_name: self.name.as_minifi_c_type(),
                description: self.description_text.as_minifi_c_type(),
                properties_count: self.c_properties.len(),
                properties_ptr: self.c_properties.get_ptr(),
                dynamic_properties_count: 0,
                dynamic_properties_ptr: ptr::null(),
                relationships_count: self.c_relationships.len(),
                relationships_ptr: self.c_relationships.as_ptr(),
                output_attributes_count: self.c_output_attributes.len(),
                output_attributes_ptr: self.c_output_attributes.get_ptr(),
                supports_dynamic_properties: self.supports_dynamic_properties,
                supports_dynamic_relationships: self.supports_dynamic_relationships,
                input_requirement: self.input_requirement.as_minifi_c_type(),
                is_single_threaded: T::Threading::IS_EXCLUSIVE,
                callbacks: minifi_processor_callbacks {
                    create: Some(Self::create_processor),
                    destroy: Some(Self::destroy_processor),
                    trigger: Some(Self::trigger_processor),
                    schedule: Some(Self::schedule_processor),
                    unschedule: Some(Self::unschedule_processor),
                },
            })
        }
    }
}

pub trait RawRegisterableProcessor {
    fn get_definition() -> Box<dyn DynRawProcessorDefinition>;
}

impl<Implementation, Kind: 'static, Threading> RawRegisterableProcessor
    for Processor<Implementation, Kind, Threading, CffiLogger>
where
    Threading: ThreadingModel + 'static,
    Implementation: Schedule
        + ComponentIdentifier
        + ProcessorDefinition
        + 'static,
    Processor<Implementation, Kind, Threading, CffiLogger>:
        RawProcessor<Threading = Threading, LoggerType = CffiLogger> + DispatchOnTrigger<Threading>,
{
    fn get_definition() -> Box<dyn DynRawProcessorDefinition> {
        Box::new(RawProcessorDefinition::<
            Processor<Implementation, Kind, Threading, CffiLogger>,
        >::new(
            Implementation::CLASS_NAME,
            Implementation::DESCRIPTION,
            Implementation::INPUT_REQUIREMENT,
            Implementation::SUPPORTS_DYNAMIC_PROPERTIES,
            Implementation::SUPPORTS_DYNAMIC_RELATIONSHIPS,
            Implementation::OUTPUT_ATTRIBUTES,
            Implementation::RELATIONSHIPS,
            Implementation::PROPERTIES,
        ))
    }
}
