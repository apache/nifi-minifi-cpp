mod output_attributes;
pub(crate) mod properties;
mod relationships;

use crate::controller_services::private_key_service::PGPPrivateKeyService;
use crate::processors::decrypt_content::properties::{PRIVATE_KEY_SERVICE, SYMMETRIC_PASSWORD};
use crate::processors::decrypt_content::relationships::{FAILURE, SUCCESS};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    FlowFileStreamTransform, GetControllerService, GetProperty, InputStream, Logger, MinifiError,
    OutputStream, Schedule, TransformStreamResult, warn,
};
use pgp::composed::{Message, TheRing};
use std::collections::HashMap;
use std::fmt::Debug;
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};

#[derive(Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr)]
#[strum(serialize_all = "UPPERCASE", const_into_str)]
enum DecryptionStrategy {
    Decrypted,
    Packaged,
}

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DecryptContentPGP {
    decompress_data: bool,
    symmetric_password: Option<pgp::types::Password>,
}

impl Schedule for DecryptContentPGP {
    fn schedule<P: GetProperty, L>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
        L: Logger,
    {
        let decryption_strategy = context
            .get_property(&properties::DECRYPTION_STRATEGY)?
            .expect("required property")
            .parse::<DecryptionStrategy>()?;

        let symmetric_password = context
            .get_property(&SYMMETRIC_PASSWORD)?
            .and_then(|pwd_str| Option::from(pgp::types::Password::from(pwd_str)));
        let has_context_service = context.get_property(&PRIVATE_KEY_SERVICE)?.is_some();
        if !has_context_service && symmetric_password.is_none() {
            Err(MinifiError::schedule_err(
                "Either Symmetric Password or Private Key Service must be set",
            ))
        } else {
            Ok(DecryptContentPGP {
                decompress_data: decryption_strategy == DecryptionStrategy::Decrypted,
                symmetric_password,
            })
        }
    }
}

impl DecryptContentPGP {
    fn decrypt_msg<'a>(
        &'a self,
        msg: Message<'a>,
        private_key_service: Option<&'a PGPPrivateKeyService>,
    ) -> pgp::errors::Result<Message<'a>> {
        let mut ring = if let Some(pks) = private_key_service {
            pks.get_the_ring()
        } else {
            TheRing::default()
        };

        ring.decrypt_options = ring.decrypt_options.enable_gnupg_aead();

        if let Some(sym_passwd) = &self.symmetric_password {
            ring.message_password.push(sym_passwd);
        }
        let (decrypted_msg, _ring_result) = msg.decrypt_the_ring(ring, false)?;
        Ok(decrypted_msg)
    }

    fn extract_attributes_from_decrypted_message(
        decrypted_msg: &Message,
    ) -> HashMap<String, String> {
        let mut attributes_to_add = HashMap::new();
        if let Some(literal_data_header) = decrypted_msg.literal_data_header() {
            if let Ok(file_name) = str::from_utf8(literal_data_header.file_name()) {
                attributes_to_add.insert(
                    output_attributes::LITERAL_DATA_FILENAME.name.to_string(),
                    file_name.to_string(),
                );
            }
            attributes_to_add.insert(
                output_attributes::LITERAL_DATA_MODIFIED.name.to_string(),
                (1000u64 * literal_data_header.created().as_secs() as u64).to_string(), // Nifi uses ms timestamp
            );
        }
        attributes_to_add
    }
}

impl FlowFileStreamTransform for DecryptContentPGP {
    fn transform<Ctx: GetProperty + GetControllerService, LoggerImpl: Logger>(
        &self,
        context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, MinifiError> {
        let Ok(msg) = Message::from_reader(input_stream).map(|(msg, _header)| msg) else {
            warn!(logger, "No valid PGP message found");
            return Ok(TransformStreamResult::route_without_changes(&FAILURE));
        };

        let private_key_service =
            context.get_controller_service::<PGPPrivateKeyService>(&PRIVATE_KEY_SERVICE)?;

        let Ok(mut decrypted_msg) = self.decrypt_msg(msg, private_key_service) else {
            warn!(logger, "Failed to decrypt data");
            return Ok(TransformStreamResult::route_without_changes(&FAILURE));
        };

        if self.decompress_data && decrypted_msg.is_compressed() {
            match decrypted_msg.decompress() {
                Ok(decompressed_data) => {
                    decrypted_msg = decompressed_data;
                }
                Err(e) => {
                    warn!(logger, "Failed to decompress data: {}", e);
                    return Ok(TransformStreamResult::route_without_changes(&FAILURE));
                }
            }
        };

        let attributes_to_add = Self::extract_attributes_from_decrypted_message(&decrypted_msg);
        let Ok(_written_bytes) = std::io::copy(&mut decrypted_msg.into_inner(), output_stream)
        else {
            warn!(logger, "Failed to extract raw data from decrypted message");
            return Ok(TransformStreamResult::route_without_changes(&FAILURE));
        };

        Ok(TransformStreamResult::new(&SUCCESS, attributes_to_add))
    }
}

#[cfg(test)]
mod tests;

pub(crate) mod processor_definition;
