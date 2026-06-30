use crate::{ProcessError, Relationship, TransformStreamResult};

pub fn assert_routed_to(
    res: Result<TransformStreamResult, ProcessError>,
    expected_relationship: &Relationship,
) {
    match res {
        Err(ProcessError::Route(route)) => {
            assert_eq!(route.relationship, expected_relationship.name)
        }
        Err(other) => {
            panic!("expected route to '{expected_relationship}', got fatal error: {other:?}")
        }
        Ok(_) => panic!("expected route to '{expected_relationship}', got Ok"),
    }
}
