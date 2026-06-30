use crate::controller_services::private_key_service::PGPPrivateKeyService;
use crate::processors::decrypt_content::{DecryptContentPGP, output_attributes};
use crate::test_utils;
use crate::test_utils::get_test_message;
use minifi_native::{
    ComponentIdentifier, EnableControllerService, FlowFileStreamTransform, IoState,
    MockControllerServiceContext, MockLogger, MockProcessContext, Schedule,
};

