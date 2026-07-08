use minifi_native::{GetAttribute, MinifiError};

#[derive(Debug, Clone, Copy, PartialEq)]
pub(crate) struct Dimensions {
    pub(crate) width: f32,
    pub(crate) height: f32,
}

impl Dimensions {
    pub(crate) fn from_image(img: &image::DynamicImage) -> Self {
        Self {
            width: img.width() as f32,
            height: img.height() as f32,
        }
    }

    pub(crate) fn original_from_attributes<Context: GetAttribute>(
        context: &Context,
    ) -> Result<Dimensions, MinifiError> {
        let orig_w = context
            .get_required_attribute("image.original.width")?
            .parse::<f32>()?;

        let orig_h = context
            .get_required_attribute("image.original.height")?
            .parse::<f32>()?;

        Ok(Dimensions {
            width: orig_w,
            height: orig_h,
        })
    }

    pub(crate) fn target_from_attributes<Context: GetAttribute>(
        context: &Context,
    ) -> Result<Dimensions, MinifiError> {
        let orig_w = context
            .get_required_attribute("image.target.width")?
            .parse::<f32>()?;

        let orig_h = context
            .get_required_attribute("image.target.height")?
            .parse::<f32>()?;

        Ok(Dimensions {
            width: orig_w,
            height: orig_h,
        })
    }
}
