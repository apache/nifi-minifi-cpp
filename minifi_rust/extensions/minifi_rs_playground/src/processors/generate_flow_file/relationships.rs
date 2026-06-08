use minifi_native::Relationship;

pub(crate) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "success operational on the flow record", // TODO(wtf... but copy paste)
};
