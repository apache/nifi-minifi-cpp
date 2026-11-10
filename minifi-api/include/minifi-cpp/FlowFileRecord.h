// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>
#include "minifi-cpp/core/ContentRepository.h"
#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/Repository.h"
#include "minifi-cpp/io/OutputStream.h"

namespace org::apache::nifi::minifi {

class FlowFileRecord : public virtual core::FlowFile {
 public:
  virtual bool Serialize(io::OutputStream &outStream) = 0;

  //! Serialize and Persistent to the repository
  virtual bool Persist(const std::shared_ptr<core::Repository>& flowRepository) = 0;

  static std::shared_ptr<FlowFileRecord> DeSerialize(std::span<const std::byte> buffer, const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier &container);
  static std::shared_ptr<FlowFileRecord> DeSerialize(io::InputStream &stream, const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier &container);
  static std::shared_ptr<FlowFileRecord> DeSerialize(const std::string& key, const std::shared_ptr<core::Repository>& flowRepository,
                                                     const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier &container);

  virtual std::string getContentFullPath() const = 0;
};

}  // namespace org::apache::nifi::minifi
