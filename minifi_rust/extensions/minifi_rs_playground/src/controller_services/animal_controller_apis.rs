use minifi_native::macros::controller_service_api;
use std::fmt::Debug;

#[controller_service_api]
pub trait NumberOfLegsControllerApi: Debug {
    fn number_of_legs(&self) -> u8;
}

#[controller_service_api]
pub trait CanFlyControllerApi: Debug {
    fn can_fly(&self) -> bool;
}
