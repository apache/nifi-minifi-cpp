from .Connectable import Connectable


class Processor(Connectable):
    def __init__(self,
                 clazz,
                 properties=None,
                 schedule=None,
                 name=None,
                 controller_services=None,
                 auto_terminate=None):

        super(Processor, self).__init__(name=name,
                                        auto_terminate=auto_terminate)

        if controller_services is None:
            controller_services = []

        if schedule is None:
            schedule = {}

        if properties is None:
            properties = {}

        if name is None:
            pass

        self.clazz = clazz
        self.properties = properties
        self.controller_services = controller_services

        self.schedule = {
            'scheduling strategy': 'TIMER_DRIVEN',
            'scheduling period': '1 sec',
            'penalization period': '30 sec',
            'yield period': '1 sec',
            'run duration nanos': 0
        }
        self.schedule.update(schedule)

    def set_property(self, key, value):
        if value.isdigit():
            self.properties[key] = int(value)
        else:
            self.properties[key] = value

    def unset_property(self, key):
        self.properties.pop(key, None)

    def set_scheduling_strategy(self, value):
        self.schedule["scheduling strategy"] = value

    def set_scheduling_period(self, value):
        self.schedule["scheduling period"] = value

    def nifi_property_key(self, key):
        """
        Returns the Apache NiFi-equivalent property key for the given key. This is often, but not always, the same as
        the internal key.
        """
        return key
