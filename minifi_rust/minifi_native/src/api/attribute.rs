use crate::MinifiError;

pub struct OutputAttribute {
    pub name: &'static str,
    pub relationships: &'static [&'static str],
    pub description: &'static str,
}

pub trait GetAttribute {
    fn get_attribute(&self, name: &str) -> Result<Option<String>, MinifiError>;
}
