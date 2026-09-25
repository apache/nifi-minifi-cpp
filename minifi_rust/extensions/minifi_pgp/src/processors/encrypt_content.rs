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

use crate::controller_services::encryption_key::{EncryptionTarget, select_encryption_target};
use minifi_native::{
    FlowFileStreamTransform, GetAttribute, GetControllerService, GetId, GetProperty, InputStream,
    Logger, MinifiError, OutputStream, ProcessError, RouteErrorExt, Schedule,
    TransformStreamResult,
};
use pgp::composed::{ArmorOptions, MessageBuilder, SignedPublicKey};
use pgp::types::{Password, StringToKey};

use proc_def::*;

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
            match select_encryption_target(pub_key)? {
                EncryptionTarget::Primary(primary_key) => builder
                    .encrypt_to_key(rand::thread_rng(), primary_key)
                    .map_err(MinifiError::other)?,
                EncryptionTarget::Subkey(subkey) => builder
                    .encrypt_to_key(rand::thread_rng(), subkey)
                    .map_err(MinifiError::other)?,
            };
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
}

impl Schedule for EncryptContentPGP {
    fn schedule<P: GetProperty + GetControllerService, L: Logger>(
        context: &P,
        _logger: &L,
    ) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let file_encoding = context.get_property(&FILE_ENCODING)?;
        let symmetric_password = context.get_property(&SYMMETRIC_PASSPHRASE)?;

        let public_key_service = context.get_controller_service(&PUBLIC_KEY_SERVICE)?;

        // Given API support we should check if PUBLIC_KEY_SEARCH is set (without EL)
        if symmetric_password.is_none() && public_key_service.is_none() {
            return Err(MinifiError::custom(
                "Either a password or Public Key Service with Public Key Search should be configured to encrypt files",
            ));
        }
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
            Ok(Some(public_key_service.get(&pub_key_search)?))
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
        let file_name = match context.get_attribute("filename")? {
            Some(file_name) => file_name,
            None => context.get_id()?,
        };
        let public_key = Self::get_public_key(context).route_err_to_failure()?;

        self.encrypt_bytes(input_stream, output_stream, public_key, file_name)
            .route_err_to_failure()?;

        Ok(TransformStreamResult::new(&SUCCESS)
            .with_attribute(FILE_ENCODING_ATTR.name, self.file_encoding.into_str()))
    }
}

mod proc_def {
    use super::*;
    use crate::controller_services::public_key_service::PGPPublicKeyService;
    use crate::utils;
    use minifi_native::{
        OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property,
        PropertyDefinition, Relationship, property_definitions,
    };

    pub(crate) const FILE_ENCODING: Property<FileEncoding> =
        Property::new("File Encoding", "File Encoding for encryption")
            .with_default(FileEncoding::Binary.into_str());

    pub(crate) const SYMMETRIC_PASSPHRASE: Property<Option<utils::Password>> = Property::new(
        "Passphrase",
        "Passphrase used for encrypting data with Password-Based Encryption",
    )
    .sensitive();

    pub(crate) const PUBLIC_KEY_SEARCH: Property<Option<String>> = Property::new(
        "Public Key Search",
        "PGP Public Key Search will be used to match against the User ID or Key ID when formatted as uppercase hexadecimal string of 16 characters",
    ).supports_expression_language();

    pub(crate) const PUBLIC_KEY_SERVICE: Property<Option<PGPPublicKeyService>> = Property::new(
        "Public Key Service",
        "PGP Public Key Service for encrypting data with Public Key Encryption",
    );

    pub(super) const FILE_ENCODING_ATTR: OutputAttribute = OutputAttribute {
        name: "pgp.file.encoding",
        relationships: &["success"],
        description: "File Encoding",
    };

    pub(super) const SUCCESS: Relationship = Relationship {
        name: "success",
        description: "Encryption Succeeded",
    };

    pub(super) const FAILURE: Relationship = Relationship {
        name: "failure",
        description: "Encryption Failed",
    };

    impl ProcessorDefinition for EncryptContentPGP {
        const DESCRIPTION: &'static str = "Encrypt contents using OpenPGP.";
        const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
        const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
        const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
        const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[FILE_ENCODING_ATTR];
        const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];

        const PROPERTIES: &[PropertyDefinition] = property_definitions![
            FILE_ENCODING,
            SYMMETRIC_PASSPHRASE,
            PUBLIC_KEY_SEARCH,
            PUBLIC_KEY_SERVICE,
        ];
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
        assert_eq!(EncryptContentPGP::VERSION, "1.0.0");
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
        context
            .properties
            .insert(SYMMETRIC_PASSPHRASE.name(), "password");

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

    #[test]
    fn configured_public_key_miss_fails_even_with_password() {
        let mut context = MockProcessContext::new();
        context.properties.extend([
            ("Public Key Service", "my_controller_service"),
            ("Public Key Search", "Carol"),
            ("Symmetric Password", "password"),
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
