use minifi_native::{
    FlowFileStreamTransform, GetAttribute, GetControllerService, GetId, GetProperty, InputStream,
    Logger, MinifiError, OutputStream, ProcessError, RouteErrorExt, Schedule,
    TransformStreamResult,
};
use pgp::composed::{ArmorOptions, MessageBuilder, SignedPublicKey};
use pgp::types::{Password, StringToKey};

mod processor_definition;

use processor_definition::*;

use minifi_native::macros::{ComponentIdentifier, PropertyType};
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};

#[derive(
    Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr, PropertyType,
)]
#[strum(serialize_all = "UPPERCASE", const_into_str)]
enum FileEncoding {
    Ascii,
    Binary,
}

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct EncryptContentPGP {
    file_encoding: FileEncoding,
    symmetric_password: Option<Password>,
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
        file_name: String,
    ) -> Result<(), MinifiError> {
        if pub_key.is_none() && self.symmetric_password.is_none() {
            return Err(MinifiError::custom(
                "No password or public key to encrypt with",
            ));
        }

        let mut builder = MessageBuilder::from_reader(file_name, input_stream).seipd_v1(
            rand::thread_rng(),
            pgp::crypto::sym::SymmetricKeyAlgorithm::AES256,
        );

        if let Some(pub_key) = pub_key {
            builder
                .encrypt_to_key(rand::thread_rng(), pub_key)
                .map_err(MinifiError::other)?;
        }

        if let Some(password) = &self.symmetric_password {
            builder
                .encrypt_with_password(string_to_key(), password)
                .map_err(MinifiError::other)?;
        }

        match self.file_encoding {
            FileEncoding::Ascii => builder
                .to_armored_writer(rand::thread_rng(), ArmorOptions::default(), output_stream)
                .map_err(MinifiError::other),
            FileEncoding::Binary => builder
                .to_writer(rand::thread_rng(), output_stream)
                .map_err(MinifiError::other),
        }
    }

    fn check_validity(password: &Option<Password>, has_pub_key: bool) -> Result<(), MinifiError> {
        if password.is_none() && !has_pub_key {
            Err(MinifiError::custom(
                "Either a password or Public Key Service with Public Key Search should be configured to encrypt files",
            ))
        } else {
            Ok(())
        }
    }
}

impl Schedule for EncryptContentPGP {
    fn schedule<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let file_encoding = context.get_property::<FileEncoding>(&FILE_ENCODING)?;
        let symmetric_password = context.get_property(&PASSWORD)?;

        let has_public_key = context.get_raw_property(&PUBLIC_KEY_SERVICE)?.is_some()
            && context.get_property(&PUBLIC_KEY_SEARCH)?.is_some();

        Self::check_validity(&symmetric_password, has_public_key)?;
        Ok(EncryptContentPGP {
            file_encoding,
            symmetric_password,
        })
    }
}

impl EncryptContentPGP {
    fn get_public_key<Ctx: GetProperty + GetControllerService>(
        context: &Ctx,
    ) -> Result<Option<&SignedPublicKey>, MinifiError> {
        if let (Some(pub_key_search), Some(public_key_service)) = (
            context.get_property(&PUBLIC_KEY_SEARCH)?,
            context.get_controller_service(&PUBLIC_KEY_SERVICE)?,
        ) {
            Ok(public_key_service.get(&pub_key_search))
        } else {
            Ok(None)
        }
    }
}

impl FlowFileStreamTransform for EncryptContentPGP {
    fn transform<
        Ctx: GetProperty + GetControllerService + GetAttribute + GetId,
        LoggerImpl: Logger,
    >(
        &self,
        context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        _logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, ProcessError> {
        let file_name = context
            .get_attribute("filename")?
            .unwrap_or(context.get_id()?);
        let public_key = Self::get_public_key(context)?;

        self.encrypt_bytes(input_stream, output_stream, public_key, file_name)
            .route_err_to_failure()?;

        Ok(TransformStreamResult::new(&SUCCESS)
            .with_attribute(FILE_ENCODING_ATTR.name, self.file_encoding.into_str()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::controller_services::public_key_service::PGPPublicKeyService;
    use crate::test_utils;
    use minifi_native::{
        ComponentIdentifier, EnableControllerService, IoState, MockControllerServiceContext,
        MockLogger, MockProcessContext, test,
    };
    #[test]
    fn test_ids() {
        assert_eq!(
            EncryptContentPGP::CLASS_NAME,
            "minifi_pgp::processors::encrypt_content::EncryptContentPGP"
        );
        assert_eq!(EncryptContentPGP::GROUP_NAME, "minifi_pgp");
        assert_eq!(EncryptContentPGP::VERSION, "0.1.0");
    }

    #[test]
    fn cannot_schedule_without_password_or_public_key() {
        assert!(
            EncryptContentPGP::schedule(&MockProcessContext::new(), &MockLogger::new()).is_err()
        );
    }

    fn assert_content(transform_result: &TransformStreamResult, is_ascii: bool) {
        assert_eq!(transform_result.target_relationship_name(), SUCCESS.name);
        assert_eq!(transform_result.write_status(), IoState::Ok);
        assert_eq!(
            transform_result.get_attribute("pgp.file.encoding").unwrap(),
            if is_ascii { "ASCII" } else { "BINARY" }
        );
    }

    #[test]
    fn encrypts_via_passphrase() {
        let mut context = MockProcessContext::new();
        context.properties.insert(PASSWORD.name(), "password");

        let mut result: Vec<u8> = Vec::new();
        let mut input_stream = std::io::Cursor::new("foo".as_bytes());
        let processor =
            EncryptContentPGP::schedule(&context, &MockLogger::new()).expect("should schedule");
        let transformed_ff = processor
            .transform(&context, &mut input_stream, &mut result, &MockLogger::new())
            .expect("should transform");

        assert!(!result.is_ascii());
        assert_content(&transformed_ff, false);
    }

    fn public_key_service() -> PGPPublicKeyService {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Keyring File".to_string(),
            test_utils::get_test_key_path("keyring.asc"),
        );

        PGPPublicKeyService::enable(&context, &MockLogger::new()).expect("should enable")
    }

    #[test]
    fn encrypts_ascii_for_alice() {
        let mut context = MockProcessContext::new();
        context.properties.extend([
            ("Public Key Service", "my_controller_service"),
            ("Public Key Search", "Alice"),
            ("File Encoding", "ASCII"),
        ]);

        context.controller_services.insert(
            "my_controller_service".to_string(),
            Box::new(public_key_service()),
        );

        let mut result: Vec<u8> = Vec::new();
        let mut input_stream = std::io::Cursor::new("foo".as_bytes());
        let processor =
            EncryptContentPGP::schedule(&context, &MockLogger::new()).expect("should schedule");
        let transformed_ff = processor
            .transform(&context, &mut input_stream, &mut result, &MockLogger::new())
            .expect("should transform");

        assert!(result.is_ascii());
        assert_content(&transformed_ff, true);
    }

    #[test]
    fn encrypts_binary_for_bob() {
        let mut context = MockProcessContext::new();
        context.properties.extend([
            ("Public Key Service", "my_controller_service"),
            ("Public Key Search", "Bob"),
            ("File Encoding", "BINARY"),
        ]);

        context.controller_services.insert(
            "my_controller_service".to_string(),
            Box::new(public_key_service()),
        );

        let mut result: Vec<u8> = Vec::new();
        let mut input_stream = std::io::Cursor::new("foo".as_bytes());
        let processor =
            EncryptContentPGP::schedule(&context, &MockLogger::new()).expect("should schedule");
        let transformed_ff = processor
            .transform(&context, &mut input_stream, &mut result, &MockLogger::new())
            .expect("should transform");

        assert!(!result.is_ascii());
        assert_content(&transformed_ff, false);
    }

    #[test]
    fn cannot_encrypt_for_carol() {
        let mut context = MockProcessContext::new();
        context.properties.extend([
            ("Public Key Service", "my_controller_service"),
            ("Public Key Search", "Carol"),
        ]);

        context.controller_services.insert(
            "my_controller_service".to_string(),
            Box::new(public_key_service()),
        );

        let mut result: Vec<u8> = Vec::new();
        let mut input_stream = std::io::Cursor::new("foo".as_bytes());
        let processor =
            EncryptContentPGP::schedule(&context, &MockLogger::new()).expect("should schedule");
        let res = processor.transform(&context, &mut input_stream, &mut result, &MockLogger::new());

        test::assert_routed_to(res, &FAILURE);
    }
}
