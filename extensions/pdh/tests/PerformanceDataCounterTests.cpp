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

#include "TestBase.h"
#include "PDHCounters.h"
#include "MemoryConsumptionCounter.h"

using PDHCounter = org::apache::nifi::minifi::processors::PDHCounter;
using PDHCounterArray = org::apache::nifi::minifi::processors::PDHCounterArray;
using PDHCounterBase = org::apache::nifi::minifi::processors::PDHCounterBase;
using MemoryConsumptionCounter = org::apache::nifi::minifi::processors::MemoryConsumptionCounter;


TEST_CASE("PDHCounterNameTests", "[pdhcounternametests]") {
  PDHCounterBase* test_counter = PDHCounterBase::createPDHCounter("\\System\\Threads");
  REQUIRE(nullptr != dynamic_cast<PDHCounter*> (test_counter));
  REQUIRE("\\System\\Threads" == test_counter->getName());
  REQUIRE("System" == test_counter->getObjectName());
  REQUIRE("Threads" == test_counter->getCounterName());
}

TEST_CASE("PDHCounterArrayNameTests", "[pdhcounterarraytests]") {
  PDHCounterBase* test_counter_array = PDHCounterBase::createPDHCounter("\\LogicalDisk(*)\\% Free Space");
  REQUIRE(nullptr != dynamic_cast<PDHCounterArray*> (test_counter_array));
  REQUIRE("\\LogicalDisk(*)\\% Free Space" == test_counter_array->getName());
  REQUIRE("LogicalDisk" == test_counter_array->getObjectName());
  REQUIRE("% Free Space" == test_counter_array->getCounterName());
}

TEST_CASE("PDHCountersInvalidNameTests", "[pdhcountersinvalidnametests]") {
  REQUIRE(nullptr == PDHCounterBase::createPDHCounter("Invalid Name"));
  REQUIRE(nullptr == PDHCounterBase::createPDHCounter(""));
  REQUIRE(nullptr == PDHCounterBase::createPDHCounter("Something\\Counter"));
  REQUIRE(nullptr == PDHCounterBase::createPDHCounter("\\Too\\Many\\Separators"));
  REQUIRE(nullptr == PDHCounterBase::createPDHCounter("Too\\Many\\Separators"));
  REQUIRE(nullptr != PDHCounterBase::createPDHCounter("\\Valid\\Counter"));
}

class TestablePDHCounter : public PDHCounter {
 public:
  explicit TestablePDHCounter(const std::string& query_name, bool is_double = true) : PDHCounter(query_name, is_double) {
  }
};

class TestablePDHCounterArray : public PDHCounterArray {
 public:
  explicit TestablePDHCounterArray(const std::string& query_name, bool is_double = true) : PDHCounterArray(query_name, is_double) {
  }
};

TEST_CASE("PDHCountersAddingToQueryTests", "[pdhcountersaddingtoquerytests]") {
  PDH_HQUERY pdh_query;
  PdhOpenQuery(NULL, NULL, &pdh_query);
  PDH_HQUERY unopened_pdh_query;

  TestablePDHCounter valid_counter("\\System\\Threads");
  REQUIRE(ERROR_SUCCESS == valid_counter.addToQuery(pdh_query));
  REQUIRE_FALSE(ERROR_SUCCESS == valid_counter.addToQuery(unopened_pdh_query));

  TestablePDHCounter counter_with_invalid_object_name("\\Invalid\\Threads");
  REQUIRE(PDH_CSTATUS_NO_OBJECT == counter_with_invalid_object_name.addToQuery(pdh_query));

  TestablePDHCounter counter_with_invalid_counter_name("\\System\\Invalid");
  REQUIRE(PDH_CSTATUS_NO_COUNTER == counter_with_invalid_counter_name.addToQuery(pdh_query));

  TestablePDHCounter unparsable_counter("asd");  // Unparsable names are also filtered when using PDHCounterBase::createPDHCounter
  REQUIRE(PDH_CSTATUS_BAD_COUNTERNAME == unparsable_counter.addToQuery(pdh_query));

  PdhCloseQuery(&pdh_query);
}

TEST_CASE("PDHCounterArraysAddingToQueryTests", "[pdhcounterarraysaddingtoquerytests]") {
  PDH_HQUERY pdh_query;
  PdhOpenQuery(NULL, NULL, &pdh_query);
  PDH_HQUERY unopened_pdh_query;

  TestablePDHCounterArray valid_counter("\\Processor(*)\\% Processor Time");
  REQUIRE(ERROR_SUCCESS == valid_counter.addToQuery(pdh_query));
  REQUIRE_FALSE(ERROR_SUCCESS == valid_counter.addToQuery(unopened_pdh_query));

  TestablePDHCounterArray counter_with_invalid_object_name("\\SomethingInvalid(*)\\% Processor Time");
  REQUIRE(PDH_CSTATUS_NO_OBJECT == counter_with_invalid_object_name.addToQuery(pdh_query));

  TestablePDHCounterArray counter_with_invalid_counter_name("\\Processor(*)\\SomethingInvalid");
  REQUIRE(PDH_CSTATUS_NO_COUNTER == counter_with_invalid_counter_name.addToQuery(pdh_query));

  TestablePDHCounterArray unparsable_counter("asd");  // Unparsable names are also filtered when using PDHCounterBase::createPDHCounter
  REQUIRE(PDH_CSTATUS_BAD_COUNTERNAME == unparsable_counter.addToQuery(pdh_query));

  PdhCloseQuery(&pdh_query);
}

TEST_CASE("PDHCounterDataCollectionTest", "[pdhcounterdatacollectiontest]") {
  PDH_HQUERY pdh_query;
  PdhOpenQuery(NULL, NULL, &pdh_query);

  TestablePDHCounter double_counter("\\System\\Threads");
  TestablePDHCounter int_counter("\\System\\Processes", false);
  REQUIRE(ERROR_SUCCESS == double_counter.addToQuery(pdh_query));
  REQUIRE(ERROR_SUCCESS == int_counter.addToQuery(pdh_query));

  PdhCollectQueryData(pdh_query);

  REQUIRE(ERROR_SUCCESS == double_counter.collectData());
  REQUIRE(ERROR_SUCCESS == int_counter.collectData());

  rapidjson::Document document(rapidjson::kObjectType);
  double_counter.addToJson(document, document.GetAllocator());
  int_counter.addToJson(document, document.GetAllocator());

  REQUIRE(document.HasMember("System"));
  REQUIRE(document["System"].HasMember("Threads"));
  REQUIRE(document["System"]["Threads"].IsDouble());
  REQUIRE(document["System"]["Threads"].GetDouble() > 0);
  REQUIRE(document["System"]["Processes"].IsInt64());
  REQUIRE(document["System"]["Processes"].GetInt64() > 0);

  PdhCloseQuery(&pdh_query);
}

TEST_CASE("PDHCounterArrayDataCollectionTest", "[pdhcounterarraydatacollectiontest]") {
  PDH_HQUERY pdh_query;
  PdhOpenQuery(NULL, NULL, &pdh_query);

  TestablePDHCounterArray double_counter_array("\\Process(*)\\Thread Count");
  TestablePDHCounterArray int_counter_array("\\Process(*)\\ID Process", false);
  REQUIRE(ERROR_SUCCESS == double_counter_array.addToQuery(pdh_query));
  REQUIRE(ERROR_SUCCESS == int_counter_array.addToQuery(pdh_query));

  PdhCollectQueryData(pdh_query);

  double_counter_array.collectData();
  int_counter_array.collectData();
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  REQUIRE(ERROR_SUCCESS == double_counter_array.collectData());
  REQUIRE(ERROR_SUCCESS == int_counter_array.collectData());

  rapidjson::Document document(rapidjson::kObjectType);
  double_counter_array.addToJson(document, document.GetAllocator());
  int_counter_array.addToJson(document, document.GetAllocator());

  REQUIRE(document.HasMember("Process"));
  REQUIRE(document["Process"].HasMember("PerformanceDataCounterTests"));
  REQUIRE(document["Process"]["PerformanceDataCounterTests"].HasMember("Thread Count"));
  REQUIRE(document["Process"]["PerformanceDataCounterTests"].HasMember("ID Process"));
  REQUIRE(document["Process"]["PerformanceDataCounterTests"]["Thread Count"].IsDouble());
  REQUIRE(document["Process"]["PerformanceDataCounterTests"]["ID Process"].IsInt64());

  PdhCloseQuery(&pdh_query);
}

TEST_CASE("MemoryConsumptionCounterTest", "[memoryconsumptioncountertest]") {
  MemoryConsumptionCounter memory_counter;

  rapidjson::Document document(rapidjson::kObjectType);
  memory_counter.addToJson(document, document.GetAllocator());

  REQUIRE(document.HasMember("Memory"));
  REQUIRE(document["Memory"].HasMember("Total Physical Memory"));
  REQUIRE(document["Memory"].HasMember("Available Physical Memory"));
  REQUIRE(document["Memory"].HasMember("Total paging file size"));

  REQUIRE(document["Memory"]["Total Physical Memory"].IsInt64());
  REQUIRE(document["Memory"]["Available Physical Memory"].IsInt64());
  REQUIRE(document["Memory"]["Total paging file size"].IsInt64());

  REQUIRE(document["Memory"]["Total Physical Memory"].GetInt64() > 0);
  REQUIRE(document["Memory"]["Available Physical Memory"].GetInt64() > 0);
  REQUIRE(document["Memory"]["Total paging file size"].GetInt64() > 0);
}
