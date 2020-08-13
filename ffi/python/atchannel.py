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
