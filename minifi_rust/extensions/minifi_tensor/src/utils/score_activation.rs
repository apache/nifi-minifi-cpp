use minifi_native::macros::PropertyType;
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};

#[derive(
    Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr, PropertyType,
)]
#[strum(serialize_all = "PascalCase", const_into_str)]
pub(crate) enum ScoreActivation {
    /// Cross-class softmax; classes are mutually exclusive (typical for
    /// ImageNet-trained ResNet/MobileNet/EfficientNet ONNX exports).
    Softmax,
    /// Per-class sigmoid; classes are independent (multi-label classifiers).
    Sigmoid,
    /// Pass-through — the model already emits probabilities.
    None,
}
