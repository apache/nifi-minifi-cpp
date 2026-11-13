// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

fn main() {
    println!("cargo:rerun-if-env-changed=DEP_MINIFI_BEHAVE_PATH");
    let behave_path =
        std::env::var("DEP_MINIFI_BEHAVE_PATH").expect("DEP_MINIFI_BEHAVE_PATH is required");
    println!("cargo:rustc-env=MINIFI_BEHAVE_PATH={}", behave_path);
}
