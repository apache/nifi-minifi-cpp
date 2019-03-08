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
#ifndef EXTENSIONS_JVMLOADER_H
#define EXTENSIONS_JVMLOADER_H

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include "JavaClass.h"
#include "JavaServicer.h"
#include "../JavaException.h"
#include "core/Core.h"
#include <jni.h>
#ifndef WIN32
#include <dlfcn.h>
#endif
#include "Core.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

/**
 * Purpose and Justification: Provides a mapping function for jfields.
 * Note that jFieldIDs aren't local references, so we don't need to worry
 * about keeping them around. Thus this class provides us a caching mechanism.
 */
class FieldMapping {
 public:
  jfieldID getField(const std::string &clazz, const std::string &fnArg) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto group = map_.find(clazz);
    if (group != map_.end()) {
      auto match = group->second.find(fnArg);
      if (match != group->second.end()) {
        return match->second;
      }
    }
    return nullptr;
  }

  void putField(const std::string &clazz, const std::string &fnArg, jfieldID field) {
    std::lock_guard<std::mutex> guard(mutex_);
    map_[clazz].insert(std::make_pair(fnArg, field));
  }

 private:
  std::mutex mutex_;
  std::map<std::string, std::map<std::string, jfieldID>> map_;
};

typedef jint (*registerNatives_t)(JNIEnv* env, jclass clazz);

jfieldID getPtrField(JNIEnv *env, jobject obj);

template<typename T>
T *getPtr(JNIEnv *env, jobject obj);

template<typename T>
void setPtr(JNIEnv *env, jobject obj, T *t);

/**
 * Purpose and Justification: Provides a singleton reference to a JVM.
 *
 * Since JVMs are singular in reference ( meaning that there cannot be more than one
 * at any given time ), we need to create a singleton access pattern.
 *
 */
class JVMLoader {
 public:

  bool initialized() {
    return initialized_;
  }

  /**
   * Attach the current thread
   * @return JNIEnv reference.
   */
  JNIEnv *attach(const std::string &name = "") {
    JNIEnv* jenv;
    jint ret = jvm_->GetEnv((void**) &jenv, JNI_VERSION_1_8);

    if (ret == JNI_EDETACHED) {
      ret = jvm_->AttachCurrentThread((void**) &jenv, NULL);
      if (ret != JNI_OK || jenv == NULL) {
        throw std::runtime_error("Could not find class");
      }
    }

    return jenv;
  }

  void detach(){
    jvm_->DetachCurrentThread();
  }

  /**
   * Returns a reference to an instantiated class loader
   * @return class loader.
   */
  jobject getClassLoader() {
    return gClassLoader;
  }

  /**
   * Removes a class reference
   * @param name class name
   */
  void remove_class(const std::string &name) {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    auto finder = objects_.find(name);
    if (finder != objects_.end()) {
      auto oldClzz = finder->second;
      JavaClass clazz(oldClzz);
      attach(name)->DeleteGlobalRef(clazz.getReference());
    }
    objects_.erase(name);
  }

  /**
   * Loads a class, creating a global reference to the jclass
   * @param class name.
   * @return JavaClass
   */
  JavaClass load_class(const std::string &clazz_name, JNIEnv *lenv = nullptr) {
    // names should come with package/package/name, so we must normalize the name

    JNIEnv *env = lenv;

    // when we do the lookup here we will be using a boostrap CL so we'll need
    // to ensure class names are not in their canonical form. Since we want to
    // support both we will do the transition from . to / and later on from
    // / to . to normalize. Forcing all classes that enter this method
    // to be a certain way isn't very defensible.
    std::string name = clazz_name;
    name = utils::StringUtils::replaceAll(name, ".", "/");

    if (env == nullptr) {
      env = attach(name);
    }

    std::lock_guard<std::mutex> lock(internal_mutex_);
    auto finder = objects_.find(name);
    if (finder != objects_.end()) {
      auto oldClzz = finder->second;
      JavaClass clazz(oldClzz);
      return clazz;
    }

    std::string modifiedName = name;
    modifiedName = utils::StringUtils::replaceAll(modifiedName, "/", ".");

    auto jstringclass = env->NewStringUTF(modifiedName.c_str());

    auto preclass = env->CallObjectMethod(gClassLoader, gFindClassMethod, jstringclass);

    minifi::jni::ThrowIf(env);

    auto obj = (jclass) env->NewGlobalRef(preclass);

    minifi::jni::ThrowIf(env);

    auto clazzobj = static_cast<jclass>(obj);

    JavaClass clazz(name, clazzobj, env);
    objects_.insert(std::make_pair(name, clazz));
    return clazz;
  }

  JavaClass getObjectClass(const std::string &name, jobject jobj) {
    auto env = attach();
    auto jcls = (jclass) env->NewGlobalRef(env->GetObjectClass(jobj));
    return JavaClass(name, jcls, env);
  }

  static JVMLoader *getInstance() {
    static JVMLoader jvm;
    return &jvm;
  }

  JNIEnv *getEnv() {
    return env_;
  }

  /**
   * Returns an instance to the JVMLoader
   * @param pathVector vector of paths
   * @param otherOptions jvm options.
   */
  static JVMLoader *getInstance(const std::vector<std::string> &pathVector, const std::vector<std::string> &otherOptions = std::vector<std::string>()) {
    JVMLoader *jvm = getInstance();
    if (!jvm->initialized()) {
      std::stringstream str;
      std::vector<std::string> options;
      for (const auto &path : pathVector) {
        if (str.str().length() > 0) {
#ifdef WIN32
          str << ";" << path;
#else
          str << ":" << path;
#endif
        } else
          str << path;
      }
      options.insert(options.end(), otherOptions.begin(), otherOptions.end());
      std::string classpath = "-Djava.class.path=" + str.str();
      options.push_back(classpath);
      jvm->initialize(options);
    }
    return jvm;
  }

  template<typename T>
  void setReference(jobject obj, T *t) {
    setPtr(env_, obj, t);
  }

  template<typename T>
  void setReference(jobject obj, JNIEnv *env, T *t) {
    setPtr(env, obj, t);
  }

  template<typename T>
  T *getReference(JNIEnv *env, jobject obj) {
    return getPtr<T>(env, obj);
  }

  /**
   * Get the pointer field to the
   * This expects nativePtr to exist. I've always used nativePtr because I know others use it across
   * stack overflow. It's a common field, so the hope is that we can potentially access any class
   * in which the native pointer is stored in nativePtr.
   */
  static jfieldID getPtrField(const std::string &className, JNIEnv *env, jobject obj) {
    static std::string fn = "nativePtr", args = "J", lookup = "nativePtrJ";
    auto field = getClassMapping().getField(className, lookup);
    if (field != nullptr) {
      return field;
    }

    jclass c = env->GetObjectClass(obj);
    return env->GetFieldID(c, "nativePtr", "J");
  }

  template<typename T>
  static T *getPtr(JNIEnv *env, jobject obj) {
    jlong handle = env->GetLongField(obj, getPtrField(minifi::core::getClassName<T>(), env, obj));
    return reinterpret_cast<T *>(handle);
  }

  template<typename T>
  static void setPtr(JNIEnv *env, jobject obj, T *t) {
    jlong handle = reinterpret_cast<jlong>(t);
    env->SetLongField(obj, getPtrField(minifi::core::getClassName<T>(), env, obj), handle);
  }

  void setBaseServicer(std::shared_ptr<JavaServicer> servicer) {
    java_servicer_ = servicer;
  }

  std::shared_ptr<JavaServicer> getBaseServicer() const {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    return java_servicer_;
  }

  template<typename T>
  static void putClassMapping(JNIEnv *env, JavaClass &clazz, const std::string &fieldStr, const std::string &arg) {
    auto classref = clazz.getReference();
    auto name = minifi::core::getClassName<T>();
    auto field = env->GetFieldID(classref, fieldStr.c_str(), arg.c_str());
    auto fieldName = fieldStr + arg;
    getClassMapping().putField(name, fieldName, field);

  }

 protected:

  static FieldMapping &getClassMapping() {
    static FieldMapping map;
    return map;
  }

  mutable std::mutex internal_mutex_;

#ifdef WIN32

// base_object doesn't have a handle
  std::map< HMODULE, std::string > resource_mapping_;

  std::string error_str_;
  std::string current_error_;

  void store_error() {
    auto error = GetLastError();

    if (error == 0) {
      error_str_ = "";
      return;
    }

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    current_error_ = std::string(messageBuffer, size);

    //Free the buffer.
    LocalFree(messageBuffer);
  }

  void *dlsym(void *handle, const char *name)
  {
    FARPROC symbol;
    HMODULE hModule;

    symbol = GetProcAddress((HMODULE)handle, name);

    if (symbol == nullptr) {
      store_error();

      for (auto hndl : resource_mapping_)
      {
        symbol = GetProcAddress((HMODULE)hndl.first, name);
        if (symbol != nullptr) {
          break;
        }
      }
    }

#ifdef _MSC_VER
#pragma warning( suppress: 4054 )
#endif
    return (void*)symbol;
  }

  const char *dlerror(void)
  {
    std::lock_guard<std::mutex> lock(internal_mutex_);

    error_str_ = current_error_;

    current_error_ = "";

    return error_str_.c_str();
  }

  void *dlopen(const char *file, int mode) {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    HMODULE object;
    char * current_error = NULL;
    uint32_t uMode = SetErrorMode(SEM_FAILCRITICALERRORS);
    if (nullptr == file)
    {
      HMODULE allModules[1024];
      HANDLE current_process_id = GetCurrentProcess();
      DWORD cbNeeded;
      object = GetModuleHandle(NULL);

      if (!object)
      store_error();
      if (EnumProcessModules(current_process_id, allModules,
              sizeof(allModules), &cbNeeded) != 0)
      {

        for (uint32_t i = 0; i < cbNeeded / sizeof(HMODULE); i++)
        {
          TCHAR szModName[MAX_PATH];

          // Get the full path to the module's file.
          resource_mapping_.insert(std::make_pair(allModules[i], "minifi-system"));
        }
      }
    }
    else
    {
      char lpFileName[MAX_PATH];
      int i;

      for (i = 0; i < sizeof(lpFileName) - 1; i++)
      {
        if (!file[i])
        break;
        else if (file[i] == '/')
        lpFileName[i] = '\\';
        else
        lpFileName[i] = file[i];
      }
      lpFileName[i] = '\0';
      object = LoadLibraryEx(lpFileName, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
      if (!object)
      store_error();
      else if ((mode & RTLD_GLOBAL))
      resource_mapping_.insert(std::make_pair(object, lpFileName));
    }

    /* Return to previous state of the error-mode bit flags. */
    SetErrorMode(uMode);

    return (void *)object;

  }

  int dlclose(void *handle)
  {
    std::lock_guard<std::mutex> lock(internal_mutex_);

    HMODULE object = (HMODULE)handle;
    BOOL ret;

    current_error_ = "";
    ret = FreeLibrary(object);

    resource_mapping_.erase(object);

    ret = !ret;

    return (int)ret;
  }

#endif

  inline jclass find_class_global(JNIEnv* env, const char *name) {
    jclass c = env->FindClass(name);
    jclass c_global = (jclass) env->NewGlobalRef(c);
    if (!c) {
	    std::stringstream ss;
	    ss << "Could not find " << name;
      throw std::runtime_error(ss.str());
    }
    return c_global;
  }

  void initialize(const std::vector<std::string> &opts) {
    string_options_ = opts;
    java_options_ = new JavaVMOption[opts.size()];
    int i = 0;
    for (const auto &opt : string_options_) {
      java_options_[i++].optionString = const_cast<char*>(opt.c_str());
    }

    JavaVMInitArgs vm_args;
    // rely on 1.8 and above
    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = opts.size();
    vm_args.options = java_options_;
    // if we see an unrecognized option fail.
    vm_args.ignoreUnrecognized = JNI_FALSE;
    // load and initialize a Java VM, return a JNI interface
    // pointer in env
    JNI_CreateJavaVM(&jvm_, (void**) &env_, &vm_args);
    // we're actually using a known class to locate the class loader and provide it
    // to referentially perform lookups.
    auto randomClass = find_class_global(env_, "org/apache/nifi/processor/ProcessContext");
    jclass classClass = env_->GetObjectClass(randomClass);
    auto classLoaderClass = find_class_global(env_, "java/lang/ClassLoader");
    auto getClassLoaderMethod = env_->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    auto refclazz = env_->CallObjectMethod(randomClass, getClassLoaderMethod);
    minifi::jni::ThrowIf(env_);
    gClassLoader = env_->NewGlobalRef(refclazz);
    gFindClassMethod = env_->GetMethodID(classLoaderClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    minifi::jni::ThrowIf(env_);
    initialized_ = true;
  }

 private:

  std::atomic<bool> initialized_;

  std::map<std::string, JavaClass> objects_;

  std::vector<std::string> string_options_;

  JavaVMOption* java_options_;

  JavaVM *jvm_;
  JNIEnv *env_;

  jobject gClassLoader;
  jmethodID gFindClassMethod;

  std::shared_ptr<JavaServicer> java_servicer_;

  JVMLoader()
      : java_options_(nullptr),
        jvm_(nullptr),
        env_(nullptr),
        java_servicer_(nullptr),
        gFindClassMethod(nullptr),
        gClassLoader(nullptr) {
    initialized_ = false;
  }

  ~JVMLoader() {
    if (java_options_) {
      delete[] java_options_;
    }
  }
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_JVMLOADER_H */
