use crate::processors::lorem_ipsum_cs_user::LoremIpsumCSUser;
use minifi_native::{ComponentIdentifier, MockLogger, MockProcessContext, Schedule};

#[test]
fn test_ids() {
    assert_eq!(
        LoremIpsumCSUser::CLASS_NAME,
        "minifi_rs_playground::processors::lorem_ipsum_cs_user::LoremIpsumCSUser"
    );
    assert_eq!(LoremIpsumCSUser::GROUP_NAME, "minifi_rs_playground");
    assert_eq!(LoremIpsumCSUser::VERSION, "0.1.0");
}

#[test]
fn schedules_with_controller() {
    let context = MockProcessContext::new();
    let schedule_result = LoremIpsumCSUser::schedule(&context, &MockLogger::new());
    assert!(schedule_result.is_ok());
}
