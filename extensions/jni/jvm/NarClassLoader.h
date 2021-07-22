/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <jni.h>

#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <memory>
#include <utility>
#include <map>
#include <algorithm>

#include "JavaServicer.h"
#include "JniBundle.h"
#include "../JavaException.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

class NarClassLoader {
 public:
  NarClassLoader(std::shared_ptr<minifi::jni::JavaServicer> servicer, JavaClass &clazz, const std::string &dir_name, const std::string &scratch_nar_dir, const std::string &docs_dir)
      : java_servicer_(servicer) {
    class_ref_ = clazz;
    auto env = java_servicer_->attach();
    class_loader_ = class_ref_.newInstance(env);
    jmethodID mthd = env->GetMethodID(class_ref_.getReference(), "initializeNarDirectory", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    if (mthd == nullptr) {
      throw std::runtime_error("Could not find method to construct JniClassLoader");
    } else {
      auto dirNameStr = env->NewStringUTF(dir_name.c_str());
      auto narWriteBaseStr = env->NewStringUTF(scratch_nar_dir.c_str());
      auto docsDirStr = env->NewStringUTF(docs_dir.c_str());
      env->CallVoidMethod(class_loader_, mthd, dirNameStr, narWriteBaseStr, docsDirStr, servicer->getClassLoader());
      ThrowIf(env);
    }

    // we should now have

    getBundles();
  }

  ~NarClassLoader() {
    java_servicer_->attach()->DeleteGlobalRef(class_loader_);
  }

  jclass getClass(const std::string &requested_name) {
    auto env = java_servicer_->attach();
    jmethodID mthd = env->GetMethodID(class_ref_.getReference(), "getClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (mthd == nullptr) {
      ThrowIf(env);
    }

    auto clazz_name = env->NewStringUTF(requested_name.c_str());

    auto job = env->CallObjectMethod(class_loader_, mthd, clazz_name);

    ThrowIf(env);

    return (jclass) job;
  }

  std::pair<std::string, std::string> getAnnotation(const std::string &requested_name, const std::string &method_name) {
    auto env = java_servicer_->attach();
    std::string methodName, signature;
    {
      jmethodID mthd = env->GetMethodID(class_ref_.getReference(), "getMethod", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
      if (mthd == nullptr) {
        ThrowIf(env);
      }

      auto clazz_name = env->NewStringUTF(requested_name.c_str());
      auto annotation_name = env->NewStringUTF(method_name.c_str());

      jstring obj = (jstring) env->CallObjectMethod(class_loader_, mthd, clazz_name, annotation_name);

      ThrowIf(env);

      if (obj) {
        methodName = JniStringToUTF(env, obj);
      }
    }
    {
      jmethodID mthd = env->GetMethodID(class_ref_.getReference(), "getSignature", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
      if (mthd == nullptr) {
        ThrowIf(env);
      }

      auto clazz_name = env->NewStringUTF(requested_name.c_str());
      auto annotation_name = env->NewStringUTF(method_name.c_str());

      jstring obj = (jstring) env->CallObjectMethod(class_loader_, mthd, clazz_name, annotation_name);

      ThrowIf(env);

      if (obj) {
        signature = JniStringToUTF(env, obj);
      }
    }

    return std::make_pair(methodName, signature);
  }

  std::map<std::string, std::string> getAnnotations(const std::string &requested_name, const std::string &method_name) {
    auto env = java_servicer_->attach();
    std::vector<std::string> method_names;
    std::map<std::string, std::string> methods_with_signatures;
    {
      jmethodID mthd = env->GetMethodID(class_ref_.getReference(), "getMethods", "(Ljava/lang/String;Ljava/lang/String;)Ljava/util/List;");
      if (mthd == nullptr) {
        ThrowIf(env);
      }

      auto clazz_name = env->NewStringUTF(requested_name.c_str());
      auto annotation_name = env->NewStringUTF(method_name.c_str());

      jobject jList = env->CallObjectMethod(class_loader_, mthd, clazz_name, annotation_name);
      ThrowIf(env);
      jclass cList = env->FindClass("java/util/List");
      jmethodID mSize = env->GetMethodID(cList, "size", "()I");
      jmethodID mGet = env->GetMethodID(cList, "get", "(I)Ljava/lang/Object;");

      // get the size of the list
      jint size = env->CallIntMethod(jList, mSize);
      ThrowIf(env);
      // walk through and fill the vector
      for (jint i = 0; i < size; i++) {
        jstring strObj = (jstring) env->CallObjectMethod(jList, mGet, i);
        ThrowIf(env);
        method_names.push_back(JniStringToUTF(env, strObj));
      }
    }
    for (const auto &method_name_str : method_names) {
      jmethodID mthd = env->GetMethodID(class_ref_.getReference(), "getMethodSignature", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
      if (mthd == nullptr) {
        ThrowIf(env);
      }

      auto clazz_name = env->NewStringUTF(requested_name.c_str());
      auto jstring_method_name = env->NewStringUTF(method_name_str.c_str());
      auto annotation_name = env->NewStringUTF(method_name.c_str());

      jstring obj = (jstring) env->CallObjectMethod(class_loader_, mthd, clazz_name, jstring_method_name, annotation_name);

      ThrowIf(env);

      if (obj) {
        auto signature = JniStringToUTF(env, obj);
        methods_with_signatures[method_name_str] = signature;
      }
    }

    return methods_with_signatures;
  }
  /**
   * Call empty constructor
   */
  JNIEXPORT
  jobject newInstance(const std::string &requested_name) {
    auto env = java_servicer_->attach();

    jmethodID mthd = env->GetMethodID(class_ref_.getReference(), "createObject", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (mthd == nullptr) {
      ThrowIf(env);
    }

    auto clazz_name = env->NewStringUTF(requested_name.c_str());

    auto newref = env->CallObjectMethod(class_loader_, mthd, clazz_name);

    ThrowIf(env);

    jobject obj = env->NewGlobalRef(newref);

    ThrowIf(env);

    return obj;
  }

 private:
  /**
   * Call empty constructor
   */
  jobject getBundles() {
    auto env = java_servicer_->attach();

    jmethodID mthd = env->GetMethodID(class_ref_.getReference(), "getBundles", "()Ljava/util/List;");
    if (mthd == nullptr) {
      ThrowIf(env);
    }

    jobject obj = env->CallObjectMethod(class_loader_, mthd);

    ThrowIf(env);

    auto list_class = env->FindClass("java/util/ArrayList");

    ThrowIf(env);

    size_t size = getSize(list_class, env, obj);

    for (size_t i = 0; i < size; i++) {
      JniBundle bundle = getBundle(list_class, env, obj, i);
      for (const auto &cd : bundle.getDescriptions()) {
        auto lastOfIdx = cd.class_name_.find_last_of(".");
        if (lastOfIdx != std::string::npos) {
          lastOfIdx++;  // if a value is found, increment to move beyond the .
          int nameLength = cd.class_name_.length() - lastOfIdx;
          const auto processorName = cd.class_name_.substr(lastOfIdx, nameLength);
          if (!core::ClassLoader::getDefaultClassLoader().getGroupForClass(processorName)) {
            minifi::ExternalBuildDescription::addExternalComponent(bundle.getDetails(), cd);
          }
        }
      }
    }

    return obj;
  }

  size_t getSize(jclass list_class, JNIEnv *env, jobject list) {
    jmethodID mthd = env->GetMethodID(list_class, "size", "()I");
    if (mthd == nullptr) {
      ThrowIf(env);
    }
    jint obj = (jint) env->CallIntMethod(list, mthd);
    minifi::jni::ThrowIf(env);
    return obj;
  }

  JniBundle getBundle(jclass list_class, JNIEnv *env, jobject list, int index) {
    jmethodID mthd = env->GetMethodID(list_class, "get", "(I)Ljava/lang/Object;");
    if (mthd == nullptr) {
      ThrowIf(env);
    }
    auto bundle = env->CallObjectMethod(list, mthd, index);
    ThrowIf(env);
    if (bundle != nullptr) {
      auto jni_bundle_clazz = getClass("org.apache.nifi.processor.JniBundle");
      struct BundleDetails details = getCoordinateDetails(env, jni_bundle_clazz, bundle);
      std::vector<ClassDescription> descriptions = getDescriptions(env, jni_bundle_clazz, bundle);

      JniBundle newBundle(details);
      for (const auto &cd : descriptions) {
        newBundle.addDescription(cd);
      }

      return newBundle;
    }
    // assuming we have the bundle, we need to get the coordinate.

    return JniBundle();
  }

  std::vector<ClassDescription> getDescriptions(JNIEnv *env, jclass jni_bundle_clazz, jobject bundle) {
    std::vector<ClassDescription> descriptions;
    auto jni_component_clazz = getClass("org.apache.nifi.processor.JniComponent");

    auto list_class = env->FindClass("java/util/ArrayList");

    if (nullptr == jni_component_clazz || nullptr == jni_bundle_clazz) {
      return descriptions;
    }
    auto components = getComponents(jni_bundle_clazz, env, bundle);

    size_t size = getSize(list_class, env, components);

    for (size_t i = 0; i < size; i++) {
      descriptions.push_back(getClassDescription(list_class, env, jni_component_clazz, components, i));
    }

    return descriptions;
  }

  ClassDescription getClassDescription(jclass list_class, JNIEnv *env, jclass jni_component_clazz, jobject list, int index) {
    jmethodID mthd = env->GetMethodID(list_class, "get", "(I)Ljava/lang/Object;");
    auto property_descriptor_clazz = getClass("org.apache.nifi.components.PropertyDescriptor");
    if (mthd == nullptr) {
      ThrowIf(env);
    }
    auto component = env->CallObjectMethod(list, mthd, index);
    minifi::jni::ThrowIf(env);
    if (component != nullptr) {
      auto type = getStringMethod("getType", jni_component_clazz, env, component);
      auto isControllerService = getBoolmethod("isControllerService", jni_component_clazz, env, component);
      ClassDescription description(type);
      {
        jmethodID getDescriptorMethod = env->GetMethodID(jni_component_clazz, "getDescriptors", "()Ljava/util/List;");

        jobject descriptors = env->CallObjectMethod(component, getDescriptorMethod);

        ThrowIf(env);

        if (descriptors) {
          size_t size = getSize(list_class, env, descriptors);

          // iterate through each property descriptor
          for (size_t i = 0; i < size; i++) {
            auto propertyDescriptorObj = env->CallObjectMethod(descriptors, mthd, i);
            minifi::jni::ThrowIf(env);
            if (propertyDescriptorObj != nullptr) {
              auto propName = getStringMethod("getName", property_descriptor_clazz, env, propertyDescriptorObj);
              auto propDesc = getStringMethod("getDescription", property_descriptor_clazz, env, propertyDescriptorObj);
              auto defaultValue = getStringMethod("getDefaultValue", property_descriptor_clazz, env, propertyDescriptorObj);

              auto builder = core::PropertyBuilder::createProperty(propName)->withDescription(propDesc);
              if (!defaultValue.empty()) {
                builder->withDefaultValue(defaultValue);
              }

              builder = builder->isRequired(getBoolmethod("isRequired", property_descriptor_clazz, env, propertyDescriptorObj));
              core::Property prop(builder->build());
              description.class_properties_.insert(std::make_pair(prop.getName(), prop));
            }
          }
        }
      }
      description.is_controller_service_ = isControllerService;
      jmethodID getRelationshipsMethod = env->GetMethodID(jni_component_clazz, "getRelationships", "()Ljava/util/List;");
      ThrowIf(env);
      jobject relationships = env->CallObjectMethod(component, getRelationshipsMethod);
      ThrowIf(env);
      if (relationships) {
        size_t size = getSize(list_class, env, relationships);

        // iterate through each property descriptor
        for (size_t i = 0; i < size; i++) {
          auto propertyDescriptorObj = env->CallObjectMethod(relationships, mthd, i);
          minifi::jni::ThrowIf(env);
          if (propertyDescriptorObj != nullptr) {
            auto relName = getStringMethod("getName", property_descriptor_clazz, env, propertyDescriptorObj);
            auto relDesc = getStringMethod("getDescription", property_descriptor_clazz, env, propertyDescriptorObj);

            core::Relationship relationship(relName, relDesc);

            description.class_relationships_.push_back(relationship);
          }
        }
      }

      auto classDescription = getStringMethod("getDescription", jni_component_clazz, env, component);
      description.dynamic_relationships_ = getBoolmethod("getDynamicRelationshipsSupported", jni_component_clazz, env, component);
      description.dynamic_properties_ = getBoolmethod("getDynamicPropertiesSupported", jni_component_clazz, env, component);

      AgentDocs::putDescription(type, classDescription);

      return description;
    }
    // assuming we have the bundle, we need to get the coordinate.

    return ClassDescription("unknown");
  }

  struct BundleDetails getCoordinateDetails(JNIEnv *env, jclass jni_bundle_clazz, jobject bundle) {
    auto bundle_details = getClass("org.apache.nifi.bundle.BundleDetails");
    auto bundle_coordinate = getClass("org.apache.nifi.bundle.BundleCoordinate");
    struct BundleDetails details;

    if (nullptr == bundle_details || nullptr == bundle_coordinate || nullptr == jni_bundle_clazz) {
      return details;
    }

    auto jdetails = getDetails(jni_bundle_clazz, env, bundle);

    if (nullptr != jdetails) {
      auto jcoordinate = getCoordinate(bundle_details, env, jdetails);
      if (nullptr != jcoordinate) {
        details.artifact = getArtifact(bundle_coordinate, env, jcoordinate);
        details.group = getGroup(bundle_coordinate, env, jcoordinate);
        details.version = getVersion(bundle_coordinate, env, jcoordinate);
      }
    }

    return details;
  }

  bool getBoolmethod(const std::string &methodName, jclass bundle_coordinate, JNIEnv *env, jobject coord) {
    jmethodID getIdMethod = env->GetMethodID(bundle_coordinate, methodName.c_str(), "()Z");

    if (getIdMethod == nullptr) {
      ThrowIf(env);
    }
    auto res = (jboolean) env->CallBooleanMethod(coord, getIdMethod);
    ThrowIf(env);
    return res;
  }

  std::string getStringMethod(const std::string &methodName, jclass bundle_coordinate, JNIEnv *env, jobject coord) {
    jmethodID getIdMethod = env->GetMethodID(bundle_coordinate, methodName.c_str(), "()Ljava/lang/String;");

    if (getIdMethod == nullptr) {
      ThrowIf(env);
    }
    auto id = (jstring) env->CallObjectMethod(coord, getIdMethod);
    ThrowIf(env);
    if (id == nullptr)
      return "";
    return JniStringToUTF(env, id);
  }

  std::string getArtifact(jclass bundle_coordinate, JNIEnv *env, jobject coord) {
    return getStringMethod("getId", bundle_coordinate, env, coord);
  }

  std::string getGroup(jclass bundle_coordinate, JNIEnv *env, jobject coord) {
    return getStringMethod("getGroup", bundle_coordinate, env, coord);
  }

  std::string getVersion(jclass bundle_coordinate, JNIEnv *env, jobject coord) {
    return getStringMethod("getVersion", bundle_coordinate, env, coord);
  }

  jobject getCoordinate(jclass bundle_details, JNIEnv *env, jobject jdetail) {
    jmethodID getCoordinateMethod = env->GetMethodID(bundle_details, "getCoordinate", "()Lorg/apache/nifi/bundle/BundleCoordinate;");
    if (getCoordinateMethod == nullptr) {
      ThrowIf(env);
    }
    auto coordinate = env->CallObjectMethod(jdetail, getCoordinateMethod);
    ThrowIf(env);
    return coordinate;
  }

  jobject getDetails(jclass bundle_details, JNIEnv *env, jobject bundle) {
    jmethodID getDetailsMethod = env->GetMethodID(bundle_details, "getDetails", "()Lorg/apache/nifi/bundle/BundleDetails;");
    if (getDetailsMethod == nullptr) {
      ThrowIf(env);
    }
    auto details = env->CallObjectMethod(bundle, getDetailsMethod);
    ThrowIf(env);
    return details;
  }

  jobject getComponents(jclass bundle_details, JNIEnv *env, jobject bundle) {
    jmethodID getDetailsMethod = env->GetMethodID(bundle_details, "getComponents", "()Ljava/util/List;");
    if (getDetailsMethod == nullptr) {
      ThrowIf(env);
    }
    auto details = env->CallObjectMethod(bundle, getDetailsMethod);
    ThrowIf(env);
    return details;
  }

  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<minifi::jni::JavaServicer> java_servicer_;
  JavaClass class_ref_;
  jobject class_loader_;
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
