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

#[macro_export]
macro_rules! impl_with_attributes {
    ($name:ident $(<$lt:lifetime>)?) => {
        impl $(<$lt>)? $name $(<$lt>)? {
            #[must_use]
            pub fn with_attribute(
                mut self,
                key: impl Into<std::borrow::Cow<'static, str>>,
                value: impl Into<std::borrow::Cow<'static, str>>,
            ) -> Self {
                self.attributes_to_add.push((key.into(), value.into()).into());
                self
            }

            #[must_use]
            pub fn with_attributes<K, V>(
                mut self,
                attributes: impl IntoIterator<Item = (K, V)>
            ) -> Self
            where
                K: Into<std::borrow::Cow<'static, str>>,
                V: Into<std::borrow::Cow<'static, str>>,
            {
                self.attributes_to_add
                    .extend(attributes.into_iter().map(|(k, v)| (k.into(), v.into()).into()));
                self
            }
        }
    };
}
