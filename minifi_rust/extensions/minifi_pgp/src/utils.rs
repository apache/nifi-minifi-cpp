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

use minifi_native::{
    MinifiError, PropertyConstraints, PropertySchema, PropertyType, StandardPropertyValidator,
};

pub(crate) struct Password {}

impl PropertySchema for Password {
    const CONSTRAINT: Option<PropertyConstraints> = Some(PropertyConstraints::Validator(
        StandardPropertyValidator::NonBlankValidator,
    ));
    const IS_REQUIRED: bool = true;
}

impl PropertyType for Password {
    type Output = pgp::types::Password;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        Ok(pgp::types::Password::from(s))
    }
}

/// A newline separated list of passwords, each of which is tried in turn when unlocking a key.
pub(crate) struct Passwords {}

impl PropertySchema for Passwords {
    const CONSTRAINT: Option<PropertyConstraints> = Some(PropertyConstraints::Validator(
        StandardPropertyValidator::NonBlankValidator,
    ));
    const IS_REQUIRED: bool = true;
}

impl PropertyType for Passwords {
    type Output = Vec<pgp::types::Password>;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        Ok(s.lines()
            .filter(|line| !line.is_empty())
            .map(pgp::types::Password::from)
            .collect())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn single_password() {
        assert_eq!(Passwords::parse("hunter2").unwrap().len(), 1);
    }

    #[test]
    fn one_password_per_line() {
        let passwords = Passwords::parse("alice-pw\nbob-pw").unwrap();
        assert_eq!(passwords.len(), 2);
    }

    #[test]
    fn blank_lines_are_dropped() {
        let passwords = Passwords::parse("alice-pw\n\nbob-pw\n").unwrap();
        assert_eq!(passwords.len(), 2);
    }
}
