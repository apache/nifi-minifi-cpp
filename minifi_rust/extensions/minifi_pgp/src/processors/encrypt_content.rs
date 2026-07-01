use minifi_native::{
    FlowFileStreamTransform, GetAttribute, GetControllerService, GetProperty, InputStream, Logger,
    MinifiError, OutputStream, Schedule, TransformStreamResult, warn,
};
use pgp::composed::{ArmorOptions, MessageBuilder, SignedPublicKey};
use pgp::types::StringToKey;
use std::collections::HashMap;

mod output_attributes;
mod properties;
mod relationships;

use crate::controller_services::public_key_service::PGPPublicKeyService;
use crate::processors::encrypt_content::output_attributes::FILE_ENCODING;
use crate::processors::encrypt_content::properties::{
    PASSWORD, PUBLIC_KEY_SEARCH, PUBLIC_KEY_SERVICE,
};
use crate::processors::encrypt_content::relationships::{FAILURE, SUCCESS};
use minifi_native::macros::ComponentIdentifier;
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};

#[derive(Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr)]
#[strum(serialize_all = "UPPERCASE", const_into_str)]
enum FileEncoding {
    Ascii,
    Binary,
}

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct EncryptContentPGP {
    file_encoding: FileEncoding,
}

#[cfg(not(test))]
fn string_to_key() -> StringToKey {
    StringToKey::new_argon2(rand::thread_rng(), 3, 4, 16) // 64 MiB with rpgp's recommended parameter choice
}

#[cfg(test)]
fn string_to_key() -> StringToKey {
    StringToKey::new_argon2(rand::thread_rng(), 1, 1, 10) // fast for unit tests
}

impl EncryptContentPGP {
    fn encrypt_bytes(
        &self,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        pub_key: Option<&SignedPublicKey>,
        passphrase: Option<&str>,
        file_name: String,
    ) -> pgp::errors::Result<()> {
        let mut builder = MessageBuilder::from_reader(file_name, input_stream).seipd_v1(
            rand::thread_rng(),
            pgp::crypto::sym::SymmetricKeyAlgorithm::AES256,
        );

        if let Some(pub_key) = pub_key {
            builder.encrypt_to_key(rand::thread_rng(), pub_key)?;
        }

        if let Some(passphrase) = passphrase {
            builder.encrypt_with_password(string_to_key(), &passphrase.into())?;
        }

        match self.file_encoding {
            FileEncoding::Ascii => builder.to_armored_writer(
                rand::thread_rng(),
                ArmorOptions::default(),
                output_stream,
            ),
            FileEncoding::Binary => builder.to_writer(rand::thread_rng(), output_stream),
        }
    }
}

impl Schedule for EncryptContentPGP {
    fn schedule<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let file_encoding = context
            .get_property(&properties::FILE_ENCODING)?
            .expect("required property")
            .parse::<FileEncoding>()?;

        let has_password = context.get_property(&PASSWORD)?.is_some();
        let has_public_key = context.get_property(&PUBLIC_KEY_SERVICE)?.is_some()
            && context.get_property(&PUBLIC_KEY_SEARCH)?.is_some();

        if !has_password && !has_public_key {
            Err(MinifiError::schedule_err(
                "Either a password or Public Key Service with Public Key Search should be configured to encrypt files",
            ))
        } else {
            Ok(EncryptContentPGP { file_encoding })
        }
    }
}

impl FlowFileStreamTransform for EncryptContentPGP {
    fn transform<Ctx: GetProperty + GetControllerService + GetAttribute, LoggerImpl: Logger>(
        &self,
        context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, MinifiError> {
        let file_name = context.get_attribute("filename")?.unwrap_or_default();
        let public_key = if let (Some(pub_key_search), Some(public_key_service)) = (
            context.get_property(&PUBLIC_KEY_SEARCH)?,
            context.get_controller_service::<PGPPublicKeyService>(&PUBLIC_KEY_SERVICE)?,
        ) {
            public_key_service.get(&pub_key_search)
        } else {
            None
        };
        let password = context.get_property(&PASSWORD)?;
        if public_key.is_none() && password.is_none() {
            warn!(logger, "No password or public key to encrypt with");
            return Ok(TransformStreamResult::route_without_changes(&FAILURE));
        }

        match self.encrypt_bytes(
            input_stream,
            output_stream,
            public_key.as_deref(),
            password.as_deref(),
            file_name,
        ) {
            Ok(_) => Ok(TransformStreamResult::new(
                &SUCCESS,
                HashMap::from([(
                    FILE_ENCODING.name.to_string(),
                    self.file_encoding.to_string(),
                )]),
            )),
            Err(e) => {
                warn!(logger, "Failed to encrypt content {:?}", e);
                Ok(TransformStreamResult::route_without_changes(&FAILURE))
            }
        }
    }
}

#[cfg(test)]
mod tests;

pub(crate) mod processor_definition;
