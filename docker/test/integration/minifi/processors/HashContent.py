from ..core.Processor import Processor

class HashContent(Processor):
	def __init__(self, schedule={"scheduling period": "2 sec"}):
		super(HashContent, self).__init__("HashContent",
			properties={"Hash Attribute": "hash"},
			schedule=schedule,
			auto_terminate=["success", "failure"])
