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

use proc_macro::TokenStream;
use quote::quote;
use syn::{DeriveInput, ItemTrait, parse_macro_input};

#[proc_macro_derive(ComponentIdentifier)]
pub fn derive_component_identifier(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;
    let name_str = name.to_string();

    let expanded = quote! {
        impl ::minifi_native::ComponentIdentifier for #name {
            const CLASS_NAME: &'static str = concat!(module_path!(), "::", #name_str);
            const GROUP_NAME: &'static str = env!("CARGO_PKG_NAME");
            const VERSION: &'static str = env!("CARGO_PKG_VERSION");
        }

        impl ::minifi_native::ProvidesPropertyConstraint for #name {
            const PROPERTY_CONSTRAINT: Option<::minifi_native::PropertyConstraints> =
                Some(::minifi_native::PropertyConstraints::ControllerService(<Self as ::minifi_native::ComponentIdentifier>::CLASS_NAME));
        }
    };

    TokenStream::from(expanded)
}

#[proc_macro_attribute]
pub fn controller_service_api(_attr: TokenStream, item: TokenStream) -> TokenStream {
    let input = parse_macro_input!(item as ItemTrait);
    let name = &input.ident;
    let name_str = name.to_string();

    let expanded = quote! {
        #input

        impl ::minifi_native::ControllerServiceApi for dyn #name {
            const INTERFACE_NAME: &'static str = concat!(module_path!(), "::", #name_str);
        }

        impl ::minifi_native::ProvidesPropertyConstraint for dyn #name {
            const PROPERTY_CONSTRAINT: Option<::minifi_native::PropertyConstraints> =
                Some(::minifi_native::PropertyConstraints::ControllerService(<Self as ::minifi_native::ControllerServiceApi>::INTERFACE_NAME));
        }
    };

    TokenStream::from(expanded)
}

#[proc_macro_derive(PropertyType)]
pub fn derive_property_type(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;

    let expanded = quote! {
        impl ::minifi_native::PropertyType for #name {
            type Output = #name;
            const EXPECTED_CONSTRAINTS: Option<::minifi_native::PropertyConstraints> =
                Some(::minifi_native::PropertyConstraints::AllowedValues(
                    <#name as ::strum::VariantNames>::VARIANTS
                ));
            fn parse(s: &str) -> Result<Self::Output, ::minifi_native::MinifiError> {
                s.parse::<#name>().map_err(Into::into)
            }
        }

        impl ::minifi_native::ProvidesPropertyConstraint for #name {
            const PROPERTY_CONSTRAINT: Option<::minifi_native::PropertyConstraints> = <Self as ::minifi_native::PropertyType>::EXPECTED_CONSTRAINTS;
        }
    };

    TokenStream::from(expanded)
}
