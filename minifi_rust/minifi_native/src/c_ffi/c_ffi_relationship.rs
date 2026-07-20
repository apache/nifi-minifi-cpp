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

use super::c_ffi_primitives::StaticStrAsMinifiCStr;
use crate::Relationship;
use minifi_native_sys::minifi_relationship_definition;

impl Relationship {
    pub(crate) fn create_c_vec(relationships: &[Self]) -> Vec<minifi_relationship_definition> {
        relationships
            .iter()
            .map(|r| minifi_relationship_definition {
                name: r.name.as_minifi_c_type(),
                description: r.description.as_minifi_c_type(),
            })
            .collect()
    }
}
