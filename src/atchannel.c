/* //device/system/reference-ril/atchannel.c
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

#define _POSIX_C_SOURCE (200809L)
#include <features.h>
#include "atchannel.h"
#include "at_tok.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#define LOG_NDEBUG 0
#define LOG_TAG "AT"

#include "misc.h"


#define NUM_ELEMS(x) (sizeof(x)/sizeof((x)[0]))
#define RLOGD(atch, ...) outputLog(atch, LOG_DEBUG, __VA_ARGS__)
#define RLOGE(atch, ...) outputLog(atch, LOG_ERR, __VA_ARGS__)

#if AT_DEBUG
void  AT_DUMP(ATChannel* atch, const char*  prefix, const char*  buff, int  len)
{
    if (len < 0)
        len = strlen(buff);
    RLOGD(atch, "%.*s", len, buff);
}
#endif

typedef enum {
    NO_RESULT,   /* no intermediate response expected */
    NUMERIC,     /* a single intermediate response starting with a 0-9 */
    SINGLELINE,  /* a single intermediate response starting with a prefix */
    MULTILINE    /* multiple line intermediate response
                    starting with a prefix */
} ATCommandType;

#define MAX_AT_RESPONSE ((size_t)(8 * 1024))
#define HANDSHAKE_RETRY_COUNT (8)
#define HANDSHAKE_TIMEOUT_MSEC (250)

struct ATChannelImpl {
    pthread_t tid_reader;

/* for input buffering */
    char ATBuffer[MAX_AT_RESPONSE+1];
    char *ATBufferCur;

/*
 * for current pending command
 * these are protected by commandmutex
 */
    pthread_mutex_t commandmutex;
    pthread_cond_t commandcond;

    ATCommandType type;
    const char *responsePrefix;
    const char *smsPDU;
    ATResponse *p_response;

    int readerClosed;
};

static void onReaderClosed(ATChannel* atch);
static int writeCtrlZ (ATChannel* atch, const char *s);
static int writeline (ATChannel* atch, const char *s);
static void outputLog(ATChannel* atch, int level, const char* format, ...);

#define NS_PER_S (1000000000)
static void setTimespecRelative(struct timespec *p_ts, long long msec)
{
    struct timeval tv;

    gettimeofday(&tv, (struct timezone *) NULL);

    p_ts->tv_sec = tv.tv_sec + (msec / 1000);
    p_ts->tv_nsec = (tv.tv_usec + (msec % 1000) * 1000L ) * 1000L;
    /* assuming tv.tv_usec < 10^6 */
    if (p_ts->tv_nsec >= NS_PER_S) {
        p_ts->tv_sec++;
        p_ts->tv_nsec -= NS_PER_S;
    }
}

static void sleepMsec(long long msec)
{
    struct timespec ts;
    int err;

    ts.tv_sec = (msec / 1000);
    ts.tv_nsec = (msec % 1000) * 1000 * 1000;

    do {
        err = nanosleep (&ts, &ts);
    } while (err < 0 && errno == EINTR);
}



/** add an intermediate response to p_response*/
static void addIntermediate(ATChannel* atch, const char *line)
{
    ATLine *p_new;

    p_new = (ATLine  *) malloc(sizeof(ATLine));

    p_new->line = strdup(line);

    /* note: this adds to the head of the list, so the list
       will be in reverse order of lines received. the order is flipped
       again before passing on to the command issuer */
    p_new->p_next = atch->impl->p_response->p_intermediates;
    atch->impl->p_response->p_intermediates = p_new;
}


/**
 * returns 1 if line is a final response indicating error
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static const char * s_finalResponsesError[] = {
    "ERROR",
    "+CMS ERROR:",
    "+CME ERROR:",
    "NO CARRIER", /* sometimes! */
    "NO ANSWER",
    "NO DIALTONE",
};
static bool isFinalResponseError(const char *line)
{
    size_t i;

    for (i = 0 ; i < NUM_ELEMS(s_finalResponsesError) ; i++) {
        if (strStartsWith(line, s_finalResponsesError[i])) {
            return true;
        }
    }

    return false;
}

/**
 * returns 1 if line is a final response indicating success
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static const char * s_finalResponsesSuccess[] = {
    "OK",
    "CONNECT"       /* some stacks start up data on another channel */
};
static bool isFinalResponseSuccess(const char *line)
{
    size_t i;

    for (i = 0 ; i < NUM_ELEMS(s_finalResponsesSuccess) ; i++) {
        if (strStartsWith(line, s_finalResponsesSuccess[i])) {
            return true;
        }
    }

    return false;
}

#if 0
/**
 * returns 1 if line is a final response, either  error or success
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static bool isFinalResponse(const char *line)
{
    return isFinalResponseSuccess(line) || isFinalResponseError(line);
}
#endif  /* 0 */

/**
 * returns 1 if line is the first line in (what will be) a two-line
 * SMS unsolicited response
 */
static const char * s_smsUnsoliciteds[] = {
    "+CMT:",
    "+CDS:",
    "+CBM:"
};
static int isSMSUnsolicited(const char *line)
{
    size_t i;

    for (i = 0 ; i < NUM_ELEMS(s_smsUnsoliciteds) ; i++) {
        if (strStartsWith(line, s_smsUnsoliciteds[i])) {
            return 1;
        }
    }

    return 0;
}


/** assumes commandmutex is held */
static void handleFinalResponse(ATChannel* atch, const char *line)
{
    atch->impl->p_response->finalResponse = strdup(line);

    pthread_cond_signal(&atch->impl->commandcond);
}

static void handleUnsolicited(ATChannel* atch, const char *line)
{
    if (atch->unsolHandler != NULL) {
        atch->unsolHandler(atch, line, NULL);
    }
}

static void processLine(ATChannel* atch, const char *line)
{
    pthread_mutex_lock(&atch->impl->commandmutex);

    if (atch->impl->p_response == NULL) {
        /* no command pending */
        handleUnsolicited(atch, line);
    } else if (isFinalResponseSuccess(line)) {
        atch->impl->p_response->success = true;
        handleFinalResponse(atch, line);
    } else if (isFinalResponseError(line)) {
        atch->impl->p_response->success = false;
        handleFinalResponse(atch, line);
    } else if (atch->impl->smsPDU != NULL && 0 == strcmp(line, "> ")) {
        // See eg. TS 27.005 4.3
        // Commands like AT+CMGS have a "> " prompt
        writeCtrlZ(atch, atch->impl->smsPDU);
        atch->impl->smsPDU = NULL;
    } else switch (atch->impl->type) {
        case NO_RESULT:
            handleUnsolicited(atch, line);
            break;
        case NUMERIC:
            if (atch->impl->p_response->p_intermediates == NULL
                && isdigit(line[0])
            ) {
                addIntermediate(atch, line);
            } else {
                /* either we already have an intermediate response or
                   the line doesn't begin with a digit */
                handleUnsolicited(atch, line);
            }
            break;
        case SINGLELINE:
            if (atch->impl->p_response->p_intermediates == NULL
                && strStartsWith (line, atch->impl->responsePrefix)
            ) {
                addIntermediate(atch, line);
            } else {
                /* we already have an intermediate response */
                handleUnsolicited(atch, line);
            }
            break;
        case MULTILINE:
            if (strStartsWith (line, atch->impl->responsePrefix)) {
                addIntermediate(atch, line);
            } else {
                handleUnsolicited(atch, line);
            }
        break;

        default: /* this should never be reached */
            RLOGE(atch, "Unsupported AT command type %d.", atch->impl->type);
            handleUnsolicited(atch, line);
        break;
    }

    pthread_mutex_unlock(&atch->impl->commandmutex);
}


/**
 * Returns a pointer to the end of the next line
 * special-cases the "> " SMS prompt
 *
 * returns NULL if there is no complete line
 */
static char * findNextEOL(char *cur)
{
    if (cur[0] == '>' && cur[1] == ' ' && cur[2] == '\0') {
        /* SMS prompt character...not \r terminated */
        return cur+2;
    }

    // Find next newline
    while (*cur != '\0' && *cur != '\r' && *cur != '\n') cur++;

    return *cur == '\0' ? NULL : cur;
}


/**
 * Reads a line from the AT channel, returns NULL on timeout.
 * Assumes it has exclusive read access to the FD
 *
 * This line is valid only until the next call to readline
 *
 * This function exists because as of writing, android libc does not
 * have buffered stdio.
 */

static const char *readline(ATChannel* atch)
{
    ssize_t count;

    char *p_read = NULL;
    char *p_eol = NULL;
    char *ret;

    /* this is a little odd. I use *ATBufferCur == 0 to
     * mean "buffer consumed completely". If it points to a character, than
     * the buffer continues until a \0
     */
    if (*atch->impl->ATBufferCur == '\0') {
        /* empty buffer */
        atch->impl->ATBufferCur = atch->impl->ATBuffer;
        *atch->impl->ATBufferCur = '\0';
        p_read = atch->impl->ATBuffer;
    } else {   /* *ATBufferCur != '\0' */
        /* there's data in the buffer from the last read */

        // skip over leading newlines
        while (*atch->impl->ATBufferCur == '\r' || *atch->impl->ATBufferCur == '\n')
            atch->impl->ATBufferCur++;

        p_eol = findNextEOL(atch->impl->ATBufferCur);

        if (p_eol == NULL) {
            /* a partial line. move it up and prepare to read more */
            size_t len;

            len = strlen(atch->impl->ATBufferCur);

            memmove(atch->impl->ATBuffer, atch->impl->ATBufferCur, len + 1);
            p_read = atch->impl->ATBuffer + len;
            atch->impl->ATBufferCur = atch->impl->ATBuffer;
        }
        /* Otherwise, (p_eol !- NULL) there is a complete line  */
        /* that will be returned the while () loop below        */
    }

    while (p_eol == NULL) {
        if (0 == MAX_AT_RESPONSE - (size_t)(p_read - atch->impl->ATBuffer)) {
            RLOGE(atch, "ERROR: Input line exceeded buffer.");
            /* ditch buffer and start over again */
            atch->impl->ATBufferCur = atch->impl->ATBuffer;
            *atch->impl->ATBufferCur = '\0';
            p_read = atch->impl->ATBuffer;
        }

        do {
            count = read(atch->fd, p_read,
                            MAX_AT_RESPONSE - (size_t)(p_read - atch->impl->ATBuffer));
        } while (count < 0 && errno == EINTR);

        if (count > 0) {
            AT_DUMP( atch, "<< ", p_read, count );

            p_read[count] = '\0';

            // skip over leading newlines
            while (*atch->impl->ATBufferCur == '\r' || *atch->impl->ATBufferCur == '\n')
                atch->impl->ATBufferCur++;

            p_eol = findNextEOL(atch->impl->ATBufferCur);
            p_read += count;
        } else if (count <= 0) {
            /* read error encountered or EOF reached */
            if(count == 0) {
                RLOGD(atch, "atchannel: EOF reached.");
            } else {
                RLOGD(atch, "atchannel: read error %s.", strerror(errno));
            }
            return NULL;
        }
    }

    /* a full line in the buffer. Place a \0 over the \r and return */

    ret = atch->impl->ATBufferCur;
    *p_eol = '\0';
    atch->impl->ATBufferCur = p_eol + 1; /* this will always be <= p_read,    */
                              /* and there will be a \0 at *p_read */

    RLOGD(atch, "AT< %s", ret);
    return ret;
}


static void onReaderClosed(ATChannel* atch)
{
    if (atch->onCloseHandler != NULL && atch->impl->readerClosed == 0) {

        pthread_mutex_lock(&atch->impl->commandmutex);

        atch->impl->readerClosed = 1;

        pthread_cond_signal(&atch->impl->commandcond);

        pthread_mutex_unlock(&atch->impl->commandmutex);

        atch->onCloseHandler(atch);
    }
}


static void *readerLoop(void *arg)
{
    ATChannel* atch = (ATChannel*)arg;

    for (;;) {
        const char * line;

        line = readline(atch);

        if (line == NULL) {
            break;
        }

        if(isSMSUnsolicited(line)) {
            char *line1;
            const char *line2;

            // The scope of string returned by 'readline()' is valid only
            // till next call to 'readline()' hence making a copy of line
            // before calling readline again.
            line1 = strdup(line);
            line2 = readline(atch);

            if (line2 == NULL) {
                free(line1);
                break;
            }

            if (atch->unsolHandler != NULL) {
                atch->unsolHandler (atch, line1, line2);
            }
            free(line1);
        } else {
            processLine(atch, line);
        }
    }

    onReaderClosed(atch);

    return NULL;
}

/**
 * Sends string s to the radio with a \r appended.
 * Returns AT_ERROR_* on error, 0 on success
 *
 * This function exists because as of writing, android libc does not
 * have buffered stdio.
 */
static int writeline (ATChannel* atch, const char *s)
{
    size_t cur = 0;
    size_t len = strlen(s);
    ssize_t written;

    if (atch->fd < 0 || atch->impl->readerClosed > 0) {
        return AT_ERROR_CHANNEL_CLOSED;
    }

    RLOGD(atch, "AT> %s", s);

    AT_DUMP( atch, ">> ", s, strlen(s) );

    /* the main string */
    while (cur < len) {
        do {
            written = write (atch->fd, s + cur, len - cur);
        } while (written < 0 && errno == EINTR);

        if (written < 0) {
            return AT_ERROR_GENERIC;
        }

        cur += (size_t)written;
    }

    /* the \r  */

    do {
        written = write (atch->fd, "\r" , 1);
    } while ((written < 0 && errno == EINTR) || (written == 0));

    if (written < 0) {
        return AT_ERROR_GENERIC;
    }

    return 0;
}
static int writeCtrlZ (ATChannel* atch, const char *s)
{
    size_t cur = 0;
    size_t len = strlen(s);
    ssize_t written;

    if (atch->fd < 0 || atch->impl->readerClosed > 0) {
        return AT_ERROR_CHANNEL_CLOSED;
    }

    RLOGD(atch, "AT> %s^Z", s);

    AT_DUMP( atch, ">* ", s, strlen(s) );

    /* the main string */
    while (cur < len) {
        do {
            written = write (atch->fd, s + cur, len - cur);
        } while (written < 0 && errno == EINTR);

        if (written < 0) {
            return AT_ERROR_GENERIC;
        }

        cur += (size_t)written;
    }

    /* the ^Z  */

    do {
        written = write (atch->fd, "\032" , 1);
    } while ((written < 0 && errno == EINTR) || (written == 0));

    if (written < 0) {
        return AT_ERROR_GENERIC;
    }

    return 0;
}

static void clearPendingCommand(ATChannel* atch)
{
    if (atch->impl->p_response != NULL) {
        at_response_free(atch->impl->p_response);
    }

    atch->impl->p_response = NULL;
    atch->impl->responsePrefix = NULL;
    atch->impl->smsPDU = NULL;
}


ATReturn  at_open(ATChannel* atch)
{
    int fd = 0;

    fd = open(atch->path, O_RDWR);
    if (fd < 0) {
        RLOGE(atch, "opening port %s failed.", atch->path);
        return AT_ERROR_GENERIC;
    }
    atch->fd = fd;

    struct termios ios;
    tcgetattr(fd, &ios);
    ios.c_lflag = atch->lflag;
    tcsetattr(fd, TCSANOW, &ios);

    return at_attach(atch);
}


/**
 * Starts AT handler on stream "fd'
 * returns 0 on success, -1 on error
 */
ATReturn  at_attach(ATChannel* atch)
{
    int ret;
    pthread_attr_t attr;

    atch->impl = calloc(1, sizeof(*atch->impl));
    atch->impl->tid_reader = 0;
    atch->impl->ATBufferCur = atch->impl->ATBuffer;
    pthread_mutex_init(&atch->impl->commandmutex, NULL);
    pthread_cond_init(&atch->impl->commandcond, NULL);
    atch->impl->type = 0;
    atch->impl->responsePrefix = NULL;
    atch->impl->smsPDU = NULL;
    atch->impl->p_response = NULL;
    atch->impl->readerClosed = 0;

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&atch->impl->tid_reader, &attr, readerLoop, atch);

    if (ret < 0) {
        free(atch->impl);
        atch->impl = NULL;
        perror ("pthread_create");
        return AT_ERROR_GENERIC;
    }


    return AT_SUCCESS;
}

/* FIXME is it ok to call this from the reader and the command thread? */
void at_close(ATChannel* atch)
{
    if (atch->fd >= 0) {
        close(atch->fd);
    }
    atch->fd = -1;

    pthread_mutex_lock(&atch->impl->commandmutex);

    atch->impl->readerClosed = 1;

    pthread_cond_signal(&atch->impl->commandcond);

    pthread_mutex_unlock(&atch->impl->commandmutex);

    free(atch->impl);
    atch->impl = NULL;

    /* the reader thread should eventually die */
}

static ATResponse * at_response_new(void)
{
    return (ATResponse *) calloc(1, sizeof(ATResponse));
}

void at_response_free(ATResponse *p_response)
{
    ATLine *p_line;

    if (p_response == NULL) return;

    p_line = p_response->p_intermediates;

    while (p_line != NULL) {
        ATLine *p_toFree;

        p_toFree = p_line;
        p_line = p_line->p_next;

        free(p_toFree->line);
        free(p_toFree);
    }

    free (p_response->finalResponse);
    free (p_response);
}

/**
 * The line reader places the intermediate responses in reverse order
 * here we flip them back
 */
static void reverseIntermediates(ATResponse *p_response)
{
    ATLine *pcur,*pnext;

    pcur = p_response->p_intermediates;
    p_response->p_intermediates = NULL;

    while (pcur != NULL) {
        pnext = pcur->p_next;
        pcur->p_next = p_response->p_intermediates;
        p_response->p_intermediates = pcur;
        pcur = pnext;
    }
}

/**
 * Internal send_command implementation
 * Doesn't lock or call the timeout callback
 *
 * timeoutMsec == 0 means infinite timeout
 */

static int at_send_command_full_nolock (ATChannel* atch, const char *command, ATCommandType type,
                    const char *responsePrefix, const char *smspdu,
                    long long timeoutMsec, ATResponse **pp_outResponse)
{
    int err = 0;
    struct timespec ts;

    if(atch->impl->p_response != NULL) {
        err = AT_ERROR_COMMAND_PENDING;
        goto error;
    }

    err = writeline (atch, command);

    if (err < 0) {
        goto error;
    }

    atch->impl->type = type;
    atch->impl->responsePrefix = responsePrefix;
    atch->impl->smsPDU = smspdu;
    atch->impl->p_response = at_response_new();

    if (timeoutMsec != 0) {
        setTimespecRelative(&ts, timeoutMsec);
    }

    while (atch->impl->p_response->finalResponse == NULL && atch->impl->readerClosed == 0) {
        if (timeoutMsec != 0) {
            err = pthread_cond_timedwait(&atch->impl->commandcond, &atch->impl->commandmutex, &ts);
        } else {
            err = pthread_cond_wait(&atch->impl->commandcond, &atch->impl->commandmutex);
        }

        if (err == ETIMEDOUT) {
            err = AT_ERROR_TIMEOUT;
            goto error;
        }
    }

    if (pp_outResponse == NULL) {
        at_response_free(atch->impl->p_response);
    } else {
        /* line reader stores intermediate responses in reverse order */
        reverseIntermediates(atch->impl->p_response);
        *pp_outResponse = atch->impl->p_response;
    }

    atch->impl->p_response = NULL;

    if(atch->impl->readerClosed > 0) {
        err = AT_ERROR_CHANNEL_CLOSED;
        goto error;
    }

    err = 0;
error:
    clearPendingCommand(atch);

    return err;
}

/**
 * Internal send_command implementation
 *
 * timeoutMsec == 0 means infinite timeout
 */
static int at_send_command_full (ATChannel* atch, const char *command, ATCommandType type,
                    const char *responsePrefix, const char *smspdu,
                    long long timeoutMsec, ATResponse **pp_outResponse)
{
    int err;

    if (0 != pthread_equal(atch->impl->tid_reader, pthread_self())) {
        /* cannot be called from reader thread */
        return AT_ERROR_INVALID_THREAD;
    }

    pthread_mutex_lock(&atch->impl->commandmutex);

    err = at_send_command_full_nolock(atch, command, type,
                    responsePrefix, smspdu,
                    timeoutMsec, pp_outResponse);

    pthread_mutex_unlock(&atch->impl->commandmutex);

    if (err == AT_ERROR_TIMEOUT && atch->onTimeoutHandler != NULL) {
        atch->onTimeoutHandler(atch);
    }

    return err;
}


/**
 * Issue a single normal AT command with no intermediate response expected
 *
 * "command" should not include \r
 * pp_outResponse can be NULL
 *
 * if non-NULL, the resulting ATResponse * must be eventually freed with
 * at_response_free
 */
ATReturn at_send_command (ATChannel* atch, const char *command, ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (atch, command, NO_RESULT, NULL,
                                    NULL, 0, pp_outResponse);

    return err;
}


ATReturn at_send_command_singleline (ATChannel* atch, const char *command,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse)
{
    ATReturn err;

    err = at_send_command_full (atch, command, SINGLELINE, responsePrefix,
                                    NULL, 0, pp_outResponse);

    if (err == AT_SUCCESS && pp_outResponse != NULL
        && (*pp_outResponse)->success
        && (*pp_outResponse)->p_intermediates == NULL
    ) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


ATReturn at_send_command_numeric (ATChannel* atch, const char *command,
                                 ATResponse **pp_outResponse)
{
    ATReturn err;

    err = at_send_command_full (atch, command, NUMERIC, NULL,
                                    NULL, 0, pp_outResponse);

    if (err == AT_SUCCESS && pp_outResponse != NULL
        && (*pp_outResponse)->success
        && (*pp_outResponse)->p_intermediates == NULL
    ) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


ATReturn at_send_command_sms (ATChannel* atch, const char *command,
                                const char *pdu,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse)
{
    ATReturn err;

    err = at_send_command_full (atch, command, SINGLELINE, responsePrefix,
                                    pdu, 0, pp_outResponse);

    if (err == AT_SUCCESS && pp_outResponse != NULL
        && (*pp_outResponse)->success
        && (*pp_outResponse)->p_intermediates == NULL
    ) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


ATReturn at_send_command_multiline (ATChannel* atch, const char *command,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse)
{
    ATReturn err;

    err = at_send_command_full (atch, command, MULTILINE, responsePrefix,
                                    NULL, 0, pp_outResponse);

    return err;
}


/**
 * Periodically issue an AT command and wait for a response.
 * Used to ensure channel has start up and is active
 */

ATReturn at_handshake(ATChannel* atch)
{
    int i;
    int err = 0;

    if (0 != pthread_equal(atch->impl->tid_reader, pthread_self())) {
        /* cannot be called from reader thread */
        return AT_ERROR_INVALID_THREAD;
    }

    pthread_mutex_lock(&atch->impl->commandmutex);

    for (i = 0 ; i < HANDSHAKE_RETRY_COUNT ; i++) {
        /* some stacks start with verbose off */
        err = at_send_command_full_nolock (atch, "ATE0Q0V1", NO_RESULT,
                    NULL, NULL, HANDSHAKE_TIMEOUT_MSEC, NULL);

        if (err == 0) {
            break;
        }
    }

    if (err == 0) {
        /* pause for a bit to let the input buffer drain any unmatched OK's
           (they will appear as extraneous unsolicited responses) */

        sleepMsec(HANDSHAKE_TIMEOUT_MSEC);
    }

    pthread_mutex_unlock(&atch->impl->commandmutex);

    return err;
}

/**
 * Returns error code from response
 * Assumes AT+CMEE=1 (numeric) mode
 */
AT_CME_Error at_get_cme_error(const ATResponse *p_response)
{
    int ret;
    int err;
    char *p_cur;

    if (p_response->success) {
        return CME_SUCCESS;
    }

    if (p_response->finalResponse == NULL
        || !strStartsWith(p_response->finalResponse, "+CME ERROR:")
    ) {
        return CME_ERROR_NON_CME;
    }

    p_cur = p_response->finalResponse;
    err = at_tok_start(&p_cur);

    if (err < 0) {
        return CME_ERROR_NON_CME;
    }

    err = at_tok_nextint(&p_cur, &ret);

    if (err < 0) {
        return CME_ERROR_NON_CME;
    }

    return (AT_CME_Error) ret;
}


static void outputLog(ATChannel* atch, int level, const char* format, ...)
{
    if (!atch->log) {
        return;
    }
    if (atch->logLevel < level) {
        return;
    }

    char buff[1024];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buff, sizeof(buff), format, ap);
    va_end(ap);
    atch->log(atch, level, buff);

    return;
}
