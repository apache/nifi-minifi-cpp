mod controller_services;
mod processors;

use crate::controller_services::private_key_service::PGPPrivateKeyService;
use crate::controller_services::public_key_service::PGPPublicKeyService;
use crate::processors::decrypt_content::DecryptContentPGP;
use crate::processors::encrypt_content::EncryptContentPGP;
use minifi_native::{FlowFileStreamTransformProcessorType, MultiThreaded};

minifi_native::declare_minifi_extension!(
    processors: [
        (FlowFileStreamTransformProcessorType, MultiThreaded, EncryptContentPGP),
        (FlowFileStreamTransformProcessorType, MultiThreaded, DecryptContentPGP),
    ],
    controllers: [
        PGPPublicKeyService,
        PGPPrivateKeyService,
    ]
);

#[cfg(test)]
mod test_utils;
mod utils;
