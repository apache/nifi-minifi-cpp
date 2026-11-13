// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "StringAppender.h"
#include "rocksdb/utilities/object_registry.h"

namespace org::apache::nifi::minifi::core::repository {

bool StringAppender::Merge(const rocksdb::Slice& /*key*/, const rocksdb::Slice* existing_value, const rocksdb::Slice& value, std::string* new_value, rocksdb::Logger* /*logger*/) const {
  // Clear the *new_value for writing.
  if (nullptr == new_value) {
    return false;
  }
  new_value->clear();

  if (!existing_value) {
    // No existing_value. Set *new_value = value
    new_value->assign(value.data(), value.size());
  } else {
    new_value->reserve(existing_value->size() + value.size());
    new_value->assign(existing_value->data(), existing_value->size());
    new_value->append(value.data(), value.size());
  }

  return true;
}

static auto string_appender_registrar = rocksdb::ObjectLibrary::Default()->AddFactory<StringAppender>(
    "StringAppender",
    [] (const std::string& /* uri */, std::unique_ptr<StringAppender>* out, std::string* /* errmsg */) {
      *out = std::make_unique<StringAppender>();
      return out->get();
    });

}  // namespace org::apache::nifi::minifi::core::repository
