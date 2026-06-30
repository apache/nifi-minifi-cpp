use minifi_native::Relationship;

pub(crate) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "Decryption Succeeded",
};

pub(crate) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "Decryption Failed",
};
