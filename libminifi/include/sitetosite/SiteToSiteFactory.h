// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "SiteToSite.h"
#include "Peer.h"
#include "SiteToSiteClient.h"

namespace org::apache::nifi::minifi::sitetosite {

gsl::not_null<std::unique_ptr<SiteToSiteClient>> createClient(const SiteToSiteClientConfiguration &client_configuration);

}  // namespace org::apache::nifi::minifi::sitetosite
