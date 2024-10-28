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

#include <memory>
#include "archive.h"
#include "archive_entry.h"

namespace org::apache::nifi::minifi::processors {

struct archive_write_unique_ptr_deleter {
  void operator()(archive* arch) const noexcept { archive_write_free(arch); }
};

struct archive_read_unique_ptr_deleter {
    void operator()(archive* arch) const noexcept { archive_read_free(arch); }
};

struct archive_entry_unique_ptr_deleter {
  void operator()(archive_entry* arch_entry) const noexcept { archive_entry_free(arch_entry); }
};

using archive_write_unique_ptr = std::unique_ptr<archive, archive_write_unique_ptr_deleter>;
using archive_read_unique_ptr = std::unique_ptr<archive, archive_read_unique_ptr_deleter>;
using archive_entry_unique_ptr = std::unique_ptr<archive_entry, archive_entry_unique_ptr_deleter>;
}  // namespace org::apache::nifi::minifi::processors
