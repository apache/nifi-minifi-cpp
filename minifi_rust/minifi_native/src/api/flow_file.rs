use crate::MinifiError;

pub trait FlowFile {}

pub trait GetId {
    fn get_id(&self) -> Result<String, MinifiError>;
}
