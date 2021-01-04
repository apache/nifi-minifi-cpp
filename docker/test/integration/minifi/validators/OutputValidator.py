class OutputValidator(object):
    """
    Base output validator class. Validators must implement
    method validate, which returns a boolean.
    """

    def validate(self):
        """
        Return True if output is valid; False otherwise.
        """
        raise NotImplementedError("validate function needs to be implemented for validators")

