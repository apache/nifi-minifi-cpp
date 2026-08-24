// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

use std::any::Any;
use std::collections::HashMap;
use std::hash::Hash;
use std::time::{Duration, Instant};

use crate::MinifiError;
use crate::api::ProcessSession;

struct StoredEntry {
    flow_file: Box<dyn Any + Send>,
    stored_at: Instant,
}

pub struct FlowFileStore<K> {
    stored: HashMap<K, StoredEntry>,
}

impl<K: Eq + Hash> FlowFileStore<K> {
    pub fn new() -> Self {
        Self {
            stored: HashMap::new(),
        }
    }

    pub fn store<S>(
        &mut self,
        session: &mut S,
        key: K,
        flow_file: S::FlowFile,
    ) -> Result<(), MinifiError>
    where
        S: ProcessSession,
    {
        let stashed = session.stash(flow_file)?;
        self.stored.insert(
            key,
            StoredEntry {
                flow_file: Box::new(stashed),
                stored_at: Instant::now(),
            },
        );
        Ok(())
    }

    pub fn take<S>(&mut self, session: &mut S, key: &K) -> Result<Option<S::FlowFile>, MinifiError>
    where
        S: ProcessSession,
    {
        let Some(entry) = self.stored.remove(key) else {
            return Ok(None);
        };
        Ok(Some(Self::add_back(session, entry)?))
    }

    pub fn drain_expired<S>(
        &mut self,
        session: &mut S,
        now: Instant,
        max_age: Duration,
    ) -> Result<Vec<S::FlowFile>, MinifiError>
    where
        S: ProcessSession,
        K: Clone,
    {
        let expired_keys: Vec<K> = self
            .stored
            .iter()
            .filter(|(_, entry)| now.saturating_duration_since(entry.stored_at) > max_age)
            .map(|(key, _)| key.clone())
            .collect();

        let mut expired = Vec::with_capacity(expired_keys.len());
        for key in expired_keys {
            let entry = self.stored.remove(&key).expect("key came from the map");
            expired.push(Self::add_back(session, entry)?);
        }
        Ok(expired)
    }

    fn add_back<S>(session: &mut S, entry: StoredEntry) -> Result<S::FlowFile, MinifiError>
    where
        S: ProcessSession,
    {
        let stashed = *entry
            .flow_file
            .downcast::<S::StashedFlowFile>()
            .expect("stashed FlowFile type is stable within a processor instance");
        session.unstash(stashed)
    }

    pub fn contains(&self, key: &K) -> bool {
        self.stored.contains_key(key)
    }

    pub fn len(&self) -> usize {
        self.stored.len()
    }

    pub fn is_empty(&self) -> bool {
        self.stored.is_empty()
    }
}

impl<K: Eq + Hash> Default for FlowFileStore<K> {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::MockFlowFile;
    use crate::mock::MockProcessSession;

    #[test]
    fn store_then_take_round_trips_a_flow_file() {
        let mut session = MockProcessSession::new();
        let mut store: FlowFileStore<String> = FlowFileStore::new();

        let mut ff = MockFlowFile::new();
        ff.attributes
            .insert("enrichment.group.id".to_string(), "g1".to_string());
        let id = ff.id.clone();

        store
            .store(&mut session, "g1".to_string(), ff)
            .expect("store should succeed");
        assert_eq!(store.len(), 1);
        assert!(store.contains(&"g1".to_string()));

        let taken = store
            .take(&mut session, &"g1".to_string())
            .expect("take should succeed")
            .expect("a flow file should be stored under g1");
        assert_eq!(taken.id, id);
        assert!(store.is_empty());
    }

    #[test]
    fn take_missing_key_returns_none() {
        let mut session = MockProcessSession::new();
        let mut store: FlowFileStore<String> = FlowFileStore::new();

        let result = store
            .take(&mut session, &"missing".to_string())
            .expect("take should not error");
        assert!(result.is_none());
    }

    #[test]
    fn drain_expired_only_returns_entries_older_than_max_age() {
        let mut session = MockProcessSession::new();
        let mut store: FlowFileStore<String> = FlowFileStore::new();

        store
            .store(&mut session, "g1".to_string(), MockFlowFile::new())
            .expect("store should succeed");

        let max_age = Duration::from_secs(60);
        // `now` right after storing: nothing has aged past max_age.
        let just_now = Instant::now();
        let drained = store
            .drain_expired(&mut session, just_now, max_age)
            .expect("drain should succeed");
        assert!(drained.is_empty());
        assert_eq!(store.len(), 1);

        // `now` far in the future: the entry is older than max_age and comes back.
        let future = just_now + Duration::from_secs(120);
        let drained = store
            .drain_expired(&mut session, future, max_age)
            .expect("drain should succeed");
        assert_eq!(drained.len(), 1);
        assert!(store.is_empty());
    }
}
