/**
 * @file GenerateFlowFile.cpp
 * GenerateFlowFile class implementation
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
#include <set>
#include <sys/time.h>
#include <time.h>
#include <chrono>
#include <thread>
#include <random>
#include "utils/StringUtils.h"

#include "processors/GenerateFlowFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
const char *GenerateFlowFile::DATA_FORMAT_BINARY = "Binary";
const char *GenerateFlowFile::DATA_FORMAT_TEXT = "Text";
const std::string GenerateFlowFile::ProcessorName("GenerateFlowFile");
core::Property GenerateFlowFile::FileSize("File Size", "The size of the file that will be used", "1 kB");
core::Property GenerateFlowFile::BatchSize("Batch Size", "The number of FlowFiles to be transferred in each invocation", "1");
core::Property GenerateFlowFile::DataFormat("Data Format", "Specifies whether the data should be Text or Binary", GenerateFlowFile::DATA_FORMAT_BINARY);
core::Property GenerateFlowFile::UniqueFlowFiles("Unique FlowFiles",
		"If true, each FlowFile that is generated will be unique. If false, a random value will be generated and all FlowFiles", "true");
core::Relationship GenerateFlowFile::Success("success", "success operational on the flow record");

void GenerateFlowFile::initialize()
{
	// Set the supported properties
	std::set<core::Property> properties;
	properties.insert(FileSize);
	properties.insert(BatchSize);
	properties.insert(DataFormat);
	properties.insert(UniqueFlowFiles);
	setSupportedProperties(properties);
	// Set the supported relationships
	std::set<core::Relationship> relationships;
	relationships.insert(Success);
	setSupportedRelationships(relationships);
}

void GenerateFlowFile::onTrigger(core::ProcessContext *context, core::ProcessSession *session)
{
	int64_t batchSize = 1;
	bool uniqueFlowFile = true;
	int64_t fileSize = 1024;

	std::string value;
	if (context->getProperty(FileSize.getName(), value))
	{
	  core::Property::StringToInt(value, fileSize);
	}
	if (context->getProperty(BatchSize.getName(), value))
	{
	  core::Property::StringToInt(value, batchSize);
	}
	if (context->getProperty(UniqueFlowFiles.getName(), value))
	{
		org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, uniqueFlowFile);
	}

	if (!uniqueFlowFile)
	{
		char *data;
		data = new char[fileSize];
		if (!data)
			return;
		uint64_t dataSize = fileSize;
		GenerateFlowFile::WriteCallback callback(data, dataSize);
		char *current = data;
		for (int i = 0; i < fileSize; i+= sizeof(int))
		{
			int randValue = random();
			*((int *) current) = randValue;
			current += sizeof(int);
		}
		for (int i = 0; i < batchSize; i++)
		{
			// For each batch
			std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->create());
			if (!flowFile)
				return;
			if (fileSize > 0)
				session->write(flowFile, &callback);
			session->transfer(flowFile, Success);
		}
		delete[] data;
	}
	else
	{
		if (!_data)
		{
			// We have not create the unique data yet
			_data = new char[fileSize];
			_dataSize = fileSize;
			char *current = _data;
			for (int i = 0; i < fileSize; i+= sizeof(int))
			{
				int randValue = random();
				*((int *) current) = randValue;
				// *((int *) current) = (0xFFFFFFFF & i);
				current += sizeof(int);
			}
		}
		GenerateFlowFile::WriteCallback callback(_data, _dataSize);
		for (int i = 0; i < batchSize; i++)
		{
			// For each batch
			std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->create());
			if (!flowFile)
				return;
			if (fileSize > 0)
				session->write(flowFile, &callback);
			session->transfer(flowFile, Success);
		}
	}
}
} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
