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

use minifi_native::OutputAttribute;

pub(crate) const FILENAME_OUTPUT_ATTRIBUTE: OutputAttribute = OutputAttribute {
    name: "filename",
    relationships: &["success"],
    description: "The filename is set to the name of the file on disk",
};

pub(crate) const ABSOLUTE_PATH_OUTPUT_ATTRIBUTE: OutputAttribute = OutputAttribute {
    name: "absolute.path",
    relationships: &["success"],
    description: "The full/absolute path from where a file was picked up. The current 'path' attribute is still populated, but may be a relative path",
};
