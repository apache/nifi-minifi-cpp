use crate::api::attribute::GetAttribute;
use crate::api::property::{GetControllerService, GetProperty};
use crate::{
    ComponentIdentifier, EnableControllerService, MinifiError, ProcessContext, ProcessSession,
    Property,
};

pub struct ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    context: &'a PC,
    session: &'a PS,
    flow_file: Option<&'a PC::FlowFile>,
}

impl<'a, PC, PS> ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    pub(crate) fn new(
        context: &'a PC,
        session: &'a PS,
        flow_file: Option<&'a PC::FlowFile>,
    ) -> Self {
        Self {
            context,
            session,
            flow_file,
        }
    }
}
impl<'a, PC, PS> GetProperty for ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    fn get_property(&self, property: &Property) -> Result<Option<String>, MinifiError> {
        self.context.get_property(property, self.flow_file)
    }
}

impl<'a, PC, PS> GetControllerService for ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    fn get_controller_service<Cs>(&self, property: &Property) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + 'static,
    {
        self.context.get_controller_service(property)
    }
}

impl<'a, PC, PS> GetAttribute for ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    fn get_attribute(&self, name: &str) -> Result<Option<String>, MinifiError> {
        if let Some(ff) = &self.flow_file {
            Ok(self.session.get_attribute(ff, name))
        } else {
            Ok(None)
        }
    }
}
