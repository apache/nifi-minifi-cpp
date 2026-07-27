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

use std::fmt::Formatter;

pub enum Content<'a> {
    Buffer(Vec<u8>),
    Stream(Box<dyn std::io::Read + 'a>),
}

impl std::fmt::Debug for Content<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            Content::Buffer(b) => f.debug_struct("Content").field("Buffer", &b).finish(),
            Content::Stream(_s) => f.debug_struct("Content::Stream").finish(),
        }
    }
}

impl From<Vec<u8>> for Content<'_> {
    fn from(v: Vec<u8>) -> Self {
        Content::Buffer(v)
    }
}

impl From<String> for Content<'_> {
    fn from(s: String) -> Self {
        Content::Buffer(s.into_bytes())
    }
}
