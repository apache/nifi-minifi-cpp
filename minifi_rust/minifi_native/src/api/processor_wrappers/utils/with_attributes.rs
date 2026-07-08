#[macro_export]
macro_rules! impl_with_attributes {
    ($name:ident $(<$lt:lifetime>)?) => {
        impl $(<$lt>)? $name $(<$lt>)? {
            #[must_use]
            pub fn with_attribute(
                mut self,
                key: impl Into<std::borrow::Cow<'static, str>>,
                value: impl Into<std::borrow::Cow<'static, str>>,
            ) -> Self {
                self.attributes_to_add.push((key.into(), value.into()).into());
                self
            }

            #[must_use]
            pub fn with_attributes<K, V>(
                mut self,
                attributes: impl IntoIterator<Item = (K, V)>
            ) -> Self
            where
                K: Into<std::borrow::Cow<'static, str>>,
                V: Into<std::borrow::Cow<'static, str>>,
            {
                self.attributes_to_add
                    .extend(attributes.into_iter().map(|(k, v)| (k.into(), v.into()).into()));
                self
            }
        }
    };
}
