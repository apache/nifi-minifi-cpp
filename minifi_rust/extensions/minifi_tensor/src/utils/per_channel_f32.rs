use crate::utils::per_channel_f32::PerChannelF32::TriChannel;
use minifi_native::{MinifiError, PropertyConstraints, PropertySchema, PropertyType};

#[derive(PartialEq, Debug)]
pub(crate) enum PerChannelF32 {
    SingleChannel(f32),
    TriChannel([f32; 3]),
}

impl PerChannelF32 {
    pub fn contains_zero(&self) -> bool {
        match self {
            Self::SingleChannel(f) => *f == 0.0f32,
            TriChannel(channels) => channels.contains(&0.0f32),
        }
    }

    pub fn per_channel(&self, channel_id: usize) -> f32 {
        match self {
            Self::SingleChannel(f) => *f,
            TriChannel(channels) => channels[channel_id],
        }
    }
}

impl PropertySchema for PerChannelF32 {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = false;
}

impl PropertyType for PerChannelF32 {
    type Output = PerChannelF32;

    fn parse(input: &str) -> Result<Self::Output, MinifiError> {
        let parts: Vec<&str> = input.split(',').map(|s| s.trim()).collect();
        match parts.len() {
            1 => Ok(Self::SingleChannel(parts[0].parse::<f32>()?)),
            3 => {
                let f = parts[0].parse::<f32>()?;
                let s = parts[1].parse::<f32>()?;
                let t = parts[2].parse::<f32>()?;
                Ok(TriChannel([f, s, t]))
            }
            _n => Err(MinifiError::validation(
                "expected 1 or 3 comma-separated floats",
            )),
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::utils::per_channel_f32::PerChannelF32;
    use minifi_native::PropertyType;

    #[test]
    fn test_parse_per_channel_f32_accepts_scalar_and_triple() {
        assert_eq!(
            PerChannelF32::parse("0.5").unwrap(),
            PerChannelF32::SingleChannel(0.5)
        );
        assert_eq!(
            PerChannelF32::parse("0.485, 0.456, 0.406").unwrap(),
            PerChannelF32::TriChannel([0.485, 0.456, 0.406])
        );
        assert!(PerChannelF32::parse("1, 2").is_err());
        assert!(PerChannelF32::parse("1, 2, three").is_err());
    }
}
