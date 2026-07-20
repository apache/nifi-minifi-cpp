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

pub trait ControllerServiceApi {
    const INTERFACE_NAME: &'static str;
}

#[macro_export]
macro_rules! impl_interface_fqn {
    ($trait_name:ident) => {
        impl ControllerServiceApi for dyn $trait_name {
            const INTERFACE_NAME: &'static str =
                concat!(module_path!(), "::", stringify!($trait_name));
        }
    };
}

#[derive(Debug)]
pub struct ProvidedInterface<T> {
    pub name: &'static str,
    pub cast: fn(&T) -> *mut std::ffi::c_void,
}

#[macro_export]
macro_rules! create_provided_interface {
    ($trait_type:ty) => {
        ProvidedInterface {
            name: <$trait_type as ControllerServiceApi>::INTERFACE_NAME,
            cast: |instance| {
                let trait_ref: &$trait_type = instance;
                let boxed_ref: Box<&$trait_type> = Box::new(trait_ref);
                Box::into_raw(boxed_ref) as *mut std::ffi::c_void
            },
        }
    };
}
