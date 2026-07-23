// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

mod controller_services;
mod processors;

use crate::controller_services::dog_controller_service::DogControllerRs;
use crate::controller_services::duck_controller_service::DuckControllerRs;
use crate::controller_services::dummy_controller_service::DummyControllerService;
use crate::controller_services::lorem_ipsum_controller_service::LoremIpsumControllerService;
use crate::processors::asciify_german::AsciifyGerman;
use crate::processors::count_actual_logging::CountActualLogging;
use crate::processors::duplicate_text::DuplicateStreamText;
use crate::processors::generate_flow_file::GenerateFlowFileRs;
use crate::processors::get_file::GetFileRs;
use crate::processors::kamikaze_processor::KamikazeProcessorRs;
use crate::processors::log_attribute::LogAttributeRs;
use crate::processors::lorem_ipsum_cs_user::LoremIpsumCSUser;
use crate::processors::put_file::PutFileRs;
use crate::processors::zoo_processor::ZooProcessorRs;

use minifi_native::{
    ComplexProcessorType, FlowFileSourceProcessorType, FlowFileStreamTransformProcessorType,
    FlowFileTransformProcessorType, MultiThreaded, SingleThreaded,
};

minifi_native::declare_minifi_extension!(
    processors: [
        (ComplexProcessorType, MultiThreaded, GenerateFlowFileRs),
        (ComplexProcessorType, MultiThreaded, LogAttributeRs),
        (ComplexProcessorType, MultiThreaded, GetFileRs),
        (ComplexProcessorType, MultiThreaded, KamikazeProcessorRs),
        (ComplexProcessorType, SingleThreaded, CountActualLogging),
        (FlowFileSourceProcessorType, MultiThreaded, LoremIpsumCSUser),
        (FlowFileTransformProcessorType, MultiThreaded, PutFileRs),
        (FlowFileStreamTransformProcessorType, MultiThreaded, AsciifyGerman),
        (FlowFileStreamTransformProcessorType, SingleThreaded, DuplicateStreamText),
        (ComplexProcessorType, MultiThreaded, ZooProcessorRs),
    ],
    controllers: [
        LoremIpsumControllerService,
        DummyControllerService,
        DogControllerRs,
        DuckControllerRs
    ]
);
