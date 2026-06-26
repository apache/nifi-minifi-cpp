mod controller_services;
mod processors;

use crate::controller_services::dog_controller_service::DogController;
use crate::controller_services::duck_controller_service::DuckController;
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
    ComplexProcessorType, Concurrent, Exclusive, FlowFileSourceProcessorType,
    FlowFileStreamTransformProcessorType, FlowFileTransformProcessorType,
};

minifi_native::declare_minifi_extension!(
processors: [
    (ComplexProcessorType, Concurrent, GenerateFlowFileRs),
    (ComplexProcessorType, Concurrent, LogAttributeRs),
    (ComplexProcessorType, Concurrent, GetFileRs),
    (ComplexProcessorType, Concurrent, KamikazeProcessorRs),
    (ComplexProcessorType, Exclusive, CountActualLogging),
    (FlowFileSourceProcessorType, Concurrent, LoremIpsumCSUser),
    (FlowFileTransformProcessorType, Concurrent, PutFileRs),
    (FlowFileStreamTransformProcessorType, Concurrent, AsciifyGerman),
    (FlowFileStreamTransformProcessorType, Exclusive, DuplicateStreamText),
    (ComplexProcessorType, Concurrent, ZooProcessorRs),
],
controllers: [
    LoremIpsumControllerService,
    DummyControllerService,
    DogController,
    DuckController
]
);
