use minifi_native::Relationship;

pub(crate) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "All asciified flowfiles are routed here",
};

pub(crate) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "Non-german flowfiles are routed here",
};
