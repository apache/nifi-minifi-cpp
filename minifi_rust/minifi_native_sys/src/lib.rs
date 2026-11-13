// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

//! This crate provides raw, unsafe FFI bindings for the Apache NiFi MiNiFi C API.
//!
//! The bindings are generated automatically by `bindgen` from the `minifi-api.h` header.

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
