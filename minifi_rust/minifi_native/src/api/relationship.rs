// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use std::fmt::{Display, Formatter};

#[derive(Debug, Eq, PartialEq)]
pub struct Relationship {
    pub name: &'static str,
    pub description: &'static str,
}

impl Display for Relationship {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

const fn names_match(lhs: &str, rhs: &str) -> bool {
    let (lhs, rhs) = (lhs.as_bytes(), rhs.as_bytes());
    if lhs.len() != rhs.len() {
        return false;
    }
    let mut i = 0;
    while i < lhs.len() {
        if lhs[i] != rhs[i] {
            return false;
        }
        i += 1;
    }
    true
}

/// Whether `relationship` is one of `declared`, comparing by name.
pub const fn is_declared(declared: &[Relationship], relationship: &Relationship) -> bool {
    let mut i = 0;
    while i < declared.len() {
        if names_match(declared[i].name, relationship.name) {
            return true;
        }
        i += 1;
    }
    false
}

#[cfg(test)]
mod tests {
    use super::*;

    const SUCCESS: Relationship = Relationship {
        name: "success",
        description: "",
    };
    const FAILURE: Relationship = Relationship {
        name: "failure",
        description: "",
    };

    #[test]
    fn finds_a_declared_relationship() {
        assert!(is_declared(&[SUCCESS, FAILURE], &FAILURE));
    }

    #[test]
    fn rejects_an_undeclared_relationship() {
        assert!(!is_declared(&[SUCCESS], &FAILURE));
        assert!(!is_declared(&[], &FAILURE));
    }

    #[test]
    fn matches_by_name_not_by_identity() {
        const OTHER_FAILURE: Relationship = Relationship {
            name: "failure",
            description: "a different description",
        };
        assert!(is_declared(&[OTHER_FAILURE], &FAILURE));
    }

    #[test]
    fn does_not_match_a_name_that_is_merely_a_prefix() {
        const FAIL: Relationship = Relationship {
            name: "fail",
            description: "",
        };
        assert!(!is_declared(&[FAIL], &FAILURE));
    }
}
