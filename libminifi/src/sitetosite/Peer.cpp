// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "sitetosite/Peer.h"

#include "io/validation.h"
#include "utils/net/AsioSocketUtils.h"

namespace org::apache::nifi::minifi::sitetosite {

bool SiteToSitePeer::open() {
  if (IsNullOrEmpty(host_)) {
    return false;
  }

  /**
   * We may override the interface provided to us within the socket in this step; however, this is a
   * known configuration path, and thus we will allow the RPG configuration to override anything provided to us
   * previously by the socket preference.
   */
  if (!local_network_interface_.getInterface().empty()) {
    if (auto socket = dynamic_cast<utils::net::AsioSocketConnection*>(stream_.get())) {
      socket->setInterface(local_network_interface_.getInterface());
    }
  }

  if (stream_->initialize() < 0) {
    return false;
  }

  return stream_->write(reinterpret_cast<const uint8_t *>(MAGIC_BYTES.data()), MAGIC_BYTES.size()) == MAGIC_BYTES.size();
}

void SiteToSitePeer::close() {
  if (stream_) {
    stream_->close();
  }
}

}  // namespace org::apache::nifi::minifi::sitetosite
