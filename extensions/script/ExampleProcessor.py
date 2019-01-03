def describe(processor):
    processor.setDescription("Adds an attribute to your flow files")

def onInitialize(processor):
  processor.setSupportsDynamicProperties()

def onTrigger(context, session):
  flow_file = session.get()
  if flow_file is not None:
    flow_file.addAttribute("Python attribute","attributevalue")
    session.transfer(flow_file, REL_SUCCESS)
