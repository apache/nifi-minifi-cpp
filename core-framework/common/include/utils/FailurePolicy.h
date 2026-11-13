// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

namespace org::apache::nifi::minifi::utils {

/**
 * Basic failure policy enumeration
 *
 */
enum FailurePolicy {
  /**
   * DO NOTHING
   */
  NOTHING,
  /**
   * Return a response code from the executing function
   */
  RETURN,
  /**
   * Throw an exception for flow control.
   */
  EXCEPT,
  /**
   * Exit the program. This should only be used when something
   * precludes us from continuing
   */
  EXIT
};

}  // namespace org::apache::nifi::minifi::utils
