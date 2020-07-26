/* //device/system/reference-ril/atchannel.h
**
** Copyright 2006, The Android Open Source Project
** Copyright 2020, The libatch Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef ATCHANNEL_H
#define ATCHANNEL_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <termios.h>
#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>

/* define AT_DEBUG to send AT traffic to /tmp/radio-at.log" */
#define AT_DEBUG  0

#if AT_DEBUG
extern void  AT_DUMP(ATChannel* atch, const char* prefix, const char*  buff, int  len);
#else
#define  AT_DUMP(atch, prefix,buff,len)  do{}while(0)
#endif

typedef enum {
    AT_SUCCESS =                 0,
    AT_ERROR_GENERIC =          -1,
    AT_ERROR_COMMAND_PENDING =  -2,
    AT_ERROR_CHANNEL_CLOSED =   -3,
    AT_ERROR_TIMEOUT =          -4,
    AT_ERROR_INVALID_THREAD =   -5, /* AT commands may not be issued from
                                       reader thread (or unsolicited response
                                       callback */
    AT_ERROR_INVALID_RESPONSE = -6, /* eg an at_send_command_singleline that
                                       did not get back an intermediate
                                       response */
} ATReturn;


/** a singly-lined list of intermediate responses */
typedef struct ATLine  {
    struct ATLine *p_next;
    char *line;
} ATLine;

/** Free this with at_response_free() */
typedef struct {
    bool success;              /* true if final response indicates
                                    success (eg "OK") */
    char *finalResponse;      /* eg OK, ERROR */
    ATLine  *p_intermediates; /* any intermediate responses */
} ATResponse;

typedef struct ATChannel ATChannel;

/**
 * a user-provided unsolicited response handler function
 * this will be called from the reader thread, so do not block
 * "s" is the line, and "sms_pdu" is either NULL or the PDU response
 * for multi-line TS 27.005 SMS PDU responses (eg +CMT:)
 */
typedef void (*ATUnsolHandler)(ATChannel* atch, const char *s, const char *sms_pdu);

/* This callback is invoked on the command thread.
   You should reset or handshake here to avoid getting out of sync */
typedef void (*ATOnTimeoutHandler)(ATChannel* atch);

/* This callback is invoked on the reader thread (like ATUnsolHandler)
   when the input stream closes before you call at_close
   (not when you call at_close())
   You should still call at_close()
   It may also be invoked immediately from the current thread if the read
   channel is already closed */
typedef void (*ATOnCloseHandler)(ATChannel* atch);

typedef void (*ATLog)(ATChannel* atch, int level, const char* message);

typedef struct ATChannelImpl ATChannelImpl;

struct ATChannel {
    const char* path;
    int bitrate;
    tcflag_t lflag;
    int fd;
    ATUnsolHandler unsolHandler;
    ATOnTimeoutHandler onTimeoutHandler;
    ATOnCloseHandler onCloseHandler;
    ATLog log;
    int logLevel;
    uintptr_t param;
    ATChannelImpl* impl;
};

ATReturn at_open(ATChannel* atch);
ATReturn at_attach(ATChannel* atch);
void at_detach(ATChannel* atch);

ATReturn at_send_command_singleline (ATChannel* atch,
                                const char *command,
                                const char *responsePrefix,
                                ATResponse **pp_outResponse);

ATReturn at_send_command_numeric (ATChannel* atch,
                                const char *command,
                                ATResponse **pp_outResponse);

ATReturn at_send_command_multiline (ATChannel* atch,
                                const char *command,
                                const char *responsePrefix,
                                ATResponse **pp_outResponse);


ATReturn at_handshake(ATChannel* atch);

ATReturn at_send_command (ATChannel* atch, const char *command, ATResponse **pp_outResponse);

ATReturn at_send_command_sms (ATChannel* atch, const char *command, const char *pdu,
                            const char *responsePrefix,
                            ATResponse **pp_outResponse);

void at_response_free(ATResponse *p_response);

typedef enum {
    CME_ERROR_NON_CME = -1,
    CME_SUCCESS = 0,
    CME_SIM_NOT_INSERTED = 10
} AT_CME_Error;

AT_CME_Error at_get_cme_error(const ATResponse *p_response);

#ifdef __cplusplus
}
#endif

#endif /*ATCHANNEL_H*/
