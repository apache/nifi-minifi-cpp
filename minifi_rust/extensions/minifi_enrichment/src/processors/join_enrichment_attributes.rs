use std::time::{Duration, Instant};

use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    FlowFileStore, GetProperty, Logger, MinifiError, MutTrigger, OnTriggerResult, ProcessContext,
    ProcessError, ProcessSession, Schedule, warn,
};

use crate::processors::attributes::{FORK_ROLE_ATTR, GROUP_ID_ATTR, JOIN_ROLE_ATTR, Role};
use crate::processors::join_enrichment_attributes::join_enrichment_attributes_def::{
    BATCH_SIZE, INVALID, JOINED, ORIGINAL, TIMEOUT_PROP, TIMEOUT_REL,
};

mod join_enrichment_attributes_def;

#[derive(ComponentIdentifier)]
pub(crate) struct JoinEnrichmentAttributesRs {
    batch_size: usize,
    timeout: Option<Duration>,
    pending: FlowFileStore<String>,
}

fn get_role<Session: ProcessSession>(
    session: &Session,
    flow_file: &Session::FlowFile,
) -> Option<Role> {
    session
        .get_attribute(flow_file, FORK_ROLE_ATTR.name)?
        .parse()
        .ok()
}

fn get_role_and_group_id<Session: ProcessSession>(
    flow_file: &Session::FlowFile,
    session: &Session,
) -> Option<(Role, String)> {
    let role = get_role(session, flow_file)?;
    let group_id = session
        .get_required_attribute(flow_file, GROUP_ID_ATTR.name)
        .ok()?;
    Some((role, group_id))
}

fn join<Session: ProcessSession>(
    session: &mut Session,
    original_ff: Session::FlowFile,
    enrichment_ff: Session::FlowFile,
) -> Result<(), MinifiError> {
    let mut joined_ff = session.clone_ff(&original_ff)?;

    let mut enrichment_attrs: Vec<(String, String)> = Vec::new();
    session.for_each_attribute(&enrichment_ff, |key, value| {
        enrichment_attrs.push((key.to_string(), value.to_string()));
    });
    for (key, value) in &enrichment_attrs {
        session.set_attribute(&mut joined_ff, key, value)?;
    }

    session.set_attribute(&mut joined_ff, JOIN_ROLE_ATTR.name, "JOINED")?;

    session.transfer(original_ff, ORIGINAL.name)?;
    session.transfer(enrichment_ff, ORIGINAL.name)?;
    session.transfer(joined_ff, JOINED.name)?;
    Ok(())
}

impl JoinEnrichmentAttributesRs {
    fn handle_flow_file<Session>(
        &mut self,
        incoming: Session::FlowFile,
        session: &mut Session,
        role: Role,
        group_id: String,
    ) -> Result<(), MinifiError>
    where
        Session: ProcessSession,
    {
        let Some(pending) = self.pending.take(session, &group_id)? else {
            return self.pending.store(session, group_id, incoming);
        };

        match (role, get_role(session, &pending)) {
            (Role::Original, Some(Role::Enrichment)) => join(session, incoming, pending),
            (Role::Enrichment, Some(Role::Original)) => join(session, pending, incoming),
            _ => {
                session.transfer(incoming, INVALID.name)?;
                session.transfer(pending, INVALID.name)?;
                Ok(())
            }
        }
    }
}

impl Schedule for JoinEnrichmentAttributesRs {
    fn schedule<Ctx: GetProperty, L: Logger>(
        context: &Ctx,
        _logger: &L,
    ) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self {
            batch_size: context.get_property(&BATCH_SIZE)?.unwrap_or(usize::MAX),
            timeout: context
                .get_property(&TIMEOUT_PROP)?
                .filter(|d| !d.is_zero()),
            pending: FlowFileStore::new(),
        })
    }
}

impl JoinEnrichmentAttributesRs {
    fn on_trigger<Session, Lggr>(
        &mut self,
        session: &mut Session,
        logger: &Lggr,
        now: Instant,
    ) -> Result<OnTriggerResult, ProcessError>
    where
        Session: ProcessSession,
        Lggr: Logger,
    {
        for _ in 0..self.batch_size {
            let Some(flow_file) = session.get() else {
                break;
            };

            let Some((role, group_id)) = get_role_and_group_id(&flow_file, session) else {
                warn!(logger, "Missing required attribute");
                session.transfer(flow_file, INVALID.name)?;
                continue;
            };

            self.handle_flow_file(flow_file, session, role, group_id)?;
        }

        if let Some(timeout) = self.timeout {
            for timed_out in self.pending.drain_expired(session, now, timeout)? {
                session.transfer(timed_out, TIMEOUT_REL.name)?;
            }
        }

        Ok(OnTriggerResult::Ok)
    }
}

impl MutTrigger for JoinEnrichmentAttributesRs {
    fn trigger<Context, Session, Lggr>(
        &mut self,
        _context: &mut Context,
        session: &mut Session,
        logger: &Lggr,
    ) -> Result<OnTriggerResult, ProcessError>
    where
        Context: ProcessContext,
        Session: ProcessSession<FlowFile = Context::FlowFile>,
        Lggr: Logger,
    {
        self.on_trigger(session, logger, Instant::now())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use minifi_native::{MockFlowFile, MockLogger, MockProcessContext, MockProcessSession};

    fn flow_file(role: &str, group_id: &str, content: &[u8]) -> MockFlowFile {
        let mut ff = MockFlowFile::with_content(content);
        ff.attributes
            .insert(FORK_ROLE_ATTR.name.to_string(), role.to_string());
        ff.attributes
            .insert(GROUP_ID_ATTR.name.to_string(), group_id.to_string());
        ff
    }

    fn scheduled() -> (JoinEnrichmentAttributesRs, MockProcessContext, MockLogger) {
        let logger = MockLogger::new();
        let context = MockProcessContext::new();
        let processor = JoinEnrichmentAttributesRs::schedule(&context, &logger).unwrap();
        (processor, context, logger)
    }

    fn with_timeout(timeout: Duration) -> JoinEnrichmentAttributesRs {
        JoinEnrichmentAttributesRs {
            batch_size: usize::MAX,
            timeout: Some(timeout),
            pending: FlowFileStore::new(),
        }
    }

    #[test]
    fn joins_original_and_enrichment_arriving_on_separate_triggers() {
        let (mut processor, mut context, logger) = scheduled();

        // Trigger 1: only the original arrives -> stored, nothing transferred.
        let mut session1 = MockProcessSession::new();
        session1
            .input_flow_files
            .push(flow_file("ORIGINAL", "g1", b"original-content"));
        processor
            .trigger(&mut context, &mut session1, &logger)
            .unwrap();
        assert_eq!(session1.num_of_transferred_flow_files(), 0);

        // Trigger 2: the enrichment arrives (with an extra attribute) -> join.
        let mut enrichment = flow_file("ENRICHMENT", "g1", b"enrichment-content");
        enrichment
            .attributes
            .insert("enriched".to_string(), "yes".to_string());
        let mut session2 = MockProcessSession::new();
        session2.input_flow_files.push(enrichment);
        processor
            .trigger(&mut context, &mut session2, &logger)
            .unwrap();

        let transferred = session2.transferred_flow_files.borrow();
        assert_eq!(transferred.len(), 3);
        assert_eq!(
            transferred
                .iter()
                .filter(|t| t.relationship == ORIGINAL.name)
                .count(),
            2
        );

        let joined = transferred
            .iter()
            .find(|t| t.relationship == JOINED.name)
            .expect("a joined FlowFile should be transferred");
        // Original's content is preserved.
        assert!(joined.flow_file.content_eq("original-content"));
        // Enrichment's attributes are merged in, and the role is set to JOINED.
        assert_eq!(joined.flow_file.attributes.get("enriched").unwrap(), "yes");
        assert_eq!(
            joined
                .flow_file
                .attributes
                .get(JOIN_ROLE_ATTR.name)
                .unwrap(),
            "JOINED"
        );
    }

    #[test]
    fn missing_required_attribute_routes_to_invalid() {
        let (mut processor, mut context, logger) = scheduled();

        let mut session = MockProcessSession::new();
        session
            .input_flow_files
            .push(MockFlowFile::with_content(b"no attributes"));
        processor
            .trigger(&mut context, &mut session, &logger)
            .unwrap();

        let transferred = session.transferred_flow_files.borrow();
        assert_eq!(transferred.len(), 1);
        assert_eq!(transferred[0].relationship, INVALID.name);
    }

    #[test]
    fn duplicate_role_for_a_group_routes_both_to_invalid() {
        let (mut processor, mut context, logger) = scheduled();

        // Trigger 1: first original stored.
        let mut session1 = MockProcessSession::new();
        session1
            .input_flow_files
            .push(flow_file("ORIGINAL", "g1", b"first"));
        processor
            .trigger(&mut context, &mut session1, &logger)
            .unwrap();
        assert_eq!(session1.num_of_transferred_flow_files(), 0);

        // Trigger 2: a second original for the same group -> both invalid.
        let mut session2 = MockProcessSession::new();
        session2
            .input_flow_files
            .push(flow_file("ORIGINAL", "g1", b"second"));
        processor
            .trigger(&mut context, &mut session2, &logger)
            .unwrap();

        let transferred = session2.transferred_flow_files.borrow();
        assert_eq!(transferred.len(), 2);
        assert!(transferred.iter().all(|t| t.relationship == INVALID.name));
    }

    #[test]
    fn unpaired_half_is_routed_to_timeout_after_the_timeout_elapses() {
        let logger = MockLogger::new();
        let mut processor = with_timeout(Duration::from_secs(60));
        let start = Instant::now();

        // Trigger 1: the original arrives and is stored; nothing is emitted yet.
        let mut session1 = MockProcessSession::new();
        session1
            .input_flow_files
            .push(flow_file("ORIGINAL", "g1", b"lonely"));
        processor.on_trigger(&mut session1, &logger, start).unwrap();
        assert_eq!(session1.num_of_transferred_flow_files(), 0);

        // Trigger 2, long past the timeout, with no pair: the stored half is
        // handed back and routed to `timeout`.
        let mut session2 = MockProcessSession::new();
        processor
            .on_trigger(&mut session2, &logger, start + Duration::from_secs(120))
            .unwrap();

        let transferred = session2.transferred_flow_files.borrow();
        assert_eq!(transferred.len(), 1);
        assert_eq!(transferred[0].relationship, TIMEOUT_REL.name);
        assert!(transferred[0].flow_file.content_eq("lonely"));
        assert!(processor.pending.is_empty());
    }

    #[test]
    fn half_within_the_timeout_window_is_kept() {
        let logger = MockLogger::new();
        let mut processor = with_timeout(Duration::from_secs(60));
        let start = Instant::now();

        let mut session1 = MockProcessSession::new();
        session1
            .input_flow_files
            .push(flow_file("ORIGINAL", "g1", b"waiting"));
        processor.on_trigger(&mut session1, &logger, start).unwrap();

        // A later trigger still inside the timeout window: nothing is emitted and
        // the half is still pending its pair.
        let mut session2 = MockProcessSession::new();
        processor
            .on_trigger(&mut session2, &logger, start + Duration::from_secs(1))
            .unwrap();
        assert_eq!(session2.num_of_transferred_flow_files(), 0);
        assert_eq!(processor.pending.len(), 1);
    }
}
