mod controller_services;
mod processors;

use crate::controller_services::private_key_service::PGPPrivateKeyService;
use crate::controller_services::public_key_service::PGPPublicKeyService;
use crate::processors::decrypt_content::DecryptContentPGP;
use crate::processors::encrypt_content::EncryptContentPGP;
use minifi_native::{Concurrent, FlowFileStreamTransformProcessorType};

minifi_native::declare_minifi_extension!(
    processors: [
        (FlowFileStreamTransformProcessorType, Concurrent, EncryptContentPGP),
        (FlowFileStreamTransformProcessorType, Concurrent, DecryptContentPGP),
    ],
    controllers: [
        PGPPublicKeyService,
        PGPPrivateKeyService,
    ]
);

#[cfg(test)]
mod test_utils;
