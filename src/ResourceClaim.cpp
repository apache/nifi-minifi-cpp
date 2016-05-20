/**
 * @file ResourceClaim.cpp
 * ResourceClaim class implementation
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
#include <vector>
#include <queue>
#include <map>

#include "ResourceClaim.h"

std::atomic<uint64_t> ResourceClaim::_localResourceClaimNumber(0);

ResourceClaim::ResourceClaim(const std::string contentDirectory)
: _id(_localResourceClaimNumber.load()),
  _flowFileRecordOwnedCount(0)
{
	char uuidStr[37];

	// Generate the global UUID for the resource claim
	uuid_generate(_uuid);
	// Increase the local ID for the resource claim
	++_localResourceClaimNumber;
	uuid_parse(uuidStr, _uuid);
	// Create the full content path for the content
	_contentFullPath = contentDirectory + "/" + uuidStr;
}
