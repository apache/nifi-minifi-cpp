// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "PythonProcessor.h"

#include "ExecutePythonProcessor.h"

namespace org::apache::nifi::minifi::extensions::python {

PythonProcessor::PythonProcessor(python::processors::ExecutePythonProcessor* proc) :
    processor_(proc) {
  gsl_Expects(processor_);
}

void PythonProcessor::setSupportsDynamicProperties() {
  processor_->setSupportsDynamicProperties();
}

void PythonProcessor::setSingleThreaded() {
  processor_->setSingleThreaded();
}

void PythonProcessor::setDescription(const std::string& desc) {
  processor_->setDescription(desc);
}

void PythonProcessor::setVersion(const std::string& version) {
  processor_->setVersion(version);
}

void PythonProcessor::addProperty(const std::string& name, const std::string& description, const std::optional<std::string>& defaultvalue, bool required, bool el, bool sensitive,
    const std::optional<int64_t>& property_type_code, std::span<const std::string_view> allowable_values, const std::optional<std::string>& controller_service_type_name) {
  processor_->addProperty(name, description, defaultvalue, required, el, sensitive, property_type_code, allowable_values, controller_service_type_name);
}

}  // namespace org::apache::nifi::minifi::extensions::python
