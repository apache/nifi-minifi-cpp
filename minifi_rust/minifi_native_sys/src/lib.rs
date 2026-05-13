// minifi-sys/src/lib.rs

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

//! This crate provides raw, unsafe FFI bindings for the Apache NiFi MiNiFi C API.
//!
//! The bindings are generated automatically by `bindgen` from the `minifi-c.h` header.

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
