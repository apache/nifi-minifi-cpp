// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

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

unsafe fn write_all_to_output_stream(
    output_stream: *mut minifi_output_stream,
    data: &[u8],
) -> Result<i64, i64> {
    let mut offset = 0;
    while offset < data.len() {
        let remaining = data.len() - offset;
        let written = unsafe {
            minifi_output_stream_write(
                output_stream,
                data.as_ptr().add(offset) as *const c_char,
                remaining,
            )
        };
        if written < 0 {
            return Err(written);
        }
        if written == 0 {
            return Err(minifi_io_status_MINIFI_IO_ERROR); // To avoid spinning
        }
        let written_usize = written as usize;
        assert!(written_usize <= remaining);
        offset += written_usize;
    }
    Ok(data.len() as i64)
}

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
        F: FnMut(&mut [u8]) -> Result<Option<usize>, MinifiError>,
    {
        unsafe {
            struct State<'b, F>
            where
                F: FnMut(&mut [u8]) -> Result<Option<usize>, MinifiError>,
            {
                callback: F,
                buffer: &'b mut [u8],
                error: Option<MinifiError>,
            }

            let mut buffer = [0u8; 8192];
            let mut state = State {
                callback: produce_batch,
                buffer: &mut buffer,
                error: None,
            };

            unsafe extern "C" fn cb<F>(
                user_ctx: *mut c_void,
                output_stream: *mut minifi_output_stream,
            ) -> i64
            where
                F: FnMut(&mut [u8]) -> Result<Option<usize>, MinifiError>,
            {
                unsafe {
                    let state = &mut *(user_ctx as *mut State<F>);
                    let mut overall_writes = 0;

                    loop {
                        let n = match (state.callback)(state.buffer) {
                            Ok(Some(n)) => n,
                            Ok(None) => break, // EOF
                            Err(err) => {
                                state.error = Some(err);
                                return minifi_io_status_MINIFI_IO_ERROR;
                            }
                        };

                        match write_all_to_output_stream(output_stream, &state.buffer[..n]) {
                            Ok(written) => overall_writes += written,
                            Err(err) => return err,
                        }
                    }
                    overall_writes
                }
            }

            let status = minifi_process_session_write(
                self.ptr,
                flow_file.get_ptr(),
                Some(cb::<F>),
                &mut state as *mut _ as *mut c_void,
            );

            if let Some(err) = state.error.take() {
                return Err(err);
            }

            match status {
                #[allow(non_upper_case_globals)]
                minifi_status_MINIFI_STATUS_SUCCESS => Ok(()),
                error_code => Err(MinifiError::StatusError((
                    "minifi_process_session_write".into(),
                    NonZeroU32::new_unchecked(error_code), // SAFETY we checked against 0 (minifi_status_MINIFI_STATUS_SUCCESS)
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
                    NonZeroU32::new_unchecked(err_code), // SAFETY we checked against 0 (minifi_status_MINIFI_STATUS_SUCCESS)
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
                    NonZeroU32::new_unchecked(err_code), // SAFETY we checked against 0 (minifi_status_MINIFI_STATUS_SUCCESS)
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
                    NonZeroU32::new_unchecked(err_code), // SAFETY we checked against 0 (minifi_status_MINIFI_STATUS_SUCCESS)
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
                    *result_target = minifi_attr_value.as_str().map(|s| s.to_owned()).ok()
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

    fn for_each_attribute<F: FnMut(&str, &str)>(
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

        unsafe extern "C" fn get_attributes_callback<F: FnMut(&str, &str)>(
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
                    let Some(data) = *result_target else {
                        return minifi_io_status_MINIFI_IO_ERROR;
                    };

                    write_all_to_output_stream(output_stream, data).unwrap_or_else(|err| err)
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
                    NonZeroU32::new_unchecked(err_code), // SAFETY we checked against 0 (minifi_status_MINIFI_STATUS_SUCCESS)
                ))),
            }
        }
    }

    fn write_from_stream<'b>(
        &self,
        flow_file: &Self::FlowFile,
        mut stream: Box<dyn Read + 'b>,
    ) -> Result<(), MinifiError> {
        self.write_in_batches(flow_file, |buffer| match stream.read(buffer) {
            Ok(0) => Ok(None), // EOF
            Ok(n) => Ok(Some(n)),
            Err(e) => Err(MinifiError::IoError(e)),
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

        let session_status = unsafe {
            minifi_process_session_write(
                self.ptr,
                flow_file.get_ptr(),
                Some(write_cb::<F, R>),
                &mut ctx as *mut _ as *mut c_void,
            )
        };

        // If the user closure ran, its outcome (including a deliberate
        // Ok((_, IoState::Cancel))) always wins over the C session status:
        // MINIFI_IO_CANCEL from the callback makes the session return
        // non-success even though from Rust's perspective the write succeeded.
        if let Some(result) = ctx.result.take() {
            return result;
        }

        // Callback never ran — surface the raw C status.
        if session_status != minifi_status_MINIFI_STATUS_SUCCESS {
            return Err(MinifiError::StatusError((
                "minifi_process_session_write".into(),
                unsafe { NonZeroU32::new_unchecked(session_status) }, // SAFETY we checked against 0 (minifi_status_MINIFI_STATUS_SUCCESS)
            )));
        }

        Err(MinifiError::UnknownError)
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
                        // Empty content is a legal state, not an error
                        *result_target = Some(Vec::new());
                        return 0;
                    }
                    let mut buffer: Vec<u8> = Vec::with_capacity(stream_size);

                    let mut total_read: usize = 0;
                    while total_read < stream_size {
                        let spare = stream_size - total_read;
                        let bytes_read = minifi_input_stream_read(
                            input_stream,
                            buffer.as_mut_ptr().add(total_read) as *mut c_char,
                            spare,
                        );

                        if bytes_read < 0 {
                            *result_target = None;
                            return bytes_read;
                        }
                        if bytes_read == 0 {
                            // EOF
                            break;
                        }
                        total_read += bytes_read as usize;
                    }

                    buffer.set_len(total_read);

                    *result_target = Some(buffer);
                    total_read as i64
                }
            }

            let status = minifi_process_session_read(
                self.ptr,
                flow_file.get_ptr(),
                Some(cb),
                &mut output as *mut _ as *mut c_void,
            );

            if status != minifi_status_MINIFI_STATUS_SUCCESS {
                output = None;
            }
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
                        let is_err = {
                            let outcome = cb(&mut reader);
                            let is_err = outcome.is_err();
                            ctx.result = Some(outcome);
                            is_err
                        };
                        if is_err {
                            return minifi_io_status_MINIFI_IO_ERROR;
                        }
                    } else {
                        return minifi_io_status_MINIFI_IO_ERROR;
                    }

                    reader.total_bytes_read() as i64
                }
            }

            let status = minifi_process_session_read(
                self.ptr,
                flow_file.get_ptr(),
                Some(read_cb::<F, R>),
                &mut ctx as *mut _ as *mut c_void,
            );

            if let Some(result) = ctx.result.take() {
                return result;
            }

            if status != minifi_status_MINIFI_STATUS_SUCCESS {
                return Err(MinifiError::StatusError((
                    "minifi_process_session_read".into(),
                    NonZeroU32::new_unchecked(status), // SAFETY we checked against 0 (minifi_status_MINIFI_STATUS_SUCCESS)
                )));
            }

            Err(MinifiError::UnknownError)
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
                    *result_target = minifi_flow_file_id.as_str().map(|s| s.to_owned()).ok()
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
