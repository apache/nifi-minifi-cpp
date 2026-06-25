use proc_macro::TokenStream;
use quote::quote;
use syn::{DeriveInput, ItemTrait, parse_macro_input};

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

#[proc_macro_attribute]
pub fn controller_service_api(_attr: TokenStream, item: TokenStream) -> TokenStream {
    let input = parse_macro_input!(item as ItemTrait);
    let name = &input.ident;
    let name_str = name.to_string();

    let expanded = quote! {
        #input

        impl ::minifi_native::ControllerServiceApi for dyn #name {
            const INTERFACE_NAME: &'static str = concat!(module_path!(), "::", #name_str);
        }
    };

    TokenStream::from(expanded)
}
