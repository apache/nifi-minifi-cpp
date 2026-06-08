use std::fmt::Formatter;

pub enum Content<'a> {
    Buffer(Vec<u8>),
    Stream(Box<dyn std::io::Read + 'a>),
}

impl std::fmt::Debug for Content<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            Content::Buffer(b) => f.debug_struct("Content").field("Buffer", &b).finish(),
            Content::Stream(_s) => f.debug_struct("Content::Stream").finish(),
        }
    }
}

impl From<Vec<u8>> for Content<'_> {
    fn from(v: Vec<u8>) -> Self {
        Content::Buffer(v)
    }
}

impl From<String> for Content<'_> {
    fn from(s: String) -> Self {
        Content::Buffer(s.into_bytes())
    }
}
