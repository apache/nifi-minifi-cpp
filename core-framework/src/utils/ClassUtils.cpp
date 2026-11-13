// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/ClassUtils.h"

#include <iostream>
#include <string>

#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils {

bool ClassUtils::shortenClassName(std::string_view class_name, std::string &out) {
  std::string class_delim = "::";
  auto class_split = utils::string::split(class_name, class_delim);
  // support . and ::
  if (class_split.size() <= 1) {
    if (class_name.find('.') != std::string::npos) {
      class_delim = ".";
      class_split = utils::string::split(class_name, class_delim);
    } else {
      // if no update can be performed, return false to let the developer know
      // this. Out will have no updates
      return false;
    }
  }
  for (auto &elem : class_split) {
    if (&elem != &class_split.back() && elem.size() > 1) {
      elem = elem.substr(0, 1);
    }
  }

  out = utils::string::join(class_delim, class_split);
  return true;
}

}  // namespace org::apache::nifi::minifi::utils
