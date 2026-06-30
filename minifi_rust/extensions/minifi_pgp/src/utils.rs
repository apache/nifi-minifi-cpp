use minifi_native::{MinifiError, PropertyType};

pub(crate) struct Password {}

impl PropertyType for Password {
    type Output = pgp::types::Password;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        Ok(pgp::types::Password::from(s))
    }
}
