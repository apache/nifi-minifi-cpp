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
use crate::processors::list_file::relationships::SUCCESS;
use minifi_native::{MockLogger, MockProcessContext};

fn context_for(dir: &std::path::Path) -> MockProcessContext {
    let mut context = MockProcessContext::new();
    context.properties.insert(
        DIRECTORY.name().to_string(),
        dir.to_str().unwrap().to_string(),
    );
    context
}

#[test]
fn schedule_fails_without_input_dir() {
    assert!(matches!(
        ListFileRs::schedule(&MockProcessContext::new(), &MockLogger::new())
            .err()
            .unwrap(),
        MinifiError::MissingRequiredProperty(_)
    ));
}

#[test]
fn schedule_fails_with_invalid_input_dir() {
    let mut context = MockProcessContext::new();
    context.properties.insert(
        DIRECTORY.name().to_string(),
        "/invalid_directory".to_string(),
    );
    assert!(ListFileRs::schedule(&context, &MockLogger::new()).is_err());
}

#[test]
fn lists_each_file_once_across_triggers() {
    let temp_dir = tempfile::tempdir().unwrap();
    std::fs::write(temp_dir.path().join("a.txt"), "a").unwrap();
    std::fs::write(temp_dir.path().join("b.txt"), "bb").unwrap();

    let mut context = context_for(temp_dir.path());
    let mut list_file = ListFileRs::schedule(&context, &MockLogger::new()).unwrap();

    // Scoped so the returned FlowFiles, which borrow the context, drop before the next trigger.
    let first_count = {
        let first = list_file
            .generate(&mut context, &MockLogger::new())
            .unwrap();
        assert!(
            first
                .iter()
                .all(|ff| ff.target_relationship_name() == SUCCESS.name)
        );
        first.len()
    };
    assert_eq!(first_count, 2);

    let second_count = list_file
        .generate(&mut context, &MockLogger::new())
        .unwrap()
        .len();
    assert_eq!(second_count, 0);

    std::fs::write(temp_dir.path().join("c.txt"), "ccc").unwrap();
    let third_count = list_file
        .generate(&mut context, &MockLogger::new())
        .unwrap()
        .len();
    assert_eq!(third_count, 1);
}

#[test]
fn persisted_state_survives_reload() {
    let temp_dir = tempfile::tempdir().unwrap();
    std::fs::write(temp_dir.path().join("a.txt"), "a").unwrap();

    let mut context = context_for(temp_dir.path());

    let mut first = ListFileRs::schedule(&context, &MockLogger::new()).unwrap();
    let first_count = first
        .generate(&mut context, &MockLogger::new())
        .unwrap()
        .len();
    assert_eq!(first_count, 1);
    minifi_native::test::persist_state(&first, &context).unwrap();

    // A fresh instance restores that state and must not re-list the same file.
    let mut second = ListFileRs::schedule(&context, &MockLogger::new()).unwrap();
    minifi_native::test::restore_state(&mut second, &context).unwrap();
    let second_count = second
        .generate(&mut context, &MockLogger::new())
        .unwrap()
        .len();
    assert_eq!(second_count, 0);
}

#[test]
fn ignores_hidden_files_by_default() {
    let temp_dir = tempfile::tempdir().unwrap();
    std::fs::write(temp_dir.path().join("visible.txt"), "v").unwrap();
    std::fs::write(temp_dir.path().join(".hidden"), "h").unwrap();

    let mut context = context_for(temp_dir.path());
    let mut list_file = ListFileRs::schedule(&context, &MockLogger::new()).unwrap();
    let listed = list_file
        .generate(&mut context, &MockLogger::new())
        .unwrap();
    assert_eq!(listed.len(), 1);
}
