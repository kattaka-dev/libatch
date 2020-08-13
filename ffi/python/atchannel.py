from ctypes import c_void_p, c_bool, c_char_p, c_int, c_uint, c_longlong, \
    byref, POINTER, Structure, CFUNCTYPE, CDLL
from typing import Tuple, List, TextIO
from enum import IntEnum


class LibATLine(Structure):
    pass


LibATLine._fields_ = [
    ("next", POINTER(LibATLine)),
    ("line", c_char_p)
]


class LibATResponse(Structure):
    _fields_ = [
        ("success", c_bool),
        ("finalResponse", c_char_p),
        ("intermediates", POINTER(LibATLine))
    ]


class LibATChannel(Structure):
    pass


LibATUnsolHandler = CFUNCTYPE(None, POINTER(LibATChannel), c_char_p)
LibATUnsolSmsHandler = CFUNCTYPE(
    None, POINTER(LibATChannel), c_char_p, c_char_p)
LibATOnTimeoutHandler = CFUNCTYPE(None, POINTER(LibATChannel))
LibATOnCloseHandler = CFUNCTYPE(None, POINTER(LibATChannel))
LibATLog = CFUNCTYPE(None, POINTER(LibATChannel), c_int, c_char_p)


class LibATChannelImpl(Structure):
    pass


LibATChannel._fields_ = [
    ("path", c_char_p),
    ("bitrate", c_int),
    ("lflag", c_uint),
    ("fd", c_int),
    ("unsolHandler", LibATUnsolHandler),
    ("unsolSmshandler", LibATUnsolSmsHandler),
    ("onTimeoutHandler", LibATOnTimeoutHandler),
    ("onCloseHanlder", LibATOnCloseHandler),
    ("log", LibATLog),
    ("logLevel", c_int),
    ("param", c_void_p),
    ("impl", POINTER(LibATChannelImpl))
]


class LibATChannelException(Exception):
    pass


class GenericException(LibATChannelException):
    pass


class CommandPendingException(LibATChannelException):
    pass


class ChannelClosedException(LibATChannelException):
    pass


class TimeoutException(LibATChannelException):
    pass


class InvalidThreadException(LibATChannelException):
    pass


class InvalidResponseException(LibATChannelException):
    pass


class InvalidArgumentException(LibATChannelException):
    pass


class InvalidOperationException(LibATChannelException):
    pass


class LibATReturn(IntEnum):
    AT_SUCCESS =                  0
    AT_ERROR_GENERIC =           -1
    AT_ERROR_COMMAND_PENDING =   -2
    AT_ERROR_CHANNEL_CLOSED =    -3
    AT_ERROR_TIMEOUT =           -4
    AT_ERROR_INVALID_THREAD =    -5
    AT_ERROR_INVALID_RESPONSE =  -6
    AT_ERROR_INVALID_ARGUMENT =  -7
    AT_ERROR_INVALID_OPERATION = -8

    def is_success(self) -> bool:
        return 0 <= self

    def is_error(self) -> bool:
        return not self.is_success()

    def to_exception(self) -> LibATChannelException:
        return {
            LibATReturn.AT_SUCCESS: None,
            LibATReturn.AT_ERROR_GENERIC: GenericException,
            LibATReturn.AT_ERROR_COMMAND_PENDING: CommandPendingException,
            LibATReturn.AT_ERROR_CHANNEL_CLOSED: ChannelClosedException,
            LibATReturn.AT_ERROR_TIMEOUT: TimeoutException,
            LibATReturn.AT_ERROR_INVALID_THREAD: InvalidThreadException,
            LibATReturn.AT_ERROR_INVALID_RESPONSE: InvalidResponseException,
            LibATReturn.AT_ERROR_INVALID_ARGUMENT: InvalidArgumentException,
            LibATReturn.AT_ERROR_INVALID_OPERATION: InvalidOperationException
        }[self]

    def check_and_raise(self) -> None:
        if self.is_error():
            raise self.to_exception()


class LibAT_CME_Error(IntEnum):
    CME_ERROR_NON_CME =    -1
    CME_SUCCESS =           0
    CME_SIM_NOT_INSERTED = 10


libatch = CDLL("libatch.so")

lib_at_open = libatch.at_open
lib_at_open.argtypes = [POINTER(LibATChannel)]
lib_at_open.restype = LibATReturn

lib_at_attach = libatch.at_attach
lib_at_attach.argtypes = [POINTER(LibATChannel)]
lib_at_attach.restype = LibATReturn

lib_at_detach = libatch.at_detach
lib_at_detach.argtypes = [POINTER(LibATChannel)]
lib_at_detach.restype = LibATReturn

lib_at_close = libatch.at_close
lib_at_close.argtypes = [POINTER(LibATChannel)]
lib_at_close.restype = LibATReturn

lib_at_send_command = libatch.at_send_command
lib_at_send_command.argtypes = [
    POINTER(LibATChannel),
    c_char_p,
    POINTER(POINTER(LibATResponse))
]
lib_at_send_command.restype = LibATReturn

lib_at_send_command_timeout = libatch.at_send_command_timeout
lib_at_send_command_timeout.argtypes = [
    POINTER(LibATChannel),
    c_char_p,
    c_longlong,
    POINTER(POINTER(LibATResponse))
]
lib_at_send_command_timeout.restype = LibATReturn

lib_at_send_command_singleline = libatch.at_send_command_singleline
lib_at_send_command_singleline.argtypes = [
    POINTER(LibATChannel),
    c_char_p,
    c_char_p,
    POINTER(POINTER(LibATResponse))
]
lib_at_send_command_singleline.restype = LibATReturn

lib_at_send_command_singleline_timeout = \
    libatch.at_send_command_singleline_timeout
lib_at_send_command_singleline_timeout.argtypes = [
    POINTER(LibATChannel),
    c_char_p,
    c_char_p,
    c_longlong,
    POINTER(POINTER(LibATResponse))
]
lib_at_send_command_singleline_timeout.restype = LibATReturn

lib_at_send_command_multiline = libatch.at_send_command_multiline
lib_at_send_command_multiline.argtypes = [
    POINTER(LibATChannel),
    c_char_p,
    c_char_p,
    POINTER(POINTER(LibATResponse))
]
lib_at_send_command_multiline.restype = LibATReturn

lib_at_send_command_multiline_timeout = \
    libatch.at_send_command_multiline_timeout
lib_at_send_command_multiline_timeout.argtypes = [
    POINTER(LibATChannel),
    c_char_p,
    c_char_p,
    c_longlong,
    POINTER(POINTER(LibATResponse))
]
lib_at_send_command_multiline_timeout.restype = LibATReturn

lib_at_send_command_numeric = libatch.at_send_command_numeric
lib_at_send_command_numeric.argtypes = [
    POINTER(LibATChannel),
    c_char_p,
    POINTER(POINTER(LibATResponse))
]
lib_at_send_command_numeric.restype = LibATReturn

lib_at_send_command_numeric_timeout = libatch.at_send_command_numeric_timeout
lib_at_send_command_numeric_timeout.argtypes = [
    POINTER(LibATChannel),
    c_char_p,
    c_longlong,
    POINTER(POINTER(LibATResponse))
]
lib_at_send_command_numeric_timeout.restype = LibATReturn

lib_at_send_command_sms = libatch.at_send_command_sms
lib_at_send_command_sms.argtypes = [
    POINTER(LibATChannel),
    c_char_p,
    c_char_p,
    POINTER(POINTER(LibATResponse))
]
lib_at_send_command_sms.restype = LibATReturn

lib_at_send_command_sms_timeout = libatch.at_send_command_sms_timeout
lib_at_send_command_sms_timeout.argtypes = [
    POINTER(LibATChannel),
    c_char_p,
    c_char_p,
    c_longlong,
    POINTER(POINTER(LibATResponse))
]
lib_at_send_command_sms_timeout.restype = LibATReturn

lib_at_handshake = libatch.at_handshake
lib_at_handshake.argtypes = [
    POINTER(LibATChannel),
    c_char_p,
    c_int,
    c_longlong
]
lib_at_handshake.restype = LibATReturn

lib_at_response_free = libatch.at_response_free
lib_at_response_free.argtypes = [
    POINTER(LibATResponse)
]
lib_at_response_free.restype = LibATReturn

lib_at_get_cme_error = libatch.at_get_cme_error
lib_at_get_cme_error.argtypes = [
    POINTER(LibATResponse)
]
lib_at_get_cme_error.restype = LibAT_CME_Error


class ATChannel:
    DEFAULT_BITRATE = 0
    DEFAULT_LFLAG = 0
    DEFAULT_FD = 0
    DEFAULT_LOGLEVEL = 7
    DEFAULT_PARAM = 0

    def __init__(
            self, path: str = None, bitrate: int = None, lflag: int = None,
            fd: int = None, loglevel: int = None, logfile: TextIO = None) \
            -> None:
        if (path is None) and (fd is None):
            raise ValueError("path or file descriptor should not be None.")
        if (path is not None) and (bitrate is None):
            raise ValueError("bitrate should be specified.")
        if (fd is not None) and (fd < 0):
            raise ValueError("fd should be grater than 0.")
        if logfile:
            if not logfile.mode.count('w'):
                raise RuntimeError("log file should be writable.")
            if logfile.closed:
                raise RuntimeError("log file should be opened.")
        self.path = path
        self.bitrate = bitrate
        self.fd = fd
        self.logFile = logfile
        self.atch = LibATChannel(
            # path
            bytes(path, 'utf-8'),
            # bitrate
            bitrate if bitrate else self.DEFAULT_BITRATE,
            # lflag
            lflag if lflag else self.DEFAULT_LFLAG,
            # fd
            fd if fd else self.DEFAULT_FD,
            # unsolHandler
            self._callback_unsolHandler(),
            # unsolSmsHandler
            self._callback_unsolSmsHandler(),
            # onTimeoutHandler
            self._callback_onTimeoutHandler(),
            # onCloseHandler
            self._callback_onCloseHandler(),
            # log
            self._callback_log(),
            # logLevel
            loglevel if loglevel else self.DEFAULT_LOGLEVEL,
            # param
            self.DEFAULT_PARAM,
            # impl
            None
        )

    def open(self) -> None:
        if (self.atch.path is not None) and (self.atch.bitrate is not None):
            pass
        elif (self.atch.fd is not None):
            pass
        else:
            raise RuntimeError("path or fd should not be None.")
        ret = lib_at_open(self.atch)
        ret.check_and_raise()

    def attach(self) -> None:
        ret = lib_at_attach(self.atch)
        ret.check_and_raise()

    def detach(self) -> None:
        ret = lib_at_detach(self.atch)
        ret.check_and_raise()

    def close(self) -> None:
        ret = lib_at_close(self.atch)
        ret.check_and_raise()

    def send_command(self, command: str) -> Tuple[bool, str]:
        return self.send_command_timeout(command, 0)

    def send_command_timeout(
            self, command: str, timeout: int) -> Tuple[bool, str]:
        patres = POINTER(LibATResponse)()
        ret = lib_at_send_command_timeout(
            self.atch, bytes(command, 'utf-8'), timeout, byref(patres))
        ret.check_and_raise()
        success = None
        finalResponse = None
        if patres is None:
            print("send_command_timeout: response is null.")
        else:
            success = patres.contents.success
            finalResponse = patres.contents.finalResponse.decode('utf-8')
            lib_at_response_free(patres)
        return (success, finalResponse)

    def send_command_singleline(
            self, command: str, responsePrefix: str) -> Tuple[bool, str, str]:
        return self.send_command_singleline_timeout(command, responsePrefix, 0)

    def send_command_singleline_timeout(
            self, command: str, responsePrefix: str, timeout: int) \
            -> Tuple[bool, str, str]:
        patres = POINTER(LibATResponse)()
        ret = lib_at_send_command_singleline_timeout(
            self.atch, bytes(command, 'utf-8'), bytes(responsePrefix, 'utf-8'),
            timeout, byref(patres))
        ret.check_and_raise()
        success = None
        finalResponse = None
        line = None
        if patres is None:
            print("send_command_singleline_timeout: response is null.")
        else:
            success = patres.contents.success
            finalResponse = patres.contents.finalResponse.decode('utf-8')
            if success:
                line = patres.contents.intermediates.contents.line.decode(
                    'utf-8')
            lib_at_response_free(patres)
        return (success, line, finalResponse)

    def send_command_multiline(
            self, command: str, responsePrefix: str) \
            -> Tuple[bool, List[str], str]:
        return self.send_command_multiline_timeout(command, responsePrefix, 0)

    def send_command_multiline_timeout(
            self, command: str, responsePrefix: str, timeout: int) \
            -> Tuple[bool, str, List[str]]:
        patres = POINTER(LibATResponse)()
        ret = lib_at_send_command_multiline_timeout(
            self.atch, bytes(command, 'utf-8'), bytes(responsePrefix, 'utf-8'),
            timeout, byref(patres))
        ret.check_and_raise()
        success = None
        finalResponse = None
        lines = None
        if patres is None:
            print("send_command_multiline_timeout: response is null.")
        else:
            success = patres.contents.success
            finalResponse = patres.contents.finalResponse.decode('utf-8')
            if success:
                lines = []
                patlines = patres.contents.intermediates
                while patlines:
                    lines.append(patlines.contents.line.decode('utf-8'))
                    patlines = patlines.contents.next
            lib_at_response_free(patres)
        return (success, lines, finalResponse)

    def send_command_numeric(
            self, command: str) -> Tuple[bool, str, str]:
        return self.send_command_numeric_timeout(command, 0)

    def send_command_numeric_timeout(
            self, command: str, timeout: int) -> Tuple[bool, str, str]:
        patres = POINTER(LibATResponse)()
        ret = lib_at_send_command_numeric_timeout(
            self.atch, bytes(command, 'utf-8'), timeout, byref(patres))
        ret.check_and_raise()
        success = None
        finalResponse = None
        line = None
        if patres is None:
            print("send_command_numeric: response is null.")
        else:
            success = patres.contents.success
            finalResponse = patres.contents.finalResponse.decode('utf-8')
            if success:
                line = patres.contents.intermediates.contents.line.decode(
                    'utf-8')
            lib_at_response_free(patres)
        return (success, line, finalResponse)

    def send_command_sms(
            self, command: str, pdu: str, responsePrefix: str) \
            -> Tuple[bool, str, str]:
        return self.send_command_sms_timeout(command, pdu, responsePrefix, 0)

    def send_command_sms_timeout(
            self, command: str, pdu: str, responsePrefix: str,
            timeout: int) -> Tuple[bool, str, str]:
        patres = POINTER(LibATResponse)()
        ret = lib_at_send_command_sms_timeout(
            self.atch, bytes(command, 'utf-8'), bytes(pdu, 'utf-8'),
            bytes(responsePrefix, 'utf-8'), timeout, byref(patres))
        ret.check_and_raise()
        success = None
        finalResponse = None
        line = None
        if patres is None:
            print("send_command_sms_timeout: response is null.")
        else:
            success = patres.contents.success
            finalResponse = patres.contents.finalResponse.decode('utf-8')
            if success:
                line = patres.contents.intermediates.contents.line.decode(
                    'utf-8')
            lib_at_response_free(patres)
        return (success, line, finalResponse)

    def handshake(
            self, command: str = None, retryCount: int = 0,
            timeout: int = 0) -> LibATReturn:
        ret = lib_at_handshake(
            self.atch, bytes(command, 'utf-8') if command else None,
            retryCount, timeout)
        ret.check_and_raise()

    def unsol_handler(self, unsol: str) -> None:
        if self.logFile:
            self.logFile.write(
                f"unsol_handler: file = {self.path}, fileno = {self.fd}: "\
                    "unsol = {unsol}.\n")

    def unsol_sms_handler(self, unsol: str, sms_pdu: str) -> None:
        if self.logFile:
            self.logFile.write(
                f"unsol_sms_handler: file = {self.path}, fileno = {self.fd}: "\
                    "unsol = {unsol}, sms_pdu = {sms_pdu}.\n")

    def on_timeout_handler(self) -> None:
        if self.logFile:
            self.logFile.write(
                f"on_timeout_handler: file = {self.path}, "\
                    "fileno = {self.fd}.\n")

    def on_close_handler(self) -> None:
        if self.logFile:
            self.logFile.write(
                f"on_close_handler: file = {self.path}, fileno = {self.fd}.\n")

    def log(self, level: int, message: str) -> None:
        if self.logFile:
            self.logFile.write(
                f"log: file = {self.path}, fileno = {self.fd}: "\
                    "level = {level}, message = {message}\n")

    def __enter__(self):
        if self.atch.path is not None:
            self.open()
        elif self.atch.fd is not None:
            self.attach()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.atch.path is not None:
            self.close
        else:
            self.detach()

    @property
    def fileno(self) -> int:
        return self.atch.fd

    def _callback_unsolHandler(self) -> LibATUnsolHandler:
        def unsolHandler(atch: POINTER(LibATChannel), s: c_char_p) -> None:
            self.unsol_handler(s.decode('utf-8'))
        return LibATUnsolHandler(unsolHandler)

    def _callback_unsolSmsHandler(self) -> LibATUnsolSmsHandler:
        def unsolSmsHandler(
                atch: POINTER(LibATChannel), s: c_char_p,
                sms_pdu: c_char_p) -> None:
            self.unsol_sms_handler(s.decode('utf-8'), sms_pdu.decode('utf-8'))
        return LibATUnsolSmsHandler(unsolSmsHandler)

    def _callback_onTimeoutHandler(self) -> LibATOnTimeoutHandler:
        def onTimeoutHandler(atch: POINTER(LibATChannel)) -> None:
            self.on_timeout_handler()
        return LibATOnTimeoutHandler(onTimeoutHandler)

    def _callback_onCloseHandler(self) -> LibATOnCloseHandler:
        def onCloseHandler(atch: POINTER(LibATChannel)) -> None:
            self.on_close_handler()
        return LibATOnCloseHandler(onCloseHandler)

    def _callback_log(self) -> LibATLog:
        def log(atch: POINTER(LibATChannel), level: c_int, message: c_char_p) \
                -> None:
            self.log(level, message.decode('utf-8'))
        return LibATLog(log)
