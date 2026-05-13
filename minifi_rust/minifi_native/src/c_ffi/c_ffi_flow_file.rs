use crate::api::FlowFile;
use minifi_native_sys::MinifiFlowFile;

pub struct CffiFlowFile<'a> {
    ptr: *mut MinifiFlowFile,
    _lifetime: std::marker::PhantomData<&'a ()>,
}

impl CffiFlowFile<'_> {
    pub(crate) fn new(ptr: *mut MinifiFlowFile) -> Self {
        Self {
            ptr,
            _lifetime: std::marker::PhantomData,
        }
    }

    pub(crate) fn get_ptr(&self) -> *mut MinifiFlowFile {
        self.ptr
    }
}

impl FlowFile for CffiFlowFile<'_> {}
