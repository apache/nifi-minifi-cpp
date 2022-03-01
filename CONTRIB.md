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

# Apache NiFi - MiNiFi - C++ Contribution Guide

We welcome all contributions to Apache MiNiFi. All new files must include a copy of the Apache License Header.
To make development easier, we've included the linter for the Google Style guide. Google provides an Eclipse formatter
for their style guide. It is located
[here](https://github.com/google/styleguide/blob/gh-pages/eclipse-cpp-google-style.xml).
New contributions are expected to follow the 
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html), except for the following points:
- Use .cpp extension for implementation files
- Use lowerCamelCase for functions, including accessors/mutators
- Use UPPER_SNAKE_CASE for constants
- Filenames are typically class names or a description of the contents in UpperCamelCase
- If a class is imitating something from STL, boost or similar, then STL-style lower_snake_case is used for naming the
  class. UpperCamelCase is used for most classes, in line with the Google Style Guide.
- Prefer `#pragma once` over include guards
- Forward declarations are OK
- Using-directives (`using namespace foo`) are discouraged, except for user-defined literal namespaces
- Some patterns in the codebase rely on objects with static storage duration without being trivially destructible and
  initialized with a constant expression. It's OK to use these.
- User-defined literal suffixes are OK
- Public mutable data members are allowed
- Inline function definition is OK
- Rvalue references, exceptions and RTTI are allowed
- Use gsl::narrow and gsl::narrow_cast in addition to standard casts. The codebase doesn't use abseil.
- We are more liberal regarding the use of `auto`. The Google Style Guide only allows using it when it makes the code
  clearer. In MiNiFi C++, it's up to the personal preferences of the contributor.
- Template metaprogramming is OK when necessary, as long as the usage is clear. Prefer the more readable alternatives
  when applicable.
- Enums are either UPPER_SNAKE_CASE or UpperCamelCase.
- File-level comments describing the contents are not required and typically not used in the codebase. A license header
  is required.
- Function comments use /** Javadoc style */
- Line length is not limited, but the linter warns on lines longer than 200 characters. Use a NOLINT line comment in the
  rare case when a longer line is more readable than splitting it up.
- Continuation indentation is ok with either 2 levels of indentation (4 spaces) or aligned.

It's ok to diverge from any of the rules with a good enough reason. We recommend following the C++ Core Guidelines, when
it doesn't contradict any of the Google Style Guide or the above exceptions.

C++ is a powerful language and "with great power comes great responsibility". Please aim for simple, readable and
maintainable solutions and avoid footguns. Be open for respectful debate in pull request reviews.

Shell script files shall follow the guidelines and best practices defined by the [shellcheck](https://github.com/koalaman/shellcheck) analysis tool.
New contributions are expected to pass the shellcheck analysis as part of the verification process.
If a shellcheck requested change is unfeasible it shall be disabled on per-line basis and will be subjected to review.
For more information on an issue please check the [shellcheck wiki page](https://github.com/koalaman/shellcheck/wiki).
You can run shellcheck by invoking the shellcheck cmake target, e.g. `make shellcheck`.

Python script files shall follow the PEP8 guidelines and best practices. The project includes [flake8](https://flake8.pycqa.org/en/latest/) checks
as part of the verification process, that is applied to all new contributions.

## Issues and Pull Requests

Issues within MiNiFi C++ are tracked in the official [Apache JIRA](https://issues.apache.org/jira/projects/MINIFICPP/issues).
New users can register freely and ask for contributor access on the
[Developers Mailing List](https://nifi.apache.org/mailing_lists.html).
Unassigned tickets may be assigned to yourself or filed via this JIRA instance. Pull requests can be submitted via our
[Github Mirror](https://github.com/apache/nifi-minifi-cpp) . When doing so please create a JIRA issue for your pull
request.

Apache NiFi MiNiFi C++ is a review then commit community. As a result, we will review your commits and merge them following 
review. We ask that you provide tests and documentation when possible. Typically PRs are merged after they get 2-3
approvals, usually in a week or two.

Once you have completed your changes, including source code and tests, you can verify that you follow the Google style guide by running the following command:

##### Linux or macOS
```
$ make linter
```
##### Windows
```
> msbuild linter.vcxproj
```

This will provide output for all source files.

### Third parties

Please see [ThirdParties.md](ThirdParties.md) on how MiNiFi builds and uses third party libraries and how you can add new ones.

### Extensions 

MiNiFi C++ contains a dynamic loading mechanism that loads arbitrary objects. To maintain consistency of development amongst the NiFi ecosystem, it is called a class loader. If you
are contributing a custom Processor or Controller Service, the mechanism to register your class into the default class loader is a pragma definition named:

    REGISTER_RESOURCE(CLASSNAME,DOCUMENTATION);

To use this include REGISTER_RESOURCE(YourClassName); in your header file. The default class loader will make instances of YourClassName available for inclusion.  

The extensions sub-directory allows you to contribute conditionally built extensions. The system adds all subdirectories in `extensions/*` that contain
a `CMakeLists.txt` file. It is up to the extension creator's discretion how they handle cmake flags.
It is important that `register_extension` be called at the end of the setup, for the extension to be made available to other stages of the build process.

```
# extensions/gps/CMakeLists.txt

# the author chooses to look for the explicit compilation request
if (NOT ENABLE_GPS)
  return()
endif()

#
# extension definition goes here
#

# at the end we should announce our extension
register_extension(minifi-gps "GPS EXTENSIONS" GPS-EXTENSION "Enables LibGPS Functionality and the GetGPS processor." "${TEST_DIR}/gps-tests")
# the first argument should be the extension target
# the next three arguments are used for documentation purposes
# the fifth optional argument designates the directory of the extension's tests

# we could optionally enforce a coding style by registering a linter target
register_extension_linter(minifi-gps-linter)
```

