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

mod processor_definition;

use processor_definition::*;

use crate::controller_services::private_key_service::PGPPrivateKeyService;

use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    FlowFileStreamTransform, GetControllerService, GetProperty, InputStream, Logger, MinifiError,
    OutputStream, ProcessError, RouteErrorExt, Schedule, TransformStreamResult,
};
use pgp::composed::{Message, TheRing};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DecryptContentPGP {
    symmetric_password: Option<pgp::types::Password>,
}

impl Schedule for DecryptContentPGP {
    fn schedule<P: GetProperty, L>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
        L: Logger,
    {
        let symmetric_password = context.get_property(&SYMMETRIC_PASSWORD)?;
        let has_context_service = context.get_raw_property(&PRIVATE_KEY_SERVICE)?.is_some();
        if !has_context_service && symmetric_password.is_none() {
            Err(MinifiError::validation(
                "Either Symmetric Password or Private Key Service must be set",
            ))
        } else {
            Ok(DecryptContentPGP { symmetric_password })
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
    ) -> Vec<(&'static str, String)> {
        let mut res = Vec::new();
        if let Some(literal_data_header) = decrypted_msg.literal_data_header() {
            if let Ok(file_name) = str::from_utf8(literal_data_header.file_name()) {
                res.push((LITERAL_DATA_FILENAME.name, file_name.to_string()));
            }
            // NiFi uses ms timestamp
            res.push((
                LITERAL_DATA_MODIFIED.name,
                (1000u64 * literal_data_header.created().as_secs() as u64).to_string(),
            ));
        }
        res
    }
}

impl FlowFileStreamTransform for DecryptContentPGP {
    fn transform<Ctx: GetProperty + GetControllerService, LoggerImpl: Logger>(
        &self,
        context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        _logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, ProcessError> {
        let private_key_service = context.get_controller_service(&PRIVATE_KEY_SERVICE)?;

        let msg = Message::from_reader(input_stream)
            .map(|(msg, _header)| msg)
            .route_err_to_failure()?;

        let mut decrypted_msg = self
            .decrypt_msg(msg, private_key_service)
            .route_err_to_failure()?;

        if decrypted_msg.is_compressed() {
            decrypted_msg = decrypted_msg
                .decompress()
                .map_err(MinifiError::other)
                .route_err_to_failure()?
        };

        let attributes_to_add = Self::extract_attributes_from_decrypted_message(&decrypted_msg);
        let _written_bytes =
            std::io::copy(&mut decrypted_msg.into_inner(), output_stream).route_err_to_failure()?;

        Ok(TransformStreamResult::new(&SUCCESS).with_attributes(attributes_to_add))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_utils;
    use crate::test_utils::get_test_message;
    use minifi_native::{
        ComponentIdentifier, EnableControllerService, IoState, MockControllerServiceContext,
        MockLogger, MockProcessContext, test,
    };
    #[test]
    fn test_ids() {
        assert_eq!(
            DecryptContentPGP::CLASS_NAME,
            "minifi_pgp::processors::decrypt_content::DecryptContentPGP"
        );
        assert_eq!(DecryptContentPGP::GROUP_NAME, "minifi_pgp");
        assert_eq!(DecryptContentPGP::VERSION, "1.0.0");
    }

    #[test]
    fn fails_to_schedule_by_default() {
        let decrypt_content =
            DecryptContentPGP::schedule(&MockProcessContext::new(), &MockLogger::new());
        assert!(decrypt_content.is_err());
    }

    #[test]
    fn schedules_with_password() {
        let mut context = MockProcessContext::new();
        context
            .properties
            .insert(SYMMETRIC_PASSWORD.name(), "my_secret_password".to_string());
        let decrypt_content = DecryptContentPGP::schedule(&context, &MockLogger::new());
        assert!(decrypt_content.is_ok());
    }

    #[test]
    fn schedules_with_controller() {
        let mut context = MockProcessContext::new();
        context.properties.insert(
            PRIVATE_KEY_SERVICE.name(),
            "my_private_key_service".to_string(),
        );
        let decrypt_content = DecryptContentPGP::schedule(&context, &MockLogger::new());
        assert!(decrypt_content.is_ok());
    }

    #[derive(Copy, Clone)]
    struct PrivateKeyData {
        key_filename: &'static str,
        passphrase: Option<&'static str>,
    }

    impl PrivateKeyData {
        fn into_controller(self) -> PGPPrivateKeyService {
            let mut context = MockControllerServiceContext::new();
            context
                .properties
                .insert("Key File", test_utils::get_test_key_path(self.key_filename));

            if let Some(passphrase) = self.passphrase {
                context.properties.insert("Key Passphrase", passphrase);
            }

            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable")
        }
    }

    fn test_decryption(
        message_file_name: &str,
        private_key_data: Option<PrivateKeyData>,
        symmetric_password: Option<&'static str>,
        expected_result: Result<&[u8], ()>,
    ) {
        let mut processor_context = MockProcessContext::new();
        if let Some(private_key) = private_key_data {
            processor_context.controller_services.insert(
                "my_private_key_service".to_string(),
                Box::new(private_key.into_controller()),
            );
            processor_context.properties.insert(
                PRIVATE_KEY_SERVICE.name(),
                "my_private_key_service".to_string(),
            );
        }
        if let Some(symmetric_password) = symmetric_password {
            processor_context
                .properties
                .insert(SYMMETRIC_PASSWORD.name(), symmetric_password.to_string());
        }

        let decrypt_content = DecryptContentPGP::schedule(&processor_context, &MockLogger::new())
            .expect("Should schedule with the configured properties");
        let mut output: Vec<u8> = Vec::new();
        let mut flow_file_stream = std::io::Cursor::new(get_test_message(message_file_name));
        let res = decrypt_content.transform(
            &processor_context,
            &mut flow_file_stream,
            &mut output,
            &MockLogger::new(),
        );

        match expected_result {
            Ok(result_bytes) => {
                let res = res.expect("Should be able to transform");
                assert_eq!(res.target_relationship_name(), SUCCESS.name);
                assert_eq!(res.write_status(), IoState::Ok);
                assert_eq!(output, result_bytes);
                let data_modified = res
                    .get_attribute(LITERAL_DATA_MODIFIED.name)
                    .unwrap()
                    .parse::<u64>()
                    .expect("Should be u64");
                assert!(data_modified > 1770000000000);
                assert!(data_modified < 1780000000000);
                assert!(res.get_attribute(LITERAL_DATA_FILENAME.name).is_some());
            }
            Err(_) => test::assert_routed_to(res, &FAILURE),
        }
    }

    #[test]
    fn decrypts_with_password() {
        test_decryption(
            "password_encrypted_foo.gpg",
            None,
            Some("my_secret_password"),
            Ok("foo\n".as_bytes()),
        );
        test_decryption(
            "password_encrypted_foo.asc",
            None,
            Some("my_secret_password"),
            Ok("foo\n".as_bytes()),
        );
        test_decryption(
            "foo_for_alice.gpg",
            None,
            Some("my_secret_password"),
            Err(()),
        );
        test_decryption(
            "foo_for_alice.asc",
            None,
            Some("my_secret_password"),
            Err(()),
        );
    }

    #[test]
    fn decrypts_for_alice() {
        let alice_private_key_data = PrivateKeyData {
            key_filename: "alice_private.asc",
            passphrase: Some("whiterabbit"),
        };

        test_decryption(
            "foo_for_alice.asc",
            Some(alice_private_key_data),
            None,
            Ok("foo\n".as_bytes()),
        );

        test_decryption(
            "foo_for_alice.gpg",
            Some(alice_private_key_data),
            None,
            Ok("foo\n".as_bytes()),
        );

        test_decryption(
            "password_encrypted_foo.gpg",
            Some(alice_private_key_data),
            None,
            Err(()),
        );

        test_decryption(
            "password_encrypted_foo.asc",
            Some(alice_private_key_data),
            None,
            Err(()),
        );
    }

    #[test]
    fn decryption_of_not_encrypted_data() {
        let alice_private_key = PrivateKeyData {
            key_filename: "alice_private.asc",
            passphrase: Some("whiterabbit"),
        };

        let mut processor_context = MockProcessContext::new();
        processor_context.controller_services.insert(
            "my_private_key_service".to_string(),
            Box::new(alice_private_key.into_controller()),
        );
        processor_context.properties.insert(
            PRIVATE_KEY_SERVICE.name(),
            "my_private_key_service".to_string(),
        );

        let logger = MockLogger::new();

        let decrypt_content = DecryptContentPGP::schedule(&processor_context, &logger)
            .expect("Should schedule without any properties");
        let mut result: Vec<u8> = vec![];
        let mut flow_file_stream = std::io::Cursor::new("something not encrypted".as_bytes());
        let res = decrypt_content.transform(
            &processor_context,
            &mut flow_file_stream,
            &mut result,
            &logger,
        );

        test::assert_routed_to(res, &FAILURE);
    }
}
