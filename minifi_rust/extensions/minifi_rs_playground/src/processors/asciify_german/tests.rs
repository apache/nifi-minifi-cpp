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

use super::*;
use crate::processors::asciify_german::relationships::SUCCESS;
use minifi_native::{IoState, MockLogger, MockProcessContext, test};
use std::io::BufReader;

#[test]
fn schedule_succeeds_with_default_values() {
    assert!(AsciifyGerman::schedule(&MockProcessContext::new(), &MockLogger::new()).is_ok());
}

#[test]
fn simple_test() {
    let process_context = MockProcessContext::new();
    let context = MockProcessContext::new();
    let logger = MockLogger::new();

    let asciify_german =
        AsciifyGerman::schedule(&process_context, &logger).expect("Should succeed");
    let input_str = "Falsches Üben von Xylophonmusik quält jeden größeren Zwerg.";
    let mut input_stream = BufReader::new(input_str.as_bytes());
    let mut output_vec: Vec<u8> = Vec::new();
    {
        let result = asciify_german
            .transform(&context, &mut input_stream, &mut output_vec, &logger)
            .expect("Should succeed");
        assert_eq!(result.write_status(), IoState::Ok);
        assert_eq!(result.target_relationship_name(), SUCCESS.name);
    }
    assert_eq!(
        output_vec,
        "Falsches Ueben von Xylophonmusik quaelt jeden groesseren Zwerg.".as_bytes()
    );
}

#[test]
fn simple_failure_test() {
    let process_context = MockProcessContext::new();
    let context = MockProcessContext::new();
    let logger = MockLogger::new();

    let asciify_german =
        AsciifyGerman::schedule(&process_context, &logger).expect("Should succeed");
    let input_str = "Üldögélő műújságíró";
    let mut input_stream = BufReader::new(input_str.as_bytes());
    let mut output_vec: Vec<u8> = Vec::new();
    {
        let result = asciify_german
            .transform(&context, &mut input_stream, &mut output_vec, &logger)
            .expect("Should succeed");
        assert_eq!(result.write_status(), IoState::Cancel);
        assert_eq!(result.target_relationship_name(), FAILURE.name);
    }
    assert_eq!(output_vec, "Ueldoeg".as_bytes());
}

#[test]
fn truncated_umlaut_at_eof_routes_to_failure() {
    let context = MockProcessContext::new();
    let logger = MockLogger::new();

    let asciify_german = AsciifyGerman::schedule(&context, &logger).expect("Should succeed");
    // A lone 0xC3 is the leading byte of every German umlaut in UTF-8; on EOF the
    // sequence is incomplete and the processor must report failure rather than
    // silently truncating.
    let input_bytes: &[u8] = &[b'a', 0xC3];
    let mut input_stream = BufReader::new(input_bytes);
    let mut output_vec: Vec<u8> = Vec::new();

    let result = asciify_german.transform(&context, &mut input_stream, &mut output_vec, &logger);
    test::assert_routed_to(result, &FAILURE);
}
