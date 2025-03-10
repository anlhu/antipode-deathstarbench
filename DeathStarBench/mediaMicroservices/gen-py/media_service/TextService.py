#
# Autogenerated by Thrift Compiler (0.13.0)
#
# DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
#
#  options string: py
#

from thrift.Thrift import TType, TMessageType, TFrozenDict, TException, TApplicationException
from thrift.protocol.TProtocol import TProtocolException
from thrift.TRecursive import fix_spec

import sys
import logging
from .ttypes import *
from thrift.Thrift import TProcessor
from thrift.transport import TTransport
all_structs = []


class Iface(object):
    def UploadText(self, req_id, text, carrier):
        """
        Parameters:
         - req_id
         - text
         - carrier

        """
        pass


class Client(Iface):
    def __init__(self, iprot, oprot=None):
        self._iprot = self._oprot = iprot
        if oprot is not None:
            self._oprot = oprot
        self._seqid = 0

    def UploadText(self, req_id, text, carrier):
        """
        Parameters:
         - req_id
         - text
         - carrier

        """
        self.send_UploadText(req_id, text, carrier)
        return self.recv_UploadText()

    def send_UploadText(self, req_id, text, carrier):
        self._oprot.writeMessageBegin('UploadText', TMessageType.CALL, self._seqid)
        args = UploadText_args()
        args.req_id = req_id
        args.text = text
        args.carrier = carrier
        args.write(self._oprot)
        self._oprot.writeMessageEnd()
        self._oprot.trans.flush()

    def recv_UploadText(self):
        iprot = self._iprot
        (fname, mtype, rseqid) = iprot.readMessageBegin()
        if mtype == TMessageType.EXCEPTION:
            x = TApplicationException()
            x.read(iprot)
            iprot.readMessageEnd()
            raise x
        result = UploadText_result()
        result.read(iprot)
        iprot.readMessageEnd()
        if result.success is not None:
            return result.success
        if result.se is not None:
            raise result.se
        raise TApplicationException(TApplicationException.MISSING_RESULT, "UploadText failed: unknown result")


class Processor(Iface, TProcessor):
    def __init__(self, handler):
        self._handler = handler
        self._processMap = {}
        self._processMap["UploadText"] = Processor.process_UploadText
        self._on_message_begin = None

    def on_message_begin(self, func):
        self._on_message_begin = func

    def process(self, iprot, oprot):
        (name, type, seqid) = iprot.readMessageBegin()
        if self._on_message_begin:
            self._on_message_begin(name, type, seqid)
        if name not in self._processMap:
            iprot.skip(TType.STRUCT)
            iprot.readMessageEnd()
            x = TApplicationException(TApplicationException.UNKNOWN_METHOD, 'Unknown function %s' % (name))
            oprot.writeMessageBegin(name, TMessageType.EXCEPTION, seqid)
            x.write(oprot)
            oprot.writeMessageEnd()
            oprot.trans.flush()
            return
        else:
            self._processMap[name](self, seqid, iprot, oprot)
        return True

    def process_UploadText(self, seqid, iprot, oprot):
        args = UploadText_args()
        args.read(iprot)
        iprot.readMessageEnd()
        result = UploadText_result()
        try:
            result.success = self._handler.UploadText(args.req_id, args.text, args.carrier)
            msg_type = TMessageType.REPLY
        except TTransport.TTransportException:
            raise
        except ServiceException as se:
            msg_type = TMessageType.REPLY
            result.se = se
        except TApplicationException as ex:
            logging.exception('TApplication exception in handler')
            msg_type = TMessageType.EXCEPTION
            result = ex
        except Exception:
            logging.exception('Unexpected exception in handler')
            msg_type = TMessageType.EXCEPTION
            result = TApplicationException(TApplicationException.INTERNAL_ERROR, 'Internal error')
        oprot.writeMessageBegin("UploadText", msg_type, seqid)
        result.write(oprot)
        oprot.writeMessageEnd()
        oprot.trans.flush()

# HELPER FUNCTIONS AND STRUCTURES


class UploadText_args(object):
    """
    Attributes:
     - req_id
     - text
     - carrier

    """


    def __init__(self, req_id=None, text=None, carrier=None,):
        self.req_id = req_id
        self.text = text
        self.carrier = carrier

    def read(self, iprot):
        if iprot._fast_decode is not None and isinstance(iprot.trans, TTransport.CReadableTransport) and self.thrift_spec is not None:
            iprot._fast_decode(self, iprot, [self.__class__, self.thrift_spec])
            return
        iprot.readStructBegin()
        while True:
            (fname, ftype, fid) = iprot.readFieldBegin()
            if ftype == TType.STOP:
                break
            if fid == 1:
                if ftype == TType.I64:
                    self.req_id = iprot.readI64()
                else:
                    iprot.skip(ftype)
            elif fid == 2:
                if ftype == TType.STRING:
                    self.text = iprot.readString().decode('utf-8') if sys.version_info[0] == 2 else iprot.readString()
                else:
                    iprot.skip(ftype)
            elif fid == 3:
                if ftype == TType.MAP:
                    self.carrier = {}
                    (_ktype84, _vtype85, _size83) = iprot.readMapBegin()
                    for _i87 in range(_size83):
                        _key88 = iprot.readString().decode('utf-8') if sys.version_info[0] == 2 else iprot.readString()
                        _val89 = iprot.readString().decode('utf-8') if sys.version_info[0] == 2 else iprot.readString()
                        self.carrier[_key88] = _val89
                    iprot.readMapEnd()
                else:
                    iprot.skip(ftype)
            else:
                iprot.skip(ftype)
            iprot.readFieldEnd()
        iprot.readStructEnd()

    def write(self, oprot):
        if oprot._fast_encode is not None and self.thrift_spec is not None:
            oprot.trans.write(oprot._fast_encode(self, [self.__class__, self.thrift_spec]))
            return
        oprot.writeStructBegin('UploadText_args')
        if self.req_id is not None:
            oprot.writeFieldBegin('req_id', TType.I64, 1)
            oprot.writeI64(self.req_id)
            oprot.writeFieldEnd()
        if self.text is not None:
            oprot.writeFieldBegin('text', TType.STRING, 2)
            oprot.writeString(self.text.encode('utf-8') if sys.version_info[0] == 2 else self.text)
            oprot.writeFieldEnd()
        if self.carrier is not None:
            oprot.writeFieldBegin('carrier', TType.MAP, 3)
            oprot.writeMapBegin(TType.STRING, TType.STRING, len(self.carrier))
            for kiter90, viter91 in self.carrier.items():
                oprot.writeString(kiter90.encode('utf-8') if sys.version_info[0] == 2 else kiter90)
                oprot.writeString(viter91.encode('utf-8') if sys.version_info[0] == 2 else viter91)
            oprot.writeMapEnd()
            oprot.writeFieldEnd()
        oprot.writeFieldStop()
        oprot.writeStructEnd()

    def validate(self):
        return

    def __repr__(self):
        L = ['%s=%r' % (key, value)
             for key, value in self.__dict__.items()]
        return '%s(%s)' % (self.__class__.__name__, ', '.join(L))

    def __eq__(self, other):
        return isinstance(other, self.__class__) and self.__dict__ == other.__dict__

    def __ne__(self, other):
        return not (self == other)
all_structs.append(UploadText_args)
UploadText_args.thrift_spec = (
    None,  # 0
    (1, TType.I64, 'req_id', None, None, ),  # 1
    (2, TType.STRING, 'text', 'UTF8', None, ),  # 2
    (3, TType.MAP, 'carrier', (TType.STRING, 'UTF8', TType.STRING, 'UTF8', False), None, ),  # 3
)


class UploadText_result(object):
    """
    Attributes:
     - success
     - se

    """


    def __init__(self, success=None, se=None,):
        self.success = success
        self.se = se

    def read(self, iprot):
        if iprot._fast_decode is not None and isinstance(iprot.trans, TTransport.CReadableTransport) and self.thrift_spec is not None:
            iprot._fast_decode(self, iprot, [self.__class__, self.thrift_spec])
            return
        iprot.readStructBegin()
        while True:
            (fname, ftype, fid) = iprot.readFieldBegin()
            if ftype == TType.STOP:
                break
            if fid == 0:
                if ftype == TType.STRUCT:
                    self.success = BaseRpcResponse()
                    self.success.read(iprot)
                else:
                    iprot.skip(ftype)
            elif fid == 1:
                if ftype == TType.STRUCT:
                    self.se = ServiceException()
                    self.se.read(iprot)
                else:
                    iprot.skip(ftype)
            else:
                iprot.skip(ftype)
            iprot.readFieldEnd()
        iprot.readStructEnd()

    def write(self, oprot):
        if oprot._fast_encode is not None and self.thrift_spec is not None:
            oprot.trans.write(oprot._fast_encode(self, [self.__class__, self.thrift_spec]))
            return
        oprot.writeStructBegin('UploadText_result')
        if self.success is not None:
            oprot.writeFieldBegin('success', TType.STRUCT, 0)
            self.success.write(oprot)
            oprot.writeFieldEnd()
        if self.se is not None:
            oprot.writeFieldBegin('se', TType.STRUCT, 1)
            self.se.write(oprot)
            oprot.writeFieldEnd()
        oprot.writeFieldStop()
        oprot.writeStructEnd()

    def validate(self):
        return

    def __repr__(self):
        L = ['%s=%r' % (key, value)
             for key, value in self.__dict__.items()]
        return '%s(%s)' % (self.__class__.__name__, ', '.join(L))

    def __eq__(self, other):
        return isinstance(other, self.__class__) and self.__dict__ == other.__dict__

    def __ne__(self, other):
        return not (self == other)
all_structs.append(UploadText_result)
UploadText_result.thrift_spec = (
    (0, TType.STRUCT, 'success', [BaseRpcResponse, None], None, ),  # 0
    (1, TType.STRUCT, 'se', [ServiceException, None], None, ),  # 1
)
fix_spec(all_structs)
del all_structs

