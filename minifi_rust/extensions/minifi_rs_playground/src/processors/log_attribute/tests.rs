use super::*;
use minifi_native::{
    ComponentIdentifier, MockFlowFile, MockLogger, MockProcessContext, MockProcessSession,
};

#[test]
fn test_component_id() {
    assert_eq!(
        LogAttributeRs::CLASS_NAME,
        "minifi_rs_playground::processors::log_attribute::LogAttributeRs"
    );
    assert_eq!(LogAttributeRs::GROUP_NAME, "minifi_rs_playground");
    assert_eq!(LogAttributeRs::VERSION, "0.1.0");
}
#[test]
fn schedule_succeeds_with_default_values() {
    assert!(LogAttributeRs::schedule(&MockProcessContext::new(), &MockLogger::new()).is_ok());
}

fn tester(
    input_flow_files: Vec<MockFlowFile>,
    log_level: LogLevel,
    properties: Box<[(&str, &str)]>,
    expected_log_msg: String,
) {
    let logger = MockLogger::new();
    let mut context = MockProcessContext::new();
    for (k, v) in properties {
        context.properties.insert(k.to_string(), v.to_string());
    }

    let processor = LogAttributeRs::schedule(&context, &logger).unwrap();

    let mut session = MockProcessSession::new();
    for flow_file in input_flow_files {
        session.input_flow_files.push(flow_file);
    }
    processor
        .trigger(&mut context, &mut session, &logger)
        .expect("The on_trigger should succeed");

    let logs = logger.logs.lock().unwrap();
    assert_eq!(logs[1], (log_level, expected_log_msg));
}

#[test]
fn warn_single_log_payload() {
    let mut flow_file = MockFlowFile::with_content("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer facilisis diam sit amet nisl interdum, vitae interdum arcu viverra. Nam placerat mi in erat pellentesque, at ultrices orci faucibus. Cras sollicitudin iaculis posuere. Sed tempus, dolor nec lacinia suscipit, tellus odio venenatis odio, nec sollicitudin dolor augue non urna. Aliquam tincidunt viverra ipsum eget hendrerit. Suspendisse varius, augue vel fermentum varius, velit elit euismod lacus, a placerat purus est a lacus. Aenean nibh neque, consectetur hendrerit egestas vitae, commodo non quam. Nullam luctus tempor ante, sed tempus quam imperdiet in. Maecenas gravida erat orci, in consequat urna pretium nec. In sodales iaculis magna at vehicula.".as_bytes());
    flow_file
        .attributes
        .insert(String::from("apple"), String::from("apfel"));
    let vec = vec![flow_file];

    let expected =
        "Logging for flow file
--------------------------------------------------
FlowFile Attributes Map Content
key:apple value:apfel
Payload:
Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer facilisis diam sit amet nisl interdum, vitae interdum arcu viverra. Nam placerat mi in erat pellentesque, at ultrices orci faucibus. Cras sollicitudin iaculis posuere. Sed tempus, dolor nec lacinia suscipit, tellus odio venenatis odio, nec sollicitudin dolor augue non urna. Aliquam tincidunt viverra ipsum eget hendrerit. Suspendisse varius, augue vel fermentum varius, velit elit euismod lacus, a placerat purus est a lacus. Aenean nibh neque, consectetur hendrerit egestas vitae, commodo non quam. Nullam luctus tempor ante, sed tempus quam imperdiet in. Maecenas gravida erat orci, in consequat urna pretium nec. In sodales iaculis magna at vehicula.
--------------------------------------------------".to_string();
    let properties_set = [
        ("Log Payload", "true"),
        ("Hexencode Payload", "false"),
        ("Log Level", "Warn"),
    ];
    tester(vec, LogLevel::Warn, Box::new(properties_set), expected);
}

#[test]
fn critical_single_hexencode_payload() {
    let mut flow_file = MockFlowFile::with_content("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer facilisis diam sit amet nisl interdum, vitae interdum arcu viverra. Nam placerat mi in erat pellentesque, at ultrices orci faucibus. Cras sollicitudin iaculis posuere. Sed tempus, dolor nec lacinia suscipit, tellus odio venenatis odio, nec sollicitudin dolor augue non urna. Aliquam tincidunt viverra ipsum eget hendrerit. Suspendisse varius, augue vel fermentum varius, velit elit euismod lacus, a placerat purus est a lacus. Aenean nibh neque, consectetur hendrerit egestas vitae, commodo non quam. Nullam luctus tempor ante, sed tempus quam imperdiet in. Maecenas gravida erat orci, in consequat urna pretium nec. In sodales iaculis magna at vehicula.".as_bytes());
    flow_file
        .attributes
        .insert(String::from("apple"), String::from("apfel"));
    let vec = vec![flow_file];

    let expected =
        "Logging for flow file
--------------------------------------------------
FlowFile Attributes Map Content
key:apple value:apfel
Payload:
4c6f72656d20697073756d20646f6c6f722073697420616d65742c20636f6e73656374657475722061646970697363696e6720656c69742e20496e746567657220666163696c69736973206469616d2073697420616d6574206e69736c20696e74657264756d2c20766974616520696e74657264756d206172637520766976657272612e204e616d20706c616365726174206d6920696e20657261742070656c6c656e7465737175652c20617420756c747269636573206f7263692066617563696275732e204372617320736f6c6c696369747564696e20696163756c697320706f73756572652e205365642074656d7075732c20646f6c6f72206e6563206c6163696e69612073757363697069742c2074656c6c7573206f64696f2076656e656e61746973206f64696f2c206e656320736f6c6c696369747564696e20646f6c6f72206175677565206e6f6e2075726e612e20416c697175616d2074696e636964756e74207669766572726120697073756d20656765742068656e6472657269742e2053757370656e6469737365207661726975732c2061756775652076656c206665726d656e74756d207661726975732c2076656c697420656c697420657569736d6f64206c616375732c206120706c616365726174207075727573206573742061206c616375732e2041656e65616e206e696268206e657175652c20636f6e73656374657475722068656e64726572697420656765737461732076697461652c20636f6d6d6f646f206e6f6e207175616d2e204e756c6c616d206c75637475732074656d706f7220616e74652c207365642074656d707573207175616d20696d7065726469657420696e2e204d616563656e617320677261766964612065726174206f7263692c20696e20636f6e7365717561742075726e61207072657469756d206e65632e20496e20736f64616c657320696163756c6973206d61676e61206174207665686963756c612e
--------------------------------------------------".to_string();
    let properties_set = [
        ("Log Payload", "true"),
        ("Hexencode Payload", "true"),
        ("Log Level", "Critical"),
    ];
    tester(vec, LogLevel::Critical, Box::new(properties_set), expected);
}

#[test]
fn default_level_multiple_attributes() {
    let mut flow_file = MockFlowFile::with_content("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer facilisis diam sit amet nisl interdum, vitae interdum arcu viverra. Nam placerat mi in erat pellentesque, at ultrices orci faucibus. Cras sollicitudin iaculis posuere. Sed tempus, dolor nec lacinia suscipit, tellus odio venenatis odio, nec sollicitudin dolor augue non urna. Aliquam tincidunt viverra ipsum eget hendrerit. Suspendisse varius, augue vel fermentum varius, velit elit euismod lacus, a placerat purus est a lacus. Aenean nibh neque, consectetur hendrerit egestas vitae, commodo non quam. Nullam luctus tempor ante, sed tempus quam imperdiet in. Maecenas gravida erat orci, in consequat urna pretium nec. In sodales iaculis magna at vehicula.".as_bytes());
    flow_file
        .attributes
        .insert(String::from("apple"), String::from("apfel"));
    flow_file
        .attributes
        .insert(String::from("pear"), String::from("birne"));
    let vec = vec![flow_file];

    let expected = "Logging for flow file
--------------------------------------------------
FlowFile Attributes Map Content
key:apple value:apfel
key:pear value:birne
--------------------------------------------------"
        .to_string();
    let properties_set = [("Log Payload", "false")];
    tester(vec, LogLevel::Info, Box::new(properties_set), expected);
}
