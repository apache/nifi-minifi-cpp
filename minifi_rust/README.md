# MiNiFi Native Rust (tech preview)

> ⚠️ **Tech Preview**
>
> Only the **Rust bindings** are a technology preview. The Rust source API
> (both the `minifi-native` safe API and the `minifi-native-sys` FFI layer)
> is still evolving, and we are **not yet committed to source-level backward
> compatibility** for the Rust bindings — expect breaking changes in the
> Rust API between MiNiFi C++ releases until the bindings are declared
> stable.
>
> Under the hood, the bindings target the **stable MiNiFi C API**, which
> **does** provide ABI backward compatibility. This means the compiled artifact
> is unaffected by Rust-side churn: an extension built against an older
> version of these bindings will keep loading into newer MiNiFi C++
> releases. You only need to rebuild against the new bindings if you want
> to pick up new Rust API features — upgrading the agent alone does not
> force a rebuild.

This project provides a safe, idiomatic, and high-performance Rust framework for building native extensions (processors) for [Apache NiFi MiNiFi C++](https://github.com/apache/nifi-minifi-cpp).

It is designed to offer a robust developer experience, allowing you to write powerful and reliable data processing components in safe Rust.

The framework completely encapsulates the unsafe C FFI (Foreign Function Interface) boundary, providing a pure Rust API that is fully mockable for unit testing.

## Project Philosophy
 - **Safety First**: Leverage Rust's compile-time guarantees to prevent common bugs like null pointers, buffer overflows, and data races.
 - **Zero-Cost Abstraction**: The safe API wrapper is designed to compile down with zero runtime overhead compared to writing raw C code.
 - **Ergonomics**: Provide a clean, idiomatic Rust API that is a pleasure to use. Developers should not need to think about unsafe code or C++ interoperability.
 - **Testability**: Every component of a processor's logic should be unit-testable in a pure Rust environment, without needing a C++ host.
 - **Cross Platform**: The library should work on all platforms that are supported by [Apache NiFi MiNiFi C++](https://github.com/apache/nifi-minifi-cpp).
   - macOS (aarch64)
   - Linux (x86_64, aarch64)
   - Windows (x86_64)

The project is structured as a Cargo workspace with a clear, layered architecture:

### [minifi-native-sys](minifi_native_sys)
Contains the raw, unsafe FFI bindings to the minifi-api.h C API.

### [minifi-native](minifi_native)
Provides the public, safe, and idiomatic Rust API. This is the crate that developers will use to build their processors.

#### API Traits
Pure Rust traits (Processor, ProcessSession, Logger, etc.) that define the abstract behavior of the MiNiFi environment.

#### Higher level API
Pure rust traits that simplify the requirements for a working processor. Pick the one that matches your processor's shape — the wrapper takes care of getting/creating the flowfile, wiring up streams, applying attributes, and transferring to the right relationship, so your code only needs to describe the transformation itself.
  - [FlowFileTransform](minifi_native/src/api/processor_wrappers/flow_file_transform.rs)
    - Consumes a single incoming flowfile, optionally rewrites its content (returned as an in-memory buffer) and/or adds attributes, then routes it to a relationship. Best for buffered, one-in / one-out transforms.
  - [FlowFileStreamingTransform](minifi_native/src/api/processor_wrappers/flow_file_stream_transform.rs)
    - Same one-in / one-out shape as `FlowFileTransform`, but you get both an `InputStream` and an `OutputStream` and write content incrementally. Use this when the payload is too large to hold in memory or when you want to stream data through as you process it.
  - [FlowFileSource](minifi_native/src/api/processor_wrappers/flow_file_source.rs)
    - Produces zero or more new flowfiles per trigger without consuming an input. Use this for processors that generate data (timers, pollers, external fetchers) — return an empty vec to yield when there is nothing to emit.
  - [ComplexProcessor](minifi_native/src/api/processor_wrappers/complex_processor.rs)
    - Escape hatch that hands you the raw `ProcessContext`, `ProcessSession`, and `Logger` — the same shape as MiNiFi C++'s `onTrigger` or NiFi Java's `Processor#onTrigger`. Use this when your processor doesn't fit the one-in / one-out / source molds: batching multiple flowfiles per trigger, custom routing logic, penalizing/rolling back, or anything that requires manual `session.get()` / `session.transfer()` bookkeeping.

#### FFI Wrappers
Concrete structs (CffiSession, CffiLogger, etc.) that implement the API traits by calling the unsafe functions from minifi-native-sys.

#### Thread safety
The trait system differentiates between thread-safe (&self) and single-threaded (&mut self) processors at compile time.

#### Comprehensive Mocking
A full suite of mock objects allows for fast and reliable unit testing of all processor logic.

### [minifi_native_macros](minifi_native_macros)
Helper crate that includes the procedural macros.

### [minifi_rs_behave](minifi_rs_behave)
Run the behave integration tests using MiNiFi's Docker framework. This will test the release artifacts against the latest released [MiNiFi native docker container](https://hub.docker.com/r/apache/nifi-minifi-cpp).
There is a handy alias to initiate all behave tests.

`cargo behave`

## Creating an Extension
Building an extension is straightforward. The framework provides a `declare_minifi_extension!` macro that automatically generates the C-compatible entry points and registers your components.

```rust
declare_minifi_extension!(
    processors: [
        (FlowFileSourceProcessorType, MultiThreaded, MyFlowFileSource),
        (FlowFileTransformProcessorType, SingleThreaded, MyDataTransformer),
    ],
    controllers: [
        MyCustomControllerService,
    ]
);
```

## Deployment
Build your extension as a dynamic library: `cd extensions/your_extension && cargo build --release`.

Locate the output artifact in `target/release/` (it will be a `.so` on Linux, `.dll` on Windows, or `.dylib` on macOS).

Copy the library file into the MiNiFi C++ application's `extensions/` directory.

Restart the MiNiFi C++ agent to automatically discover and load the new processors.

## Included Extensions

### [minifi_rs_playground](extensions/minifi_rs_playground)
A concrete example and testing ground for extensions built using the minifi-native crate.
- Demonstrates how to implement the Processor traits, define processor properties, and route to relationships.
- Includes comprehensive unit tests using the pure-Rust mocking framework.
- Includes integration testing that verifies the processor works as expected in a real, containerized MiNiFi environment.
