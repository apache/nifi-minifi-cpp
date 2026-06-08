use minifi_native_sys::{
    MinifiInputStream, MinifiInputStreamRead, MinifiOutputStream, MinifiOutputStreamWrite,
};
use std::io::{BufRead, Error, ErrorKind, Read};

#[derive(Debug)]
pub struct CffiInputStream<'a> {
    ptr: *mut MinifiInputStream,
    buffer: [u8; 8192],
    pos: usize,
    cap: usize,
    total_read: usize,
    _marker: std::marker::PhantomData<&'a ()>,
}

unsafe impl<'a> Send for CffiInputStream<'a> {}

impl<'a> CffiInputStream<'a> {
    pub fn new(ptr: *mut MinifiInputStream) -> Self {
        Self {
            ptr,
            buffer: [0u8; 8192],
            pos: 0,
            cap: 0,
            total_read: 0,
            _marker: std::marker::PhantomData,
        }
    }

    pub fn total_bytes_read(&self) -> usize {
        self.total_read
    }
}

impl<'a> Read for CffiInputStream<'a> {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        // Delegate to the BufRead implementation to ensure consistency
        let nread = {
            let mut rem = self.fill_buf()?;
            rem.read(buf)?
        };
        self.consume(nread);
        Ok(nread)
    }
}

impl<'a> BufRead for CffiInputStream<'a> {
    fn fill_buf(&mut self) -> std::io::Result<&[u8]> {
        if self.pos >= self.cap {
            unsafe {
                let ret = MinifiInputStreamRead(
                    self.ptr,
                    self.buffer.as_mut_ptr() as *mut std::ffi::c_char,
                    self.buffer.len(),
                );
                if ret < 0 {
                    return Err(Error::new(ErrorKind::Other, "Minifi Read Error"));
                }
                self.cap = ret as usize;
                self.pos = 0;
            }
        }
        Ok(&self.buffer[self.pos..self.cap])
    }

    fn consume(&mut self, amount: usize) {
        let actual_consumed = std::cmp::min(amount, self.cap - self.pos);
        self.pos += actual_consumed;
        self.total_read += actual_consumed;
    }
}

#[derive(Debug)]
pub struct CffiOutputStream<'a> {
    ptr: *mut MinifiOutputStream,
    written_bytes: usize,
    _marker: std::marker::PhantomData<&'a ()>,
}

impl<'a> CffiOutputStream<'a> {
    pub(crate) fn new(ptr: *mut MinifiOutputStream) -> Self {
        Self {
            ptr,
            written_bytes: 0,
            _marker: std::marker::PhantomData,
        }
    }
}

impl<'a> CffiOutputStream<'a> {
    pub fn written_bytes(&self) -> usize {
        self.written_bytes
    }
}

unsafe impl<'a> Send for CffiOutputStream<'a> {}

impl<'a> std::io::Write for CffiOutputStream<'a> {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        unsafe {
            let ret = MinifiOutputStreamWrite(
                self.ptr,
                buf.as_ptr() as *const std::ffi::c_char,
                buf.len(),
            );
            if ret < 0 {
                return Err(Error::new(ErrorKind::Other, "Minifi Write Error"));
            }
            self.written_bytes += ret as usize;
            Ok(ret as usize)
        }
    }
    fn flush(&mut self) -> std::io::Result<()> {
        Ok(()) // Handled by C++ session commit
    }
}
