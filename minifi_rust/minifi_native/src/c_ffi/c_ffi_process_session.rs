use super::c_ffi_flow_file::CffiFlowFile;
use crate::MinifiError;
use crate::api::process_session::{IoState, OutputStream};
use crate::api::{InputStream, ProcessSession};
use crate::c_ffi::c_ffi_primitives::{ConvertMinifiStringView, StringView};
use crate::c_ffi::c_ffi_streams::{CffiInputStream, CffiOutputStream};
use minifi_native_sys::{
    minifi_input_stream, minifi_input_stream_read, minifi_input_stream_size,
    minifi_io_status_MINIFI_IO_CANCEL, minifi_io_status_MINIFI_IO_ERROR, minifi_output_stream,
    minifi_output_stream_write, minifi_process_session, minifi_process_session_create,
    minifi_process_session_get, minifi_process_session_get_flow_file_attribute,
    minifi_process_session_get_flow_file_attributes, minifi_process_session_get_flow_file_id,
    minifi_process_session_read, minifi_process_session_remove,
    minifi_process_session_set_flow_file_attribute, minifi_process_session_transfer,
    minifi_process_session_write, minifi_status_MINIFI_STATUS_SUCCESS, minifi_string_view,
};
use std::ffi::{CString, c_void};
use std::io::Read;
use std::num::NonZeroU32;
use std::os::raw::c_char;

const _: () = {
    if minifi_status_MINIFI_STATUS_SUCCESS != 0 {
        panic!("minifi_status_MINIFI_STATUS_SUCCESS expected to be 0");
    }
};

pub struct CffiProcessSession<'a> {
    ptr: *mut minifi_process_session,
    // The lifetime ensures the session cannot outlive the `trigger` call.
    _lifetime: std::marker::PhantomData<&'a ()>,
}

impl<'a> CffiProcessSession<'a> {
    pub fn new(ptr: *mut minifi_process_session) -> Self {
        Self {
            ptr,
            _lifetime: std::marker::PhantomData,
        }
    }

    fn write_in_batches<F>(
        &self,
        flow_file: &CffiFlowFile,
        produce_batch: F,
    ) -> Result<(), MinifiError>
    where
        F: FnMut(&mut [u8]) -> Option<usize>,
    {
        unsafe {
            struct State<'b, F: FnMut(&mut [u8]) -> Option<usize>> {
                callback: F,
                buffer: &'b mut [u8],
            }

            let mut buffer = [0u8; 8192];
            let mut state = State {
                callback: produce_batch,
                buffer: &mut buffer,
            };

            unsafe extern "C" fn cb<F: FnMut(&mut [u8]) -> Option<usize>>(
                user_ctx: *mut c_void,
                output_stream: *mut minifi_output_stream,
            ) -> i64 {
                unsafe {
                    let state = &mut *(user_ctx as *mut State<F>);
                    let mut overall_writes = 0;

                    // The user fills our provided buffer
                    while let Some(n) = (state.callback)(state.buffer) {
                        let written = minifi_output_stream_write(
                            output_stream,
                            state.buffer.as_ptr() as *const c_char,
                            n,
                        );

                        if written < 0 {
                            return minifi_io_status_MINIFI_IO_ERROR;
                        }
                        overall_writes += written;
                    }
                    overall_writes
                }
            }

            match minifi_process_session_write(
                self.ptr,
                flow_file.get_ptr(),
                Some(cb::<F>),
                &mut state as *mut _ as *mut c_void,
            ) {
                #[allow(non_upper_case_globals)]
                minifi_status_MINIFI_STATUS_SUCCESS => Ok(()),
                error_code => Err(MinifiError::StatusError((
                    "minifi_process_session_write".into(),
                    NonZeroU32::new_unchecked(error_code),
                ))),
            }
        }
    }
}

impl<'a> ProcessSession for CffiProcessSession<'a> {
    type FlowFile = CffiFlowFile<'a>; // FlowFile shouldn't outlive the Session

    fn create(&mut self) -> Result<Self::FlowFile, MinifiError> {
        let ff_ptr = unsafe { minifi_process_session_create(self.ptr, std::ptr::null_mut()) };
        if ff_ptr.is_null() {
            Err(MinifiError::UnknownError)
        } else {
            Ok(CffiFlowFile::new(ff_ptr))
        }
    }

    fn get(&mut self) -> Option<Self::FlowFile> {
        let ff_ptr = unsafe { minifi_process_session_get(self.ptr) };
        if ff_ptr.is_null() {
            None
        } else {
            Some(CffiFlowFile::new(ff_ptr))
        }
    }

    fn transfer(&self, flow_file: Self::FlowFile, relationship: &str) -> Result<(), MinifiError> {
        let c_relationship = CString::new(relationship)?;
        unsafe {
            match minifi_process_session_transfer(
                self.ptr,
                flow_file.get_ptr(),
                minifi_string_view {
                    data: c_relationship.as_ptr(),
                    length: c_relationship.as_bytes().len(),
                },
            ) {
                #[allow(non_upper_case_globals)]
                minifi_status_MINIFI_STATUS_SUCCESS => Ok(()),
                err_code => Err(MinifiError::StatusError((
                    "minifi_process_session_transfer".into(),
                    NonZeroU32::new_unchecked(err_code),
                ))),
            }
        }
    }

    fn remove(&mut self, flow_file: Self::FlowFile) -> Result<(), MinifiError> {
        unsafe {
            match minifi_process_session_remove(self.ptr, flow_file.get_ptr()) {
                #[allow(non_upper_case_globals)]
                minifi_status_MINIFI_STATUS_SUCCESS => Ok(()),
                err_code => Err(MinifiError::StatusError((
                    "minifi_process_session_remove".into(),
                    NonZeroU32::new_unchecked(err_code),
                ))),
            }
        }
    }

    fn set_attribute(
        &self,
        flow_file: &mut Self::FlowFile,
        attr_key: &str,
        attr_value: &str,
    ) -> Result<(), MinifiError> {
        unsafe {
            let attr_key_string_view = StringView::new(attr_key);
            let attr_value_string_view = StringView::new(attr_value);
            match minifi_process_session_set_flow_file_attribute(
                self.ptr,
                flow_file.get_ptr(),
                attr_key_string_view.as_raw(),
                &attr_value_string_view.as_raw(),
            ) {
                #[allow(non_upper_case_globals)]
                minifi_status_MINIFI_STATUS_SUCCESS => Ok(()),
                err_code => Err(MinifiError::StatusError((
                    format!(
                        "minifi_process_session_set_flow_file_attribute({}, {})",
                        attr_key, attr_value
                    )
                    .into(),
                    NonZeroU32::new_unchecked(err_code),
                ))),
            }
        }
    }

    fn get_attribute(&self, flow_file: &Self::FlowFile, attr_key: &str) -> Option<String> {
        let mut attr_value: Option<String> = None;
        unsafe {
            unsafe extern "C" fn cb(
                rs_attr_value: *mut c_void,
                minifi_attr_value: minifi_string_view,
            ) {
                unsafe {
                    let result_target = &mut *(rs_attr_value as *mut Option<String>);
                    *result_target = minifi_attr_value.as_string().ok()
                }
            }

            let attr_key_string_view = StringView::new(attr_key);
            minifi_process_session_get_flow_file_attribute(
                self.ptr,
                flow_file.get_ptr(),
                attr_key_string_view.as_raw(),
                Some(cb),
                &mut attr_value as *mut _ as *mut c_void,
            );
        }
        attr_value
    }

    fn on_attributes<F: FnMut(&str, &str)>(
        &self,
        flow_file: &Self::FlowFile,
        process_attr: F,
    ) -> bool {
        struct OnAttrHelper<F: FnMut(&str, &str)> {
            result: bool,
            process_attr: F,
        }

        let mut on_attr_helper = OnAttrHelper {
            result: true,
            process_attr,
        };

        unsafe extern "C" fn get_attributes_callback<'b, F: FnMut(&str, &str)>(
            user_ctx: *mut c_void,
            minifi_attr_key: minifi_string_view,
            minifi_attr_val: minifi_string_view,
        ) {
            unsafe {
                let helper = &mut *(user_ctx as *mut OnAttrHelper<F>);
                let attr_key = minifi_attr_key.as_str();
                let attr_value = minifi_attr_val.as_str();
                if attr_key.is_err() || attr_value.is_err() {
                    helper.result = false;
                    return;
                }
                (helper.process_attr)(attr_key.unwrap(), attr_value.unwrap());
            }
        }

        unsafe {
            minifi_process_session_get_flow_file_attributes(
                self.ptr,
                flow_file.get_ptr(),
                Some(get_attributes_callback::<F>),
                &mut on_attr_helper as *mut _ as *mut c_void,
            )
        }
        on_attr_helper.result
    }

    fn write(&self, flow_file: &Self::FlowFile, data: &[u8]) -> Result<(), MinifiError> {
        let mut dt: Option<&[u8]> = Some(data);
        unsafe {
            unsafe extern "C" fn cb(
                user_ctx: *mut c_void,
                output_stream: *mut minifi_output_stream,
            ) -> i64 {
                unsafe {
                    let result_target = &mut *(user_ctx as *mut Option<&[u8]>);
                    if result_target.is_none() {
                        return minifi_io_status_MINIFI_IO_ERROR;
                    }

                    minifi_output_stream_write(
                        output_stream,
                        result_target.unwrap().as_ptr() as *const c_char,
                        result_target.unwrap().len(),
                    )
                }
            }

            match minifi_process_session_write(
                self.ptr,
                flow_file.get_ptr(),
                Some(cb),
                &mut dt as *mut _ as *mut c_void,
            ) {
                #[allow(non_upper_case_globals)]
                minifi_status_MINIFI_STATUS_SUCCESS => Ok(()),
                err_code => Err(MinifiError::StatusError((
                    "minifi_process_session_write".into(),
                    NonZeroU32::new_unchecked(err_code),
                ))),
            }
        }
    }

    fn write_from_stream<'b>(
        &self,
        flow_file: &Self::FlowFile,
        mut stream: Box<dyn Read + 'b>,
    ) -> Result<(), MinifiError> {
        self.write_in_batches(flow_file, |buffer| {
            match stream.read(buffer) {
                Ok(0) => None, // EOF
                Ok(n) => Some(n),
                Err(_e) => None, // Signal failure/EOF
            }
        })
    }

    fn write_stream<F, R>(&self, flow_file: &Self::FlowFile, callback: F) -> Result<R, MinifiError>
    where
        F: FnOnce(&mut dyn OutputStream) -> Result<(R, IoState), MinifiError>,
    {
        struct WriteCallbackCtx<F, R> {
            callback: Option<F>,
            result: Option<Result<R, MinifiError>>,
        }

        let mut ctx = WriteCallbackCtx {
            callback: Some(callback),
            result: None,
        };

        unsafe extern "C" fn write_cb<F, R>(
            user_data: *mut c_void,
            stream_ptr: *mut minifi_output_stream,
        ) -> i64
        where
            F: FnOnce(&mut dyn OutputStream) -> Result<(R, IoState), MinifiError>,
        {
            unsafe {
                let ctx = &mut *(user_data as *mut WriteCallbackCtx<F, R>);

                let mut writer = CffiOutputStream::new(stream_ptr);

                if let Some(cb) = ctx.callback.take() {
                    let (cb_result, state) = match cb(&mut writer) {
                        Ok((f, b)) => (Ok(f), b),
                        Err(e) => (Err(e), IoState::Cancel),
                    };
                    let is_err = cb_result.is_err();
                    ctx.result = Some(cb_result);
                    if is_err {
                        return minifi_io_status_MINIFI_IO_ERROR;
                    }
                    if state == IoState::Cancel {
                        return minifi_io_status_MINIFI_IO_CANCEL;
                    }
                }

                writer.written_bytes() as i64
            }
        }

        unsafe {
            let session_status = minifi_process_session_write(
                self.ptr,
                flow_file.get_ptr(),
                Some(write_cb::<F, R>),
                &mut ctx as *mut _ as *mut c_void,
            );
            if session_status != minifi_status_MINIFI_STATUS_SUCCESS {
                return Err(MinifiError::StatusError((
                    "minifi_process_session_write".into(),
                    NonZeroU32::new_unchecked(session_status),
                )));
            }
        }

        ctx.result
            .expect("Agent returned with success, so ctx.result should be set")
    }

    fn read(&self, flow_file: &Self::FlowFile) -> Option<Vec<u8>> {
        let mut output: Option<Vec<u8>> = None;
        unsafe {
            unsafe extern "C" fn cb(
                output_option: *mut c_void,
                input_stream: *mut minifi_input_stream,
            ) -> i64 {
                unsafe {
                    let result_target = &mut *(output_option as *mut Option<Vec<u8>>);

                    let stream_size = minifi_input_stream_size(input_stream);
                    if stream_size == 0 {
                        *result_target = None;
                        return 0;
                    }
                    let mut buffer: Vec<u8> = Vec::with_capacity(stream_size);

                    let bytes_read = minifi_input_stream_read(
                        input_stream,
                        buffer.as_mut_ptr() as *mut c_char,
                        stream_size,
                    );

                    if bytes_read < 0 {
                        *result_target = None;
                        return bytes_read;
                    }

                    buffer.set_len(bytes_read as usize);

                    *result_target = Some(buffer);
                    bytes_read
                }
            }

            minifi_process_session_read(
                self.ptr,
                flow_file.get_ptr(),
                Some(cb),
                &mut output as *mut _ as *mut c_void,
            );
        }
        output
    }

    fn read_stream<F, R>(&self, flow_file: &Self::FlowFile, callback: F) -> Result<R, MinifiError>
    where
        F: FnOnce(&mut dyn InputStream) -> Result<R, MinifiError>,
    {
        unsafe {
            struct CallbackCtx<F, R> {
                callback: Option<F>,
                result: Option<Result<R, MinifiError>>,
            }
            let mut ctx = CallbackCtx {
                callback: Some(callback),
                result: None,
            };

            unsafe extern "C" fn read_cb<F, R>(
                user_data: *mut c_void,
                stream_ptr: *mut minifi_input_stream,
            ) -> i64
            where
                F: FnOnce(&mut dyn InputStream) -> Result<R, MinifiError>,
            {
                unsafe {
                    let ctx = &mut *(user_data as *mut CallbackCtx<F, R>);

                    let mut reader = CffiInputStream::new(stream_ptr);

                    if let Some(cb) = ctx.callback.take() {
                        ctx.result = Some(cb(&mut reader));
                    }

                    reader.total_bytes_read() as i64
                }
            }

            minifi_process_session_read(
                self.ptr,
                flow_file.get_ptr(),
                Some(read_cb::<F, R>),
                &mut ctx as *mut _ as *mut c_void,
            );

            ctx.result
                .expect("Agent returned with success, so ctx.result should be set")
        }
    }

    fn read_in_batches<F>(
        &self,
        flow_file: &Self::FlowFile,
        batch_size: usize,
        process_batch: F,
    ) -> Result<(), MinifiError>
    where
        F: FnMut(&[u8]) -> Result<(), MinifiError>,
    {
        struct BatchReadHelper<F>
        where
            F: FnMut(&[u8]) -> Result<(), MinifiError>,
        {
            batch_size: usize,
            process_batch: F,
        }

        let mut batch_helper = BatchReadHelper {
            batch_size,
            process_batch,
        };
        unsafe {
            unsafe extern "C" fn cb<F>(
                output_option: *mut c_void,
                input_stream: *mut minifi_input_stream,
            ) -> i64
            where
                F: FnMut(&[u8]) -> Result<(), MinifiError>,
            {
                unsafe {
                    let batch_helper = &mut *(output_option as *mut BatchReadHelper<F>);

                    let mut remaining_size = minifi_input_stream_size(input_stream);
                    let mut overall_read = 0;
                    while remaining_size > 0 {
                        let read_size = remaining_size.min(batch_helper.batch_size);
                        let mut buffer: Vec<u8> = Vec::with_capacity(read_size);

                        let bytes_read = minifi_input_stream_read(
                            input_stream,
                            buffer.as_mut_ptr() as *mut c_char,
                            read_size,
                        );
                        if bytes_read < 0 || bytes_read > read_size as i64 {
                            return minifi_io_status_MINIFI_IO_ERROR;
                        }

                        buffer.set_len(bytes_read as usize);

                        match (batch_helper.process_batch)(&*buffer) {
                            Ok(_) => {}
                            Err(err) => {
                                eprintln!("Error during read_in_batch {:?}", err);
                                return minifi_io_status_MINIFI_IO_ERROR;
                            }
                        }
                        remaining_size -= bytes_read as usize;
                        overall_read += bytes_read;
                    }
                    overall_read
                }
            }

            match minifi_process_session_read(
                self.ptr,
                flow_file.get_ptr(),
                Some(cb::<F>),
                &mut batch_helper as *mut _ as *mut c_void,
            ) {
                #[allow(non_upper_case_globals)]
                minifi_status_MINIFI_STATUS_SUCCESS => Ok(()),
                status_code => Err(MinifiError::StatusError((
                    "minifi_process_session_read".into(),
                    NonZeroU32::new_unchecked(status_code),
                ))),
            }
        }
    }

    fn get_flow_file_id(&self, flow_file: &Self::FlowFile) -> Result<String, MinifiError> {
        let mut attr_value: Option<String> = None;
        unsafe {
            unsafe extern "C" fn cb(
                rs_flow_file_id: *mut c_void,
                minifi_flow_file_id: minifi_string_view,
            ) {
                unsafe {
                    let result_target = &mut *(rs_flow_file_id as *mut Option<String>);
                    *result_target = minifi_flow_file_id.as_string().ok()
                }
            }

            minifi_process_session_get_flow_file_id(
                self.ptr,
                flow_file.get_ptr(),
                Some(cb),
                &mut attr_value as *mut _ as *mut c_void,
            );
        }
        attr_value.ok_or(MinifiError::UnknownError)
    }
}
