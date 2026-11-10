// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <ExtractText.h>
#include <GetFile.h>
#include <PutFile.h>
#include <UpdateAttribute.h>
#include <LogAttribute.h>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/ProvenanceTestHelper.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("GetFile PutFile dynamic attribute", "[expressionLanguageTestGetFilePutFileDynamicAttribute]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::PutFile>();
  LogTestController::getInstance().setTrace<minifi::processors::ExtractText>();
  LogTestController::getInstance().setTrace<minifi::processors::GetFile>();
  LogTestController::getInstance().setTrace<minifi::processors::PutFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<minifi::processors::UpdateAttribute>();

  auto conf = std::make_shared<minifi::ConfigureImpl>();

  conf->set("nifi.my.own.property", "custom_value");

  auto plan = testController.createPlan(conf);

  auto in_dir = testController.createTempDirectory();
  REQUIRE(!in_dir.empty());

  auto in_file = in_dir / "file";
  auto out_dir = testController.createTempDirectory();
  REQUIRE(!out_dir.empty());

  auto out_file = out_dir / "extracted_attr" / "file";

  // Build MiNiFi processing graph
  auto get_file = plan->addProcessor("GetFile", "GetFile");
  plan->setProperty(get_file, minifi::processors::GetFile::Directory, in_dir.string());
  plan->setProperty(get_file, minifi::processors::GetFile::KeepSourceFile, "false");
  auto update = plan->addProcessor("UpdateAttribute", "UpdateAttribute", core::Relationship("success", "description"), true);
  REQUIRE(update->setDynamicProperty("prop_attr", "${'nifi.my.own.property'}_added"));
  plan->addProcessor("LogAttribute", "LogAttribute", core::Relationship("success", "description"), true);
  auto extract_text = plan->addProcessor("ExtractText", "ExtractText", core::Relationship("success", "description"), true);
  plan->setProperty(extract_text, minifi::processors::ExtractText::Attribute, "extracted_attr_name");
  plan->addProcessor("LogAttribute", "LogAttribute", core::Relationship("success", "description"), true);
  auto put_file = plan->addProcessor("PutFile", "PutFile", core::Relationship("success", "description"), true);
  plan->setProperty(put_file, minifi::processors::PutFile::Directory, (out_dir / "${extracted_attr_name}").string());
  plan->setProperty(put_file, minifi::processors::PutFile::ConflictResolution, magic_enum::enum_name(minifi::processors::PutFile::FileExistsResolutionStrategy::replace));
  plan->setProperty(put_file, minifi::processors::PutFile::CreateDirs, "true");

  // Write test input
  {
    std::ofstream in_file_stream(in_file);
    in_file_stream << "extracted_attr";
  }

  plan->runNextProcessor();  // GetFile
  plan->runNextProcessor();  // Update
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // ExtractText
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // PutFile

  // Verify output
  {
    std::stringstream output_str;
    std::ifstream out_file_stream(out_file);
    output_str << out_file_stream.rdbuf();
    REQUIRE("extracted_attr" == output_str.str());
  }

  REQUIRE(LogTestController::getInstance().contains("key:prop_attr value:custom_value_added"));
}

}  // namespace org::apache::nifi::minifi::test
