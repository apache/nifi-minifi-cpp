use crate::api::process_session::IoState;
use crate::api::{InputStream, ProcessSession};
use crate::{MinifiError, MockFlowFile};
use itertools::Itertools;
use std::cell::RefCell;
use std::io::Read;

pub struct TransferredFlowFile {
    pub relationship: String,
    pub flow_file: MockFlowFile,
}

pub struct MockProcessSession {
    pub input_flow_files: Vec<MockFlowFile>,
    pub transferred_flow_files: RefCell<Vec<TransferredFlowFile>>,
}

impl ProcessSession for MockProcessSession {
    type FlowFile = MockFlowFile;

    fn create(&mut self) -> Result<Self::FlowFile, MinifiError> {
        Ok(Self::FlowFile::new())
    }
    fn get(&mut self) -> Option<Self::FlowFile> {
        self.input_flow_files.pop()
    }
    fn transfer(&self, flow_file: Self::FlowFile, relationship: &str) -> Result<(), MinifiError> {
        self.transferred_flow_files
            .borrow_mut()
            .push(TransferredFlowFile {
                relationship: relationship.to_string(),
                flow_file,
            });
        Ok(())
    }

    fn remove(&mut self, _flow_file: Self::FlowFile) -> Result<(), MinifiError> {
        Ok(())
    }

    fn set_attribute(
        &self,
        flow_file: &mut Self::FlowFile,
        attr_key: &str,
        attr_value: &str,
    ) -> Result<(), MinifiError> {
        flow_file
            .attributes
            .insert(attr_key.to_string(), attr_value.to_string());
        Ok(())
    }
    fn get_attribute(&self, flow_file: &Self::FlowFile, attr_key: &str) -> Option<String> {
        flow_file.attributes.get(attr_key).cloned()
    }

    fn on_attributes<F: FnMut(&str, &str)>(
        &self,
        flow_file: &Self::FlowFile,
        mut process_attr: F,
    ) -> bool {
        // Sorting for deterministic tests.
        for (attr_key, attr_value) in flow_file.attributes.iter().sorted_by_key(|x| x.0) {
            process_attr(attr_key, attr_value);
        }
        true
    }

    fn write(&self, flow_file: &Self::FlowFile, data: &[u8]) -> Result<(), MinifiError> {
        *flow_file.content.borrow_mut() = data.to_vec();
        Ok(())
    }

    fn write_lazy<'a>(
        &self,
        flow_file: &Self::FlowFile,
        mut stream: Box<dyn Read + 'a>,
    ) -> Result<(), MinifiError> {
        stream
            .read_to_end(&mut flow_file.content.borrow_mut())
            .expect("Mock data should be readable");
        Ok(())
    }

    fn write_stream<F, R>(&self, flow_file: &Self::FlowFile, callback: F) -> Result<R, MinifiError>
    where
        F: FnOnce(
            &mut dyn crate::api::process_session::OutputStream,
        ) -> Result<(R, IoState), MinifiError>,
    {
        let mut new_content: Vec<u8> = Vec::new();
        let mut cursor = std::io::Cursor::new(&mut new_content);
        let (r, _state) = callback(&mut cursor)?;
        *flow_file.content.borrow_mut() = new_content;
        Ok(r)
    }

    fn read(&self, flow_file: &Self::FlowFile) -> Option<Vec<u8>> {
        Some(flow_file.content.borrow().clone())
    }

    fn read_stream<F, R>(&self, _flow_file: &Self::FlowFile, _callback: F) -> Result<R, MinifiError>
    where
        F: FnOnce(&mut dyn InputStream) -> Result<R, MinifiError>,
    {
        unimplemented!("Not implemented yet")
    }

    fn read_in_batches<F>(
        &self,
        flow_file: &Self::FlowFile,
        batch_size: usize,
        mut process_batch: F,
    ) -> Result<(), MinifiError>
    where
        F: FnMut(&[u8]) -> Result<(), MinifiError>,
    {
        for chunk in flow_file.content.borrow().chunks(batch_size) {
            process_batch(chunk)?;
        }
        Ok(())
    }

    fn get_flow_file_id(&self, flow_file: &Self::FlowFile) -> Result<String, MinifiError> {
        Ok(flow_file.id.clone())
    }
}

impl MockProcessSession {
    pub fn new() -> Self {
        Self {
            transferred_flow_files: RefCell::new(Vec::new()),
            input_flow_files: Vec::new(),
        }
    }

    pub fn num_of_transferred_flow_files(&self) -> usize {
        self.transferred_flow_files.borrow().len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::api::process_session::IoState;

    #[test]
    fn test_read_in_batches() {
        let session = MockProcessSession::new();
        let mut flow_file = MockFlowFile::new();
        flow_file.content = RefCell::from("Hello, World!".to_string().as_bytes().to_vec());
        let mut vec: Vec<u8> = Vec::new();

        let res = session.read_in_batches(&mut flow_file, 1, |batch| {
            assert_eq!(batch.len(), 1);
            vec.push(batch[0]);
            Ok(())
        });

        assert!(res.is_ok());

        assert_eq!(vec.len(), 13);
        assert_eq!(vec, b"Hello, World!");
    }

    #[test]
    fn test_write_stream_replaces_content() {
        let session = MockProcessSession::new();
        let flow_file = MockFlowFile::new();
        // Pre-populate the flow file with longer content
        *flow_file.content.borrow_mut() = b"Hello, World!".to_vec();

        let res: Result<(), MinifiError> = session.write_stream(&flow_file, |stream| {
            stream.write_all(b"Hi")?;
            Ok(((), IoState::Ok))
        });

        assert!(res.is_ok());
        // Old trailing bytes must not survive
        assert_eq!(*flow_file.content.borrow(), b"Hi");
    }
}
