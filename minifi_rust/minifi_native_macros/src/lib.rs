use proc_macro::TokenStream;
use quote::quote;
use syn::{DeriveInput, parse_macro_input};

#[proc_macro_derive(ComponentIdentifier)]
pub fn derive_component_identifier(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;
    let name_str = name.to_string();

    let expanded = quote! {
        impl ::minifi_native::ComponentIdentifier for #name {
            const CLASS_NAME: &'static str = concat!(module_path!(), "::", #name_str);
            const GROUP_NAME: &'static str = env!("CARGO_PKG_NAME");
            const VERSION: &'static str = env!("CARGO_PKG_VERSION");
        }
    };

    TokenStream::from(expanded)
}

#[proc_macro_derive(DefaultMetrics)]
pub fn derive_default_metrics(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;
    let expanded = quote! {
        impl ::minifi_native::CalculateMetrics for #name {
            fn calculate_metrics(&self) -> Vec<(String, f64)> {
                vec![]
            }
        }
    };

    TokenStream::from(expanded)
}

#[proc_macro_derive(NoAdvancedProcessorFeatures)]
pub fn derive_no_advanced_processor_features(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;
    let expanded = quote! {
        impl ::minifi_native::AdvancedProcessorFeatures for #name {
                fn get_trigger_when_empty(&self) -> bool { false }
        }
    };

    TokenStream::from(expanded)
}
