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
Extensions are dynamic libraries loaded at runtime by the agent. An extension makes its 
capabilities (classes) available to the system through registrars. Registration must happen in source files, not headers.

``` C++
// register user-facing classes as
REGISTER_RESOURCE(InvokeHTTP, "An HTTP client processor which can interact with a configurable HTTP Endpoint. "
    "The destination URL and HTTP Method are configurable. FlowFile attributes are converted to HTTP headers and the "
    "FlowFile contents are included as the body of the request (if the HTTP Method is PUT, POST or PATCH).");

// register internal resources as
REGISTER_INTERNAL_RESOURCE(HTTPClient);
```

Some extensions (e.g. `http-curl`) require initialization before use. You need to subclass `Extension` and let the system know by using `REGISTER_EXTENSION`.

```C++
class HttpCurlExtension : core::extension::Extension {
 public:
  using Extension::Extension;
  bool doInitialize(const core::extension::ExtensionConfig& /*config*/) override {
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
  }
  void doDeinitialize() override {
    curl_global_cleanup();
  }
};

REGISTER_EXTENSION(HttpCurlExtension);
```

If you don't use `REGISTER_EXTENSION`, the registered resources still become available, so make sure to register the extension if you need special initialization.

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
// This loads all extensions but the gps extension. (the exact name differs by platform: dylib, dll, so)
nifi.extension.path=../extensions/*,!../extensions/libminifi-gps.so
```

You could even exclude some subdirectory and then re-include specific extensions/subdirectories in that.
```
// The last pattern that matches an extension will determine if that extension is loaded or not.
nifi.extension.path=../extensions/**,!../extensions/private/*,../extension/private/my-cool-extension.dll
```
