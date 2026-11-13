// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <string_view>

namespace org::apache::nifi::minifi::utils::ClassUtils {

/**
 * Shortens class names via the canonical representation ( package with name )
 * @param class_name input class name
 * @param out output class name that is shortened.
 * @return true if out has been updated, false otherwise
 */
bool shortenClassName(std::string_view class_name, std::string &out);

}  // namespace org::apache::nifi::minifi::utils::ClassUtils
