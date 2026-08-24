mod fork_enrichment_def;

use crate::processors::attributes::{FORK_ROLE_ATTR, GROUP_ID_ATTR};
use crate::processors::fork_enrichment::fork_enrichment_def::{BATCH_SIZE, ENRICHMENT, ORIGINAL};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    GetProperty, Logger, MinifiError, OnTriggerResult, ProcessContext, ProcessError,
    ProcessSession, Schedule, Trigger,
};
use uuid::Uuid;

#[derive(ComponentIdentifier)]
pub(crate) struct ForkEnrichmentRs {
    batch_size: usize,
}

impl Schedule for ForkEnrichmentRs {
    fn schedule<Ctx: GetProperty, L: Logger>(
        context: &Ctx,
        _logger: &L,
    ) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self {
            batch_size: context.get_property(&BATCH_SIZE)?.unwrap_or(usize::MAX),
        })
    }
}

impl Trigger for ForkEnrichmentRs {
    fn trigger<Context, Session, Lggr>(
        &self,
        _context: &mut Context,
        session: &mut Session,
        _logger: &Lggr,
    ) -> Result<OnTriggerResult, ProcessError>
    where
        Context: ProcessContext,
        Session: ProcessSession<FlowFile = Context::FlowFile>,
        Lggr: Logger,
    {
        for _ in 0..self.batch_size {
            let Some(mut original) = session.get() else {
                break;
            };
            let mut enrichment = session.clone_ff(&original)?;
            session.set_attribute(&mut original, FORK_ROLE_ATTR.name, "ORIGINAL")?;
            session.set_attribute(&mut enrichment, FORK_ROLE_ATTR.name, "ENRICHMENT")?;

            let group_id = Uuid::new_v4().to_string();
            session.set_attribute(&mut original, GROUP_ID_ATTR.name, &group_id)?;
            session.set_attribute(&mut enrichment, GROUP_ID_ATTR.name, &group_id)?;

            session.transfer(original, ORIGINAL.name)?;
            session.transfer(enrichment, ENRICHMENT.name)?;
        }

        Ok(OnTriggerResult::Ok)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use minifi_native::{
        ComponentIdentifier, MockFlowFile, MockLogger, MockProcessContext, MockProcessSession,
    };

    #[test]
    fn component_id() {
        assert_eq!(
            ForkEnrichmentRs::CLASS_NAME,
            "minifi_enrichment::processors::fork_enrichment::ForkEnrichmentRs"
        );
        assert_eq!(ForkEnrichmentRs::GROUP_NAME, "minifi_enrichment");
    }

    #[test]
    fn forks_into_original_and_enrichment_sharing_a_group_id() {
        let logger = MockLogger::new();
        let mut context = MockProcessContext::new();
        let processor = ForkEnrichmentRs::schedule(&context, &logger).unwrap();

        let mut session = MockProcessSession::new();
        session
            .input_flow_files
            .push(MockFlowFile::with_content(b"hello"));

        processor
            .trigger(&mut context, &mut session, &logger)
            .expect("trigger should succeed");

        let transferred = session.transferred_flow_files.borrow();
        assert_eq!(transferred.len(), 2);

        let original = transferred
            .iter()
            .find(|t| t.relationship == ORIGINAL.name)
            .expect("an original FlowFile should be transferred");
        let enrichment = transferred
            .iter()
            .find(|t| t.relationship == ENRICHMENT.name)
            .expect("an enrichment FlowFile should be transferred");

        assert_eq!(
            original
                .flow_file
                .attributes
                .get(FORK_ROLE_ATTR.name)
                .unwrap(),
            "ORIGINAL"
        );
        assert_eq!(
            enrichment
                .flow_file
                .attributes
                .get(FORK_ROLE_ATTR.name)
                .unwrap(),
            "ENRICHMENT"
        );

        let orig_group = original
            .flow_file
            .attributes
            .get(GROUP_ID_ATTR.name)
            .unwrap();
        let enr_group = enrichment
            .flow_file
            .attributes
            .get(GROUP_ID_ATTR.name)
            .unwrap();
        assert_eq!(orig_group, enr_group);
        assert!(!orig_group.is_empty());
    }
}
