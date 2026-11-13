// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use minifi_native::Relationship;

pub(crate) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "Flowfiles that are successfully written to a file are routed to this relationship",
};

pub(crate) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "Failed files (conflict, write failure, etc.) are transferred to failure",
};
