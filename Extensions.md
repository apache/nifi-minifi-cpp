<!--
  Licensed to the Apache Software Foundation (ASF) under one or more
  contributor license agreements.  See the NOTICE file distributed with
  this work for additional information regarding copyright ownership.
  The ASF licenses this file to You under the Apache License, Version 2.0
  (the "License"); you may not use this file except in compliance with
  the License.  You may obtain a copy of the License at
      http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
-->
# Apache MiNiFi Extensions Guide

To enable all extensions for your platform, you may use -DENABLE_ALL=TRUE OR select the option to "Enable all Extensions" in the bootstrap script. [ReadMe](https://github.com/apache/nifi-minifi-cpp/#bootstrapping)

# Extension internals
Extensions are dynamic libraries loaded at runtime by the agent.

## C extensions
You can build a shared library depending on the C capabilities of the agent as given in the `minifi-c.h` file.
For the shared library to be considered a valid extension, it has to have a global symbol with the name `MinifiCApiVersion`
with its value as a null terminated string (`const char*`) of the macro `MINIFI_API_VERSION` from `minifi-c.h`.

Moreover the actual resource registration (processors/controller services) has to happen during the `MinifiInitExtension` call.
One possible example of this is:

```C++
extern "C" const uint32_t MinifiApiVersion = MINIFI_API_VERSION;

extern "C" void MinifiInitExtension(MinifiExtensionContext* extension_context) {
  MinifiExtensionCreateInfo ext_create_info{
    .name = minifi::api::utils::toStringView(MAKESTRING(EXTENSION_NAME)),
    .version = minifi::api::utils::toStringView(MAKESTRING(EXTENSION_VERSION)),
    .deinit = nullptr,
    .user_data = nullptr
  };
  auto* extension = MinifiCreateExtension(extension_context, &ext_create_info);
  minifi::api::core::useProcessorClassDescription<minifi::extensions::llamacpp::processors::RunLlamaCppInference>([&] (const MinifiProcessorClassDefinition& description) {
    MinifiRegisterProcessor(extension, &description);
  });
}
```

## C++ extensions
You can utilize the C++ api, linking to `minifi-api` and possibly using the helpers in `extension-framework`.
No compatibilities are guaranteed beyond what extensions are built together with the agent at the same time.

An extension makes its capabilities (classes) available to the system through registrars. Registration must happen in source files, not headers.

```C++
// register user-facing classes as
REGISTER_RESOURCE(InvokeHTTP, Processor);
// or
REGISTER_RESOURCE(SSLContextService, ControllerService);

// register internal resources as
REGISTER_RESOURCE(HTTPClient, InternalResource);
// or
REGISTER_RESOURCE(RESTSender, DescriptionOnly);
```

Some extensions (e.g. `OpenCVExtension`) require initialization before use.
You need to define an `MinifiInitCppExtension` function of type `MinifiExtension*(MinifiConfig*)` to be called.

```C++
extern "C" void MinifiInitCppExtension(MinifiExtension* extension, MinifiConfig* /*config*/) {
  const auto success = org::apache::nifi::minifi::utils::Environment::setEnvironmentVariable("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;udp", false /*overwrite*/);
  if (!success) {
    return nullptr;
  }
  MinifiExtensionCreateInfo ext_create_info{
    .name = minifi::utils::toStringView(MAKESTRING(MODULE_NAME)),
    .version = minifi::utils::toStringView(minifi::AgentBuild::VERSION),
    .deinit = nullptr,
    .user_data = nullptr,
    .processors_count = 0,
    .processors_ptr = nullptr,
    .controller_services_count = 0,
    .controller_services_ptr = nullptr,
  };
  minifi::utils::MinifiCreateCppExtension(extension, &ext_create_info);
}
```

# Loading extensions

The agent will look for the `nifi.extension.path` property in the `minifi.properties` file to determine what extensions to load.

The property expects a comma separated list of paths.
The paths support wildcards, specifically, a standalone `**` matches any number of nested directories, the `*` matches any number of characters in a single segment and `?` matches one single character in a segment.
Relative paths are relative to the agent executable.
```
// This matches all files in the 'extensions' directory next to the directory the executable is in.
nifi.extension.path=../extensions/*
```

### Exlusion
If you want to exclude some extensions from being loaded, without having to specify the rest, you can do so by prefixing the pattern with `!`.
```
// This loads all extensions but the azure extension. (the exact name differs by platform: dylib, dll, so)
nifi.extension.path=../extensions/*,!../extensions/libminifi-azure.so
```

You could even exclude some subdirectory and then re-include specific extensions/subdirectories in that.
```
// The last pattern that matches an extension will determine if that extension is loaded or not.
nifi.extension.path=../extensions/**,!../extensions/private/*,../extension/private/my-cool-extension.dll
```
