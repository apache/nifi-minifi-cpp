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

To enable all extensions for your platform, you may use -DENABLE_ALL=TRUE OR select option m in the bootstrap.sh guided build process defined in the [ReadMe](https://github.com/apache/nifi-minifi-cpp/#bootstrapping)

# Extensions by example

Extensions consist of modules that are conditionally built into your client. Reasons why you may wish to do this with your modules/processors

  - Do not with to make dependencies required or the lack thereof is a known/expected runtime condition.
  - You wish to allow users to exclude dependencies for a variety of reasons.

# Extensions by example
We've used HTTP-CURL as the first example. We've taken all libcURL runtime classes and placed them into an extensions folder 
   - /extensions/http-curl
   
This folder contains a CMakeLists file so that we can conditionally build it. In the case with libcURL, if the user does not have curl installed OR they specify -DDISABLE_CURL=true in the cmake build, the extensions will not be built. In this case, when the extension is not built, C2 REST protocols, InvokeHTTP, and an HTTP Client implementation will not be included.

Your CMAKE file should build a static library, that will be included into the run time. This must be added with your conditional to the libminifi CMAKE, along with a platform specific whole archive inclusion. Note that this will ensure that despite no direct linkage being found by the compiler, we will include the code so that we can dynamically find your code.

# Including your extension in the build
There is a new function that can be used in the root cmake to build and included your extension. An example is based on the LibArchive extension. The createExtension function has 8 possible arguments. The first five arguments are required.
The first argument specifies the variable controlling the exclusion of this extension, followed by the variable that
is used when including it into conditional statements. The third argument is the pretty name followed by the description of the extension and the extension directory. The first optional argument is the test directory, which must also contain a CMakeLists.txt file. The seventh argument can be a conditional variable that tells us whether or not to add a third party subdirectory specified by the final extension.

In the lib archive example, we provide all arguments, but your extension may only need the first five and the the test folder. The seventh and eighth arguments must be included in tandem. 

```cmake
if ( NOT LibArchive_FOUND OR BUILD_LIBARCHIVE )
	set(BUILD_TP "TRUE")
endif()
createExtension(DISABLE_LIBARCHIVE 
				ARCHIVE-EXTENSIONS 
				"ARCHIVE EXTENSIONS" 
				"This Enables libarchive functionality including MergeContent, CompressContent, and (Un)FocusArchiveEntry" 
				"extensions/libarchive"
				"${TEST_DIR}/archive-tests"
				BUILD_TP
				"thirdparty/libarchive-3.3.2")
```

It is advised that you also add your extension to bootstrap.sh as that is the suggested method of configuring MiNiFi C++
  
# C bindings
To find your classes, you must adhere to a dlsym call back that adheres to the core::ObjectFactory class, like the one below. This object factory will return a list of classes, that we can instantiate through the class loader mechanism. Note that since we are including your code directly into our runtime, we will take care of dlopen and dlsym calls. A map from the class name to the object factory is kept in memory.

```C++
class __attribute__((visibility("default"))) HttpCurlObjectFactory : public core::ObjectFactory {
 public:
  HttpCurlObjectFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() {
    return "HttpCurlObjectFactory";
  }

  virtual std::string getClassName() {
    return "HttpCurlObjectFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() {
    std::vector<std::string> class_names;
    class_names.push_back("RESTProtocol");
    class_names.push_back("RESTReceiver");
    class_names.push_back("RESTSender");
    class_names.push_back("InvokeHTTP");
    class_names.push_back("HTTPClient");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) {
    if (class_name == "RESTReceiver") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::c2::RESTReceiver>());
    } else if (class_name == "RESTSender") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::c2::RESTSender>());
    } else if (class_name == "InvokeHTTP") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<processors::InvokeHTTP>());
    } else if (class_name == "HTTPClient") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<utils::HTTPClient>());
    } else {
      return nullptr;
    }
  }

};

extern "C" {
void *createHttpCurlFactory(void);
}
```

#Using your object factory function
To use your C function, you must define the sequence "Class Loader Functions" in your YAML file under FlowController. This will indicate to the code that the factory function is to be called and we will create the mappings defined, above.

Note that for the case of HTTP-CURL we have made it so that if libcURL exists and it is not disabled, the createHttpCurlFactory function is automatically loaded. To do this use the function FlowConfiguration::add_static_func -- this will add your function to the list of registered resources and will do so in a thread safe way. If you take this approach you cannot disable your library with an argument within the YAML file.


#Enabling Modules

#Enabling PacketCapture

Packet Capture can be enabled to support capturing pcap files from all network interfaces on the machine. To do enable this, you must type the following when building MiNiFi C++
```
	cmake -DENABLE_PCAP=TRUE ..
```

The cmake process will include a notification that the third party dependency, PcapPlusPlus has created its platform specific modules. 

You will see a message such as this when building. This configuration script will contain the name of your platforma and be an indication that 
the configuration was successful:

		****************************************
		PcapPlusPlus Linux configuration script 
		****************************************

When the PCAP extension is built, you will be able to use the PacketCapture Processor, which has two major options: 'Batch Size' and 'Batch Directory'

Batch Size will limit the number of packets taht are combined within a given PCAP. Batch Directory will allow the user to specify the scratch directory for network data to be written to. 

Note that if Batch Directory is not specified, /tmp/ will be used.

*Running PcapTests requires root privileges on Linux.

#Expressions

To evaluate dynamic expressions using the [NiFi Expression Language](https://nifi.apache.org/docs/nifi-docs/html/expression-language-guide.html),
use the `bool getProperty(const std::string &name, std::string &value, const std::shared_ptr<FlowFile> &flow_file);`
method within processor extensions. The expression defined in the property will be compiled if it hasn't already been, and
the expression will be evaluated against the provided flow file (may be `nullptr`). The result of the evaluation is
stored into the `&value` parameter.
