use crate::processors::kamikaze_processor::KamikazeBehaviour;
use minifi_native::{Property, StandardPropertyValidator};
use strum::VariantNames;

pub(crate) const ON_SCHEDULE_BEHAVIOUR: Property = Property {
    name: "On Schedule Behaviour",
    description: "What to do during the on_schedule method",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(KamikazeBehaviour::ReturnOk.into_str()),
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &KamikazeBehaviour::VARIANTS,
    allowed_type: "",
};

pub(crate) const ON_TRIGGER_BEHAVIOUR: Property = Property {
    name: "On Trigger Behaviour",
    description: "What to do during the on_trigger method",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(KamikazeBehaviour::ReturnOk.into_str()),
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &KamikazeBehaviour::VARIANTS,
    allowed_type: "",
};

pub(crate) const NOT_REGISTERED_PROPERTY: Property = Property {
    name: "Kamikaze Processor Property",
    description: "Property purposely left out of Processor description",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: "",
};
