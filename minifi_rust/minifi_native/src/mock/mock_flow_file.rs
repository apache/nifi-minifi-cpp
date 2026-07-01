use crate::api::FlowFile;
use std::cell::RefCell;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};

pub struct MockFlowFile {
    pub content: RefCell<Vec<u8>>,
    pub attributes: HashMap<String, String>,
    pub id: String,
}

impl FlowFile for MockFlowFile {}

impl Default for MockFlowFile {
    fn default() -> Self {
        Self::new()
    }
}

fn next_mock_flow_file_id() -> String {
    static COUNTER: AtomicU64 = AtomicU64::new(0);
    let n = COUNTER.fetch_add(1, Ordering::Relaxed);
    format!("mock-flow-file-{:016x}", n)
}

impl MockFlowFile {
    pub fn new() -> MockFlowFile {
        MockFlowFile {
            content: RefCell::new(Vec::new()),
            attributes: HashMap::new(),
            id: next_mock_flow_file_id(),
        }
    }

    pub fn with_content(content: &[u8]) -> MockFlowFile {
        Self {
            content: RefCell::new(content.to_vec()),
            attributes: HashMap::new(),
            id: next_mock_flow_file_id(),
        }
    }

    pub fn content_len(&self) -> usize {
        self.content.borrow().len()
    }

    pub fn content_eq<S>(&self, other: S) -> bool
    where
        S: Into<String>,
    {
        let my_content = self.content.borrow();
        *my_content == other.into().as_bytes()
    }

    pub fn get_stream(&self) -> std::io::Cursor<Vec<u8>> {
        std::io::Cursor::new(self.content.borrow().clone())
    }
}
