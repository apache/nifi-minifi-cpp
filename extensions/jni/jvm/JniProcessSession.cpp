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

#include "JniProcessSession.h"

#include <string>
#include <memory>
#include <algorithm>
#include <iterator>
#include <set>
#include "core/Property.h"
#include "io/validation.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "properties/Configure.h"
#include "JVMLoader.h"
#include "JniReferenceObjects.h"

#include "core/Processor.h"
#include "JniFlowFile.h"
#include "../JavaException.h"

#ifdef __cplusplus
extern "C" {
#endif

jobject Java_org_apache_nifi_processor_JniProcessSession_create(JNIEnv *env, jobject obj) {
  if (obj == nullptr) {
    return nullptr;
  }
  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);

  auto ff = minifi::jni::JVMLoader::getInstance()->load_class("org/apache/nifi/processor/JniFlowFile", env);

  auto ff_instance = ff.newInstance(env);

  minifi::jni::ThrowIf(env);

  auto flow_file = session->getSession()->create();

  auto flow = std::make_shared<minifi::jni::JniFlowFile>(flow_file, session->getServicer(), ff_instance);

  flow_file->addReference(flow);

  auto rawFlow = session->addFlowFile(flow);

  minifi::jni::JVMLoader::getInstance()->setReference(ff_instance, env, rawFlow);

  return ff_instance;

}

jobject Java_org_apache_nifi_processor_JniProcessSession_readFlowFile(JNIEnv *env, jobject obj, jobject ff) {
  if (obj == nullptr) {
    return nullptr;
  }
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, ff);
  if (ptr->get()) {

    auto jincls = minifi::jni::JVMLoader::getInstance()->load_class("org/apache/nifi/processor/JniInputStream", env);

    auto jin = jincls.newInstance(env);

    minifi::jni::ThrowIf(env);

    std::unique_ptr<minifi::jni::JniByteInputStream> callback = std::unique_ptr<minifi::jni::JniByteInputStream>(new minifi::jni::JniByteInputStream(4096));

    session->getSession()->read(ptr->get(), callback.get());

    auto jniInpuStream = std::make_shared<minifi::jni::JniInputStream>(std::move(callback), jin, session->getServicer());

    session->addInputStream(jniInpuStream);

    minifi::jni::JVMLoader::getInstance()->setReference(jin, env, jniInpuStream.get());

    return jin;
  }

  return nullptr;

}

jint Java_org_apache_nifi_processor_JniInputStream_read(JNIEnv *env, jobject obj) {
  minifi::jni::JniInputStream *jin = minifi::jni::JVMLoader::getPtr<minifi::jni::JniInputStream>(env, obj);
  if (obj == nullptr) {
    // this technically can't happen per JNI specs
    return -1;
  }
  char value = 0;
  if (jin->read(value) > 0)
    return value;
  else
    return -1;
}

jint Java_org_apache_nifi_processor_JniInputStream_readWithOffset(JNIEnv *env, jobject obj, jbyteArray arr, jint offset, jint length) {
  minifi::jni::JniInputStream *jin = minifi::jni::JVMLoader::getPtr<minifi::jni::JniInputStream>(env, obj);
  if (obj == nullptr) {
    // this technically can't happen per JNI specs
    return -1;
  }
  return jin->read(env, arr, (int) offset, (int) length);
}

jboolean Java_org_apache_nifi_processor_JniProcessSession_write(JNIEnv *env, jobject obj, jobject ff, jbyteArray byteArray) {
  if (obj == nullptr) {
    return false;
  }

  THROW_IF((ff == nullptr || byteArray == nullptr), env, "No flowfile to write");

  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, ff);

  if (ptr->get()) {
    jbyte* buffer = env->GetByteArrayElements(byteArray, 0);
    jsize length = env->GetArrayLength(byteArray);

    minifi::jni::JniByteOutStream outStream(buffer, (size_t) length);
    session->getSession()->write(ptr->get(), &outStream);

    env->ReleaseByteArrayElements(byteArray, buffer, 0);

    return true;
  }

  return false;

}

jobject Java_org_apache_nifi_processor_JniProcessSession_clone(JNIEnv *env, jobject obj, jobject prevff) {
  if (obj == nullptr) {
    return nullptr;
  }
  THROW_IF_NULL(prevff, env, NO_FF_OBJECT);
  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, prevff);

  if (ptr->get()) {
    auto ff = minifi::jni::JVMLoader::getInstance()->load_class("org/apache/nifi/processor/JniFlowFile", env);

    auto ff_instance = ff.newInstance(env);

    minifi::jni::ThrowIf(env);

    auto flow_file = session->getSession()->clone(ptr->get());

    auto flow = std::make_shared<minifi::jni::JniFlowFile>(flow_file, session->getServicer(), ff_instance);

    flow_file->addReference(flow);

    auto rawFlow = session->addFlowFile(flow);

    minifi::jni::JVMLoader::getInstance()->setReference(ff_instance, env, rawFlow);

    return ff_instance;
  }

  return nullptr;

}

jobject Java_org_apache_nifi_processor_JniProcessSession_get(JNIEnv *env, jobject obj) {
  if (obj == nullptr)
    return nullptr;

  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);

  if (session == nullptr)
    return nullptr;

  auto flow_file = session->getSession()->get();

  auto prevFF = session->getFlowFileReference(flow_file);
  if (prevFF != nullptr) {
    return prevFF->getJniReference();
  }

  // otherwise create one
  auto ff = minifi::jni::JVMLoader::getInstance()->load_class("org/apache/nifi/processor/JniFlowFile");

  auto ff_instance = ff.newInstance(env);

  if (flow_file == nullptr || ff_instance == nullptr) {
    // this is an acceptable condition.
    return nullptr;
  }

  auto flow = std::make_shared<minifi::jni::JniFlowFile>(flow_file, session->getServicer(), ff_instance);

  flow_file->addReference(flow);

  auto rawFlow = session->addFlowFile(flow);

  minifi::jni::JVMLoader::getInstance()->setReference(ff_instance, env, rawFlow);

  return ff_instance;
}

jobject Java_org_apache_nifi_processor_JniProcessSession_putAttribute(JNIEnv *env, jobject obj, jobject ff, jstring key, jstring value) {
  if (obj == nullptr) {
    // does not mean an error should be thrown, rather we will let
    // Java do its thing as this is a condition of GC most likely
    return nullptr;
  }
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, ff);

  if (ff == nullptr || key == nullptr || value == nullptr) {
    return nullptr;
  }

  ptr->get()->addAttribute(JniStringToUTF(env, key), JniStringToUTF(env, value));

  return ff;

}

void Java_org_apache_nifi_processor_JniProcessSession_transfer(JNIEnv *env, jobject obj, jobject ff, jstring relationship) {
  if (obj == nullptr) {
    return;
  }
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, ff);
  core::Relationship rel(JniStringToUTF(env, relationship), "description");
  session->getSession()->transfer(ptr->get(), rel);
}

jstring Java_org_apache_nifi_processor_JniProcessSession_getPropertyValue(JNIEnv *env, jobject obj, jstring propertyName) {
  std::string value;
  if (obj == nullptr) {
    return env->NewStringUTF(value.c_str());
  }
  core::ProcessContext *context = minifi::jni::JVMLoader::getPtr<core::ProcessContext>(env, obj);
  std::string keystr = JniStringToUTF(env, propertyName);
  if (!context->getProperty(keystr, value)) {
    context->getDynamicProperty(keystr, value);
  };
  return env->NewStringUTF(value.c_str());
}

jobject Java_org_apache_nifi_processor_JniProcessSession_createWithParent(JNIEnv *env, jobject obj, jobject parent) {
  if (obj == nullptr) {
    // does not mean an error should be thrown, rather we will let
    // Java do its thing as this is a condition of GC most likely
    return nullptr;
  }
  THROW_IF_NULL(parent, env, NO_FF_OBJECT);
  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, parent);

  if (ptr->get()) {
    auto ff = minifi::jni::JVMLoader::getInstance()->load_class("org/apache/nifi/processor/JniFlowFile", env);

    auto ff_instance = ff.newInstance(env);

    minifi::jni::ThrowIf(env);

    auto flow_file = session->getSession()->create(ptr->get());

    auto flow = std::make_shared<minifi::jni::JniFlowFile>(flow_file, session->getServicer(), ff_instance);

    flow_file->addReference(flow);

    auto rawFlow = session->addFlowFile(flow);

    minifi::jni::JVMLoader::getInstance()->setReference(ff_instance, env, rawFlow);

    return ff_instance;
  }

  return nullptr;

}

void Java_org_apache_nifi_processor_JniProcessSession_commit(JNIEnv *env, jobject obj) {
  if (obj == nullptr) {
    // does not mean an error should be thrown, rather we will let
    // Java do its thing as this is a condition of GC most likely
    return;
  }
  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);
  try {
    session->getSession()->commit();
  } catch (const std::exception &e) {
    std::string error = "error while committing: ";
    error += e.what();
    minifi::jni::ThrowJava(env, error.c_str());
  } catch (...) {
    minifi::jni::ThrowJava(env, "error while commiting");
  }
}

void Java_org_apache_nifi_processor_JniProcessSession_rollback(JNIEnv *env, jobject obj) {
  if (obj == nullptr) {
    // does not mean an error should be thrown, rather we will let
    // Java do its thing as this is a condition of GC most likely
    return;
  }
  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);
  session->getSession()->rollback();
}

jobject Java_org_apache_nifi_processor_JniProcessSessionFactory_createSession(JNIEnv *env, jobject obj) {
  if (obj == nullptr) {
    // does not mean an error should be thrown, rather we will let
    // Java do its thing as this is a condition of GC most likely
    return nullptr;
  }
  minifi::jni::JniSessionFactory *sessionFactory = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSessionFactory>(env, obj);
  auto session_class = minifi::jni::JVMLoader::getInstance()->load_class("org/apache/nifi/processor/JniProcessSession", env);

  auto session_instance = session_class.newInstance(env);

  minifi::jni::ThrowIf(env);

  // create a session
  auto procSession = sessionFactory->getFactory()->createSession();

  std::shared_ptr<minifi::jni::JniSession> session = std::make_shared<minifi::jni::JniSession>(procSession, session_instance, sessionFactory->getServicer());

  // add a reference so the minifi C++ session factory knows to remove these eventually.
  procSession->addReference(session);

  auto rawSession = sessionFactory->addSession(session);

  // set the reference in session_instance using the raw pointer.
  minifi::jni::JVMLoader::getInstance()->setReference(session_instance, env, rawSession);

  // catalog the session

  return session_instance;
}

void Java_org_apache_nifi_processor_JniProcessSession_remove(JNIEnv *env, jobject obj, jobject ff) {
  if (obj == nullptr) {
    // does not mean an error should be thrown, rather we will let
    // Java do its thing as this is a condition of GC most likely
    return;
  }
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, ff);

  if (ptr->get()) {
    session->getSession()->remove(ptr->get());
  }

}

jobject Java_org_apache_nifi_processor_JniProcessSession_penalize(JNIEnv *env, jobject obj, jobject ff) {
  if (obj == nullptr) {
    // does not mean an error should be thrown, rather we will let
    // Java do its thing as this is a condition of GC most likely
    return ff;
  }
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, ff);

  if (ptr->get()) {
    session->getSession()->penalize(ptr->get());
  }
  return ff;
}

jobject Java_org_apache_nifi_processor_JniProcessSession_removeAttribute(JNIEnv *env, jobject obj, jobject ff, jstring attr) {
  if (obj == nullptr) {
    // does not mean an error should be thrown, rather we will let
    // Java do its thing as this is a condition of GC most likely
    return ff;
  }
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, ff);

  if (ptr->get()) {
    ptr->get()->removeAttribute(JniStringToUTF(env, attr));
  }
  return ff;
}

jobject Java_org_apache_nifi_processor_JniProcessSession_clonePortion(JNIEnv *env, jobject obj, jobject prevff, jlong offset, jlong size) {
  if (obj == nullptr) {
    // does not mean an error should be thrown, rather we will let
    // Java do its thing as this is a condition of GC most likely
    return prevff;
  }
  THROW_IF_NULL(prevff, env, NO_FF_OBJECT);
  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, prevff);

  if (ptr->get()) {
    auto ff = minifi::jni::JVMLoader::getInstance()->load_class("org/apache/nifi/processor/JniFlowFile", env);

    auto ff_instance = ff.newInstance(env);

    minifi::jni::ThrowIf(env);

    auto flow_file = session->getSession()->clone(ptr->get(), offset, size);

    auto flow = std::make_shared<minifi::jni::JniFlowFile>(flow_file, session->getServicer(), ff_instance);

    flow_file->addReference(flow);

    auto rawFlow = session->addFlowFile(flow);

    minifi::jni::JVMLoader::getInstance()->setReference(ff_instance, env, rawFlow);

    return ff_instance;
  }

  return nullptr;

}

jboolean Java_org_apache_nifi_processor_JniProcessSession_append(JNIEnv *env, jobject obj, jobject ff, jbyteArray byteArray) {
  if (obj == nullptr) {
    // does not mean an error should be thrown, rather we will let
    // Java do its thing as this is a condition of GC most likely
    return false;
  }
  THROW_IF((ff == nullptr || byteArray == nullptr), env, NO_FF_OBJECT);
  minifi::jni::JniSession *session = minifi::jni::JVMLoader::getPtr<minifi::jni::JniSession>(env, obj);
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, ff);

  if (ptr->get()) {
    jbyte* buffer = env->GetByteArrayElements(byteArray, 0);
    jsize length = env->GetArrayLength(byteArray);

    minifi::jni::JniByteOutStream outStream(buffer, (size_t) length);
    session->getSession()->append(ptr->get(), &outStream);

    env->ReleaseByteArrayElements(byteArray, buffer, 0);

    return true;
  }

  return false;

}

#ifdef __cplusplus
}
#endif
