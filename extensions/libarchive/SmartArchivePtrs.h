// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
