use minifi_native::{MinifiError, PropertyConstraints, PropertySchema, PropertyType};

pub(crate) struct Password {}

impl PropertySchema for Password {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = false;
}

impl PropertyType for Password {
    type Output = pgp::types::Password;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        Ok(pgp::types::Password::from(s))
    }
}
