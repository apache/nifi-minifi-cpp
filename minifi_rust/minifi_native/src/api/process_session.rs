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

use crate::MinifiError;
use crate::api::flow_file::FlowFile;
pub trait InputStream: std::io::BufRead + Send + std::fmt::Debug {}
pub trait OutputStream: std::io::Write + Send + std::fmt::Debug {}
impl<T: std::io::Write + Send + std::fmt::Debug> OutputStream for T {}
impl<T: std::io::BufRead + Send + std::fmt::Debug> InputStream for T {}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub enum IoState {
    Ok,
    Cancel,
}

pub trait ProcessSession {
    type FlowFile: FlowFile;

    fn create(&mut self) -> Result<Self::FlowFile, MinifiError>;
    fn get(&mut self) -> Option<Self::FlowFile>;
    fn transfer(&self, flow_file: Self::FlowFile, relationship: &str) -> Result<(), MinifiError>;
    fn remove(&mut self, flow_file: Self::FlowFile) -> Result<(), MinifiError>;

    fn set_attribute(
        &self,
        flow_file: &mut Self::FlowFile,
        attr_key: &str,
        attr_value: &str,
    ) -> Result<(), MinifiError>;
    fn get_attribute(&self, flow_file: &Self::FlowFile, attr_key: &str) -> Option<String>;
    fn on_attributes<F: FnMut(&str, &str)>(
        &self,
        flow_file: &Self::FlowFile,
        process_attr: F,
    ) -> bool;

    fn write(&self, flow_file: &Self::FlowFile, data: &[u8]) -> Result<(), MinifiError>;
    fn write_from_stream<'a>(
        &self,
        flow_file: &Self::FlowFile,
        stream: Box<dyn std::io::Read + 'a>,
    ) -> Result<(), MinifiError>;

    fn write_stream<F, R>(&self, flow_file: &Self::FlowFile, callback: F) -> Result<R, MinifiError>
    where
        F: FnOnce(&mut dyn OutputStream) -> Result<(R, IoState), MinifiError>;

    fn read(&self, flow_file: &Self::FlowFile) -> Option<Vec<u8>>;
    fn read_stream<F, R>(&self, flow_file: &Self::FlowFile, callback: F) -> Result<R, MinifiError>
    where
        F: FnOnce(&mut dyn InputStream) -> Result<R, MinifiError>;

    fn get_flow_file_id(&self, flow_file: &Self::FlowFile) -> Result<String, MinifiError>;
}
