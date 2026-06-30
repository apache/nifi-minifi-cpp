use minifi_native::Relationship;

pub(crate) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "Encryption Succeeded",
};

pub(crate) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "Encryption Failed",
};
