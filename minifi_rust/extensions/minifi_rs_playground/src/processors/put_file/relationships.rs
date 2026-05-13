use minifi_native::Relationship;

pub(crate) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "Flowfiles that are successfully written to a file are routed to this relationship",
};

pub(crate) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "Failed files (conflict, write failure, etc.) are transferred to failure",
};
