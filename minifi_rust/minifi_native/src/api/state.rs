// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::MinifiError;
use serde::Serialize;
use serde::de::DeserializeOwned;
use std::collections::{HashMap, HashSet};
use std::ops::{Deref, DerefMut};

/// Whole-map access to a processor's persistent key/value state.
///
/// Not a processor-facing API: processors declare `#[persisted]` fields (see [`StateSchema`]) and the framework moves
/// them through this trait around each trigger. Implemented by the FFI context and by test doubles.
pub trait StateAccess {
    /// Returns the entire stored state, including a write this session has made but not yet committed.
    fn get_state(&self) -> Result<HashMap<String, String>, MinifiError>;

    /// Replaces the entire stored state. A write of what is already stored is dropped, so re-sending an unchanged
    /// state costs no storage write.
    fn set_state(&self, state: &HashMap<String, String>) -> Result<(), MinifiError>;

    /// Stores an empty state, which is not quite the same as removing the stored entry but is indistinguishable here.
    fn clear_state(&self) -> Result<(), MinifiError> {
        self.set_state(&HashMap::new())
    }
}

/// A type that round-trips through a single state entry.
///
/// Implemented for the scalar field types, so `#[persisted] offset: u64` stores `42` - the same plain representation
/// a C++ processor writes. Wrap a type in [`Json`] to use its `serde` form instead.
pub trait StateValue: Sized {
    fn to_state_string(&self) -> Result<String, MinifiError>;

    fn from_state_string(raw: &str) -> Result<Self, MinifiError>;
}

impl StateValue for String {
    fn to_state_string(&self) -> Result<String, MinifiError> {
        Ok(self.clone())
    }

    fn from_state_string(raw: &str) -> Result<Self, MinifiError> {
        Ok(raw.to_owned())
    }
}

macro_rules! impl_state_value_via_display {
    ($($ty:ty),* $(,)?) => {
        $(
            impl StateValue for $ty {
                fn to_state_string(&self) -> Result<String, MinifiError> {
                    Ok(self.to_string())
                }

                fn from_state_string(raw: &str) -> Result<Self, MinifiError> {
                    raw.parse().map_err(|_| {
                        MinifiError::validation(format!(
                            concat!("'{}' is not a valid ", stringify!($ty)),
                            raw
                        ))
                    })
                }
            }
        )*
    };
}

impl_state_value_via_display!(
    bool, i8, i16, i32, i64, isize, u8, u16, u32, u64, usize, f32, f64
);

/// Stores `T` using its `serde` JSON representation.
///
/// For state with no natural flat form. Keeping it a wrapper makes the serialization visible at the field rather
/// than an implicit part of every extension.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Json<T>(pub T);

impl<T> StateValue for Json<T>
where
    T: Serialize + DeserializeOwned,
{
    fn to_state_string(&self) -> Result<String, MinifiError> {
        serde_json::to_string(&self.0)
            .map_err(|err| MinifiError::validation(format!("could not serialize state: {err}")))
    }

    fn from_state_string(raw: &str) -> Result<Self, MinifiError> {
        serde_json::from_str(raw)
            .map(Json)
            .map_err(|err| MinifiError::validation(format!("could not deserialize state: {err}")))
    }
}

impl<T> Deref for Json<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T> DerefMut for Json<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

/// A type that owns a *group* of state entries rather than a single one, for layouts spanning several keys.
/// Declare such a field `#[persisted(flatten)]`.
///
/// [`store_entries`](StateEntries::store_entries) gets `&mut` so a group that shrinks can remove its own stale keys;
/// an orphan left behind would be read back as live on the next load.
pub trait StateEntries {
    fn load_entries(&mut self, state: &HashMap<String, String>) -> Result<(), MinifiError>;

    fn store_entries(&self, state: &mut HashMap<String, String>) -> Result<(), MinifiError>;
}

/// Describes how a [`crate::SingleThreadedStateful`] processor's state maps onto the state store.
///
/// Implemented by `#[derive(StateSchema)]` from the `#[persisted]` fields. The framework loads before and stores
/// after each trigger, so a save commits with that trigger's session.
pub trait StateSchema {
    fn load_from_state(&mut self, state: &HashMap<String, String>) -> Result<(), MinifiError>;

    fn store_to_state(&self, state: &mut HashMap<String, String>) -> Result<(), MinifiError>;
}

/// Tracks which entries of a listing have already been emitted, so a source processor can skip them on later
/// triggers and across restarts. Only the newest timestamp and the keys sharing it are retained.
///
/// Declare it `#[persisted(flatten)]`. The stored layout is byte-for-byte the one C++ `utils::ListingStateManager`
/// writes, so a processor ported to Rust resumes from the state its C++ counterpart left behind.
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct ListingState {
    latest_timestamp_ms: i64,
    listed_keys: HashSet<String>,
}

const LISTED_TIMESTAMP_KEY: &str = "listed_timestamp";
const LISTED_KEY_PREFIX: &str = "listed_key.";

impl StateEntries for ListingState {
    fn load_entries(&mut self, state: &HashMap<String, String>) -> Result<(), MinifiError> {
        // Tolerate a corrupt watermark like C++ `getLatestListedKeyTimestampInMilliseconds` does. Failing here would
        // stall every subsequent trigger with no way out but clearing state by hand; re-listing is recoverable.
        self.latest_timestamp_ms = state
            .get(LISTED_TIMESTAMP_KEY)
            .and_then(|raw| i64::from_state_string(raw).ok())
            .unwrap_or(0);
        self.listed_keys = state
            .iter()
            .filter(|(key, _)| key.starts_with(LISTED_KEY_PREFIX))
            .map(|(_, value)| value.clone())
            .collect();
        Ok(())
    }

    fn store_entries(&self, state: &mut HashMap<String, String>) -> Result<(), MinifiError> {
        // The retained set shrinks when the watermark advances; a leftover entry would read back as already-listed
        // and skip that file forever.
        state.retain(|key, _| !key.starts_with(LISTED_KEY_PREFIX));
        state.insert(
            LISTED_TIMESTAMP_KEY.to_owned(),
            self.latest_timestamp_ms.to_state_string()?,
        );
        // Sorted so an unchanged listing produces an identical map, which is what lets the agent skip the write.
        let mut keys: Vec<&String> = self.listed_keys.iter().collect();
        keys.sort_unstable();
        for (index, key) in keys.into_iter().enumerate() {
            state.insert(format!("{LISTED_KEY_PREFIX}{index}"), key.clone());
        }
        Ok(())
    }
}

impl ListingState {
    /// Returns true if an entry with this `key` and `timestamp_ms` was already emitted in a previous listing.
    pub fn was_listed_already(&self, key: &str, timestamp_ms: i64) -> bool {
        match timestamp_ms.cmp(&self.latest_timestamp_ms) {
            std::cmp::Ordering::Less => true,
            std::cmp::Ordering::Equal => self.listed_keys.contains(key),
            std::cmp::Ordering::Greater => false,
        }
    }

    /// Records that an entry with this `key` and `timestamp_ms` has now been emitted.
    pub fn update(&mut self, key: &str, timestamp_ms: i64) {
        match timestamp_ms.cmp(&self.latest_timestamp_ms) {
            std::cmp::Ordering::Greater => {
                self.latest_timestamp_ms = timestamp_ms;
                self.listed_keys.clear();
                self.listed_keys.insert(key.to_string());
            }
            std::cmp::Ordering::Equal => {
                self.listed_keys.insert(key.to_string());
            }
            std::cmp::Ordering::Less => {}
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn listing_state_skips_older_and_equal_seen_entries() {
        let mut state = ListingState::default();
        state.update("a.txt", 100);
        state.update("b.txt", 100);

        // Same timestamp, already-seen key -> skip; new key at same timestamp -> emit.
        assert!(state.was_listed_already("a.txt", 100));
        assert!(!state.was_listed_already("c.txt", 100));
        // Older timestamp -> skip; newer -> emit.
        assert!(state.was_listed_already("a.txt", 50));
        assert!(!state.was_listed_already("a.txt", 150));
    }

    #[test]
    fn advancing_timestamp_drops_older_keys() {
        let mut state = ListingState::default();
        state.update("a.txt", 100);
        state.update("b.txt", 200);
        assert!(state.was_listed_already("b.txt", 200));
        assert!(!state.was_listed_already("a.txt", 200));
    }

    #[test]
    fn a_corrupt_watermark_resets_the_listing_instead_of_failing() {
        let stored = HashMap::from([
            (
                LISTED_TIMESTAMP_KEY.to_owned(),
                "not-a-timestamp".to_owned(),
            ),
            (format!("{LISTED_KEY_PREFIX}0"), "a.txt".to_owned()),
        ]);

        let mut state = ListingState::default();
        state.load_entries(&stored).unwrap();

        assert!(!state.was_listed_already("a.txt", 100));
    }

    #[test]
    fn listing_state_round_trips_through_state_entries() {
        let mut state = ListingState::default();
        state.update("a.txt", 100);

        let mut stored = HashMap::new();
        state.store_entries(&mut stored).unwrap();

        let mut restored = ListingState::default();
        restored.load_entries(&stored).unwrap();
        assert_eq!(restored, state);
        assert!(restored.was_listed_already("a.txt", 100));
        assert!(!restored.was_listed_already("b.txt", 100));
    }

    #[test]
    fn listing_state_uses_the_cpp_entry_layout() {
        let mut state = ListingState::default();
        state.update("b.txt", 100);
        state.update("a.txt", 100);

        let mut stored = HashMap::new();
        state.store_entries(&mut stored).unwrap();

        // Plain decimal watermark plus positionally-indexed keys, as C++ ListingStateManager writes them.
        assert_eq!(
            stored.get("listed_timestamp").map(String::as_str),
            Some("100")
        );
        assert_eq!(
            stored.get("listed_key.0").map(String::as_str),
            Some("a.txt")
        );
        assert_eq!(
            stored.get("listed_key.1").map(String::as_str),
            Some("b.txt")
        );
        assert_eq!(stored.len(), 3);
    }

    #[test]
    fn storing_drops_stale_keys_when_the_watermark_advances() {
        let mut state = ListingState::default();
        state.update("a.txt", 100);
        state.update("b.txt", 100);
        let mut stored = HashMap::new();
        state.store_entries(&mut stored).unwrap();
        assert!(stored.contains_key("listed_key.1"));

        state.update("c.txt", 200);
        state.store_entries(&mut stored).unwrap();
        assert_eq!(
            stored.get("listed_key.0").map(String::as_str),
            Some("c.txt")
        );
        assert!(!stored.contains_key("listed_key.1"));

        let mut restored = ListingState::default();
        restored.load_entries(&stored).unwrap();
        assert!(restored.was_listed_already("c.txt", 200));
        // A surviving `listed_key.1` would bring b.txt back as retained at the new watermark, never to be listed.
        assert!(!restored.was_listed_already("b.txt", 200));
    }

    #[test]
    fn unchanged_listing_state_stores_an_identical_map() {
        let mut state = ListingState::default();
        state.update("a.txt", 100);
        state.update("b.txt", 100);

        let mut first = HashMap::new();
        state.store_entries(&mut first).unwrap();
        // Must be canonical, or the agent's change detection would write on every trigger.
        let mut reloaded = ListingState::default();
        reloaded.load_entries(&first).unwrap();
        let mut second = HashMap::new();
        reloaded.store_entries(&mut second).unwrap();
        assert_eq!(first, second);
    }

    #[test]
    fn state_values_use_plain_representations() {
        assert_eq!("foo".to_owned().to_state_string().unwrap(), "foo");
        assert_eq!(42u64.to_state_string().unwrap(), "42");
        assert_eq!(true.to_state_string().unwrap(), "true");
        assert_eq!(u64::from_state_string("42").unwrap(), 42);
        assert!(u64::from_state_string("not-a-number").is_err());
    }

    #[test]
    fn json_wrapper_round_trips() {
        let value = Json(vec!["a".to_owned(), "b".to_owned()]);
        let raw = value.to_state_string().unwrap();
        assert_eq!(raw, r#"["a","b"]"#);
        assert_eq!(Json::<Vec<String>>::from_state_string(&raw).unwrap(), value);
    }
}
