#!/usr/bin/env python
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
from ctypes import cdll
import ctypes
from abc import abstractmethod



class RPG_PORT(ctypes.Structure):
    _fields_ = [('port_id', ctypes.c_char_p)]

class NIFI_STRUCT(ctypes.Structure):
    _fields_ = [('instancePtr', ctypes.c_void_p),
                 ('port', RPG_PORT)]

class CFlow(ctypes.Structure):
    _fields_ = [('plan', ctypes.c_void_p)]

class CFlowFile(ctypes.Structure):
    _fields_ = [('size', ctypes.c_int),
                 ('in', ctypes.c_void_p),
                 ('contentLocation', ctypes.c_char_p),
                 ('attributes', ctypes.c_void_p),
                 ('ffp', ctypes.c_void_p)]

class CAttribute(ctypes.Structure):
    _fields_ = [('key', ctypes.c_char_p),
                ('value', ctypes.c_void_p),
                ('value_size', ctypes.c_size_t)]

class CProcessor(ctypes.Structure):
    _fields_ = [('processor_ptr', ctypes.c_void_p)]

class CProcessSession(ctypes.Structure):
    _fields_ = [('process_session', ctypes.c_void_p)]

class CProcessContext(ctypes.Structure):
    _fields_ = [('process_context', ctypes.c_void_p)]


CALLBACK = ctypes.CFUNCTYPE(None, ctypes.POINTER(CProcessSession), ctypes.POINTER(CProcessContext))

class Processor(object):
    def __init__(self, cprocessor, minifi):
        super(Processor, self).__init__()
        self._proc = cprocessor
        self._minifi = minifi

    def set_property(self, name, value):
        self._minifi.set_property( self._proc, name.encode("UTF-8"), value.encode("UTF-8"))

class PyProcessor(object):
    def __init__(self, minifi, flow):
        super(PyProcessor, self).__init__()
        self._minifi = minifi
        self._flow = flow

    def setBase(self, proc):
        self._proc = proc

    def get(self, session, context):
        ff = self._minifi.get(session, context)
        if ff:
            return FlowFile(self._minifi, ff)
        else:
            return None

    def transfer(self, session, ff, rel):
        self._minifi.transfer(session, self._flow, rel.encode("UTF-8"))

    @abstractmethod
    def _onTriggerCallback(self):
        pass

    def getTriggerCallback(self):
        if self._callback is None:
            print("creating ptr")
            self._callback = self._onTriggerCallback()
        return self._callback

    @abstractmethod
    def onSchedule(self):
        pass


class RPG(object):
    def __init__(self, nifi_struct):
        super(RPG, self).__init__()
        self._nifi = nifi_struct

    def get_instance(self):
        return self._nifi

class FlowFile(object):
    def __init__(self, minifi, ff):
        super(FlowFile, self).__init__()
        self._minifi = minifi
        self._ff = ff

    def get_attribute(self, name):
        attr = CAttribute(name.encode("UTF-8"), 0, 0)
        if self._minifi.get_attribute(self._ff, attr) != 0:
            return ""
        if attr.value_size > 0:
            return ctypes.cast(attr.value, ctypes.c_char_p).value.decode("ascii")
        return ""

    def add_attribute(self, name, value):
        vallen = len(value)
        ret = self._minifi.add_attribute(self._ff, name.encode("UTF-8"), value.encode("UTF-8"), vallen)
        return True if ret == 0 else False

    def update_attribute(self, name, value):
        vallen = len(value)
        self._minifi.update_attribute(self._ff, name.encode("UTF-8"), value.encode("UTF-8"), vallen)

    def get_instance(self):
        return self._ff



class MiNiFi(object):
    """ Proxy Connector """
    def __init__(self, dll_file, url, port):
        super(MiNiFi, self).__init__()
        self._minifi= cdll.LoadLibrary(dll_file)
        """ create instance """
        self._minifi.create_instance.argtypes = [ctypes.c_char_p , ctypes.POINTER(RPG_PORT)]
        self._minifi.create_instance.restype = ctypes.POINTER(NIFI_STRUCT)
        """ create new flow """
        self._minifi.create_new_flow.argtype = ctypes.POINTER(NIFI_STRUCT)
        self._minifi.create_new_flow.restype = ctypes.POINTER(CFlow)
        """ add processor """
        self._minifi.add_processor.argtypes = [ctypes.POINTER(CFlow) , ctypes.c_char_p ]
        self._minifi.add_processor.restype = ctypes.POINTER(CProcessor)
        """ set processor property"""
        self._minifi.set_property.argtypes = [ctypes.POINTER(CProcessor) , ctypes.c_char_p , ctypes.c_char_p ]
        self._minifi.set_property.restype = ctypes.c_int
        """ set instance property"""
        self._minifi.set_instance_property.argtypes = [ctypes.POINTER(NIFI_STRUCT) , ctypes.c_char_p , ctypes.c_char_p ]
        self._minifi.set_instance_property.restype = ctypes.c_int
        """ get next flow file """
        self._minifi.get_next_flow_file.argtypes = [ctypes.POINTER(NIFI_STRUCT) , ctypes.POINTER(CFlow) ]
        self._minifi.get_next_flow_file.restype = ctypes.POINTER(CFlowFile)
        """ transmit flow file """
        self._minifi.transmit_flowfile.argtypes = [ctypes.POINTER(CFlowFile) , ctypes.POINTER(NIFI_STRUCT) ]
        self._minifi.transmit_flowfile.restype = ctypes.c_int
        """ get ff """
        self._minifi.get.argtypes = [ctypes.POINTER(CProcessSession), ctypes.POINTER(CProcessContext) ]
        self._minifi.get.restype = ctypes.POINTER(CFlowFile)
        """ add python processor """
        self._minifi.add_python_processor.argtypes = [ctypes.POINTER(CFlow) , ctypes.c_void_p ]
        self._minifi.add_python_processor.restype = ctypes.POINTER(CProcessor)
        """ transfer ff """
        self._minifi.transfer.argtypes = [ctypes.POINTER(CProcessSession), ctypes.POINTER(CFlow) , ctypes.c_char_p ]
        self._minifi.transfer.restype = ctypes.c_int
        """ add attribute to ff """
        self._minifi.add_attribute.argtypes = [ctypes.POINTER(CFlowFile), ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int ]
        self._minifi.add_attribute.restype = ctypes.c_int

        """ update (overwrite) attribute to ff """
        self._minifi.update_attribute.argtypes = [ctypes.POINTER(CFlowFile), ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int ]
        self._minifi.update_attribute.restype = None

        """ get attribute of ff """
        self._minifi.get_attribute.argtypes = [ctypes.POINTER(CFlowFile), ctypes.POINTER(CAttribute) ]
        self._minifi.get_attribute.restype = ctypes.c_int

        self._minifi.init_api.argtype = ctypes.c_char_p
        self._minifi.init_api.restype = ctypes.c_int
        self._minifi.init_api(dll_file.encode("UTF-8"))

        self._instance = self.__open_rpg(url,port)
        self._flow = self._minifi.create_new_flow( self._instance.get_instance() )
        self._minifi.enable_logging()



    def __open_rpg(self, url, port):
        rpgPort = (RPG_PORT)(port)
        rpg = self._minifi.create_instance(url, rpgPort)
        ret = RPG(rpg)
        return ret

    def get_c_lib(self):
        return self._minifi

    def set_property(self, name, value):
        self._minifi.set_instance_property(self._instance.get_instance(), name.encode("UTF-8"), value.encode("UTF-8"))


    def add_processor(self, processor):
        proc = self._minifi.add_processor(self._flow, processor.get_name().encode("UTF-8"))
        return Processor(proc,self._minifi)

    def create_python_processor(self, module, processor):
        m =  getattr(module, processor)(self._minifi, self._flow)
        proc = self._minifi.add_python_processor(self._flow, m.getTriggerCallback())
        m.setBase(proc)
        return m

    def get_next_flowfile(self):
        ff = self._minifi.get_next_flow_file(self._instance.get_instance(), self._flow)
        return FlowFile(self._minifi, ff)

    def transmit_flowfile(self, ff):
        if ff.get_instance():
            self._minifi.transmit_flowfile(ff.get_instance(),self._instance.get_instance())

class GetFile(object):
    def __init__(self):
        super(GetFile, self).__init__()

    def get_name(self):
        return "GetFile"
