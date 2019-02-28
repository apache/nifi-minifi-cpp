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


We welcome all contributions to Apache MiNiFi. To make development easier, we've included
the linter for the Google Style guide. Google provides an Eclipse formatter for their style
guide. It is located [here](https://github.com/google/styleguide/blob/gh-pages/eclipse-cpp-google-style.xml).
New contributions are expected to follow the Google style guide when it is reasonable.
Additionally, all new files must include a copy of the Apache License Header.


## Issues and Pull Requests

Issues within MiNiFi C++ are tracked in the official [Apache JIRA](https://issues.apache.org/jira/projects/MINIFICPP/issues)
Unassigned tickets may be assigned to yourself or filed via this JIRA instance. Pull requests can be submitted via our [Github
Mirror](https://github.com/apache/nifi-minifi-cpp) . When doing so try and have a ticket filed when submitting your pull request.

Apache NiFi MiNiFi C++ is a review then commit community. As a result, we will review your commits and merge them following 
review. We ask that you provide tests and documentation when possible. 

Once you have completed your changes, including source code and tests, you can verify that you follow the Google style guide by running the following command:
     $ make linter.
     
This will provide output for all source files.

### Extensions 

MiNiFi C++ contains a dynamic loading mechanism that loads arbitrary objects. To maintain consistency of development amongst the NiFi ecosystem, it is called a class loader. If you
are contributing a custom Processor or Controller Service, the mechanism to register your class into the default class loader is a pragma definition named:

    REGISTER_RESOURCE(CLASSNAME,DOCUMENTATION);

To use this include REGISTER_RESOURCE(YourClassName); in your header file. The default class loader will make instances of YourClassName available for inclusion.  

The extensions sub-directory allows you to contribute conditionally built extensions. An example of the GPS extension will provide an example. In this a conditional
allows flags to specify that your extension is to be include or excluded by default. In this example -DENABLE_GPS=ON must be specified by the builder to  include it.
The function call will then create an extension that will automatically be while main is built. The first argument of createExtension will be the target
reference that is automatically used for documentation and linking. The second and third arguments are used for printing information on what was built or linked in
the consumer's build. The last two argument represent where the extension and tests exist. 

	if (ENABLE_ALL OR ENABLE_GPS)
		createExtension(GPS-EXTENSION "GPS EXTENSIONS" "Enables LibGPS Functionality and the GetGPS processor." "extensions/gps" "${TEST_DIR}/gps-tests")
	endif()

	
Once the createExtension target is made in the root CMakeLists.txt , you may load your dependencies and build your targets. Once you are finished defining your build
and link commands, you must set your target reference to a target within your build. In this case, the previously mentioned GPS-EXTENSION will be assigned to minifi-gps.
The next call register_extension will ensure that minifi-gps is linked appropriately for inclusion into the final binary.  
	
	SET (GPS-EXTENSION minifi-gps PARENT_SCOPE)
	register_extension(minifi-gps)
	
	
