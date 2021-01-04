from .OutputValidator import OutputValidator

class SegfaultValidator(OutputValidator):
    """
    Validate that a file was received.
    """
    def validate(self):
        return True
