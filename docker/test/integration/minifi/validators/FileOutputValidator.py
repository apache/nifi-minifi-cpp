from .OutputValidator import OutputValidator

class FileOutputValidator(OutputValidator):
    def set_output_dir(self, output_dir):
        self.output_dir = output_dir

    def validate(self, dir=''):
        pass
