import uuid
from copy import copy

class Connectable(object):
    def __init__(self,
                 name=None,
                 auto_terminate=None):

        self.uuid = uuid.uuid4()

        if name is None:
            self.name = str(self.uuid)
        else:
            self.name = name

        if auto_terminate is None:
            self.auto_terminate = []
        else:
            self.auto_terminate = auto_terminate

        self.connections = {}
        self.out_proc = self

        self.drop_empty_flowfiles = False

    def connect(self, connections):
        for rel in connections:

            # Ensure that rel is not auto-terminated
            if rel in self.auto_terminate:
                del self.auto_terminate[self.auto_terminate.index(rel)]

            # Add to set of output connections for this rel
            if rel not in self.connections:
                self.connections[rel] = []
            self.connections[rel].append(connections[rel])

        return self

    def set_name(self, name):
        self.name = name

    def __rshift__(self, other):
        """
        Right shift operator to support flow DSL, for example:

            GetFile('/input') >> LogAttribute() >> PutFile('/output')

        """

        connected = copy(self)
        connected.connections = copy(self.connections)

        if self.out_proc is self:
            connected.out_proc = connected
        else:
            connected.out_proc = copy(connected.out_proc)

        if isinstance(other, tuple):
            # if isinstance(other[0], tuple):
            #     for rel_tuple in other:
            #         rel = {rel_tuple[0]: rel_tuple[1]}
            #         connected.out_proc.connect(rel)
            # else:
            #     rel = {other[0]: other[1]}
            #     connected.out_proc.connect(rel)
            rel = {other[0]: other[1]}
            connected.out_proc.connect(rel)
        else:
            connected.out_proc.connect({'success': other})
            connected.out_proc = other

        return connected

    def __invert__(self):
        """
        Invert operation to set empty file filtering on incoming connections
        GetFile('/input') >> ~LogAttribute()
        """
        self.drop_empty_flowfiles = True

        return self
