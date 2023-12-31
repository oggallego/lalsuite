/*
*  Copyright (C) 2007 Jolien Creighton, Bernd Machenschalk
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with with program; see the file COPYING. If not, write to the
*  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
*  MA  02110-1301  USA
*/

// ---------- SEE LALError.dox for doxygen documentation ----------

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#define BACKTRACE_LEVELMAX 0100
#endif

#include <lal/LALMalloc.h>
#include <lal/LALError.h>

void FREESTATUSPTR(LALStatus * status);
void REPORTSTATUS(LALStatus * status);

#undef LALError
#undef LALWarning
#undef LALInfo
#undef LALTrace

int LALPrintError(const char *fmt, ...)
{
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vfprintf(stderr, fmt, ap);
    va_end(ap);
    return n;
}


int (*lalRaiseHook) (int, const char *, ...) = LALRaise;
int LALRaise(int sig, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    (void) vfprintf(stderr, fmt, ap);
    va_end(ap);
#if defined(HAVE_BACKTRACE) && defined(BACKTRACE_LEVELMAX)
    void *callstack[BACKTRACE_LEVELMAX];
    size_t frames = backtrace(callstack, BACKTRACE_LEVELMAX);
    fprintf(stderr, "backtrace:\n");
    backtrace_symbols_fd(callstack, frames, fileno(stderr));
#endif
    return raise(sig);
}



void (*lalAbortHook) (const char *, ...) = LALAbort;
void LALAbort(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    (void) vfprintf(stderr, fmt, ap);
    va_end(ap);
#if defined(HAVE_BACKTRACE) && defined(BACKTRACE_LEVELMAX)
    void *callstack[BACKTRACE_LEVELMAX];
    size_t frames = backtrace(callstack, BACKTRACE_LEVELMAX);
    fprintf(stderr, "backtrace:\n");
    backtrace_symbols_fd(callstack, frames, fileno(stderr));
#endif
    abort();
}



int LALError(LALStatus * status, const char *statement)
{
    int n = 0;
    if (lalDebugLevel & LALERROR) {
        n = LALPrintError
            ("Error[%d] %d: function %s, file %s, line %d, %s\n"
             "        %s %s\n", status->level, status->statusCode,
             status->function, status->file, status->line, status->Id,
             statement ? statement : "", status->statusDescription);
    }
    return n;
}



int LALWarning(LALStatus * status, const char *warning)
{
    int n = 0;
    if (lalDebugLevel & LALWARNING) {
        n = LALPrintError
            ("Warning[%d]: function %s, file %s, line %d, %s\n"
             "        %s\n", status->level, status->function, status->file,
             status->line, status->Id, warning);
    }
    return n;
}



int LALInfo(LALStatus * status, const char *info)
{
    int n = 0;
    if (lalDebugLevel & LALINFO) {
        n = LALPrintError("Info[%d]: function %s, file %s, line %d, %s\n"
                          "        %s\n", status->level, status->function,
                          status->file, status->line, status->Id, info);
    }
    return n;
}



int LALTrace(LALStatus * status, int exitflg)
{
    int n = 0;
    if (lalDebugLevel & LALTRACE) {
        n = LALPrintError("%s[%d]: function %s, file %s, line %d, %s\n",
                          exitflg ? "Leave" : "Enter", status->level,
                          status->function, status->file, status->line,
                          status->Id);
    }
    return n;
}


int
LALInitStatus(LALStatus * status, const char *function, const char *id,
              const char *file, const int line)
{
    int exitcode = 0;
    if (status) {
        INT4 level = status->level;
        exitcode = status->statusPtr ? 1 : 0;
        memset(status, 0, sizeof(LALStatus));   /* possible memory leak */
        status->level = level > 0 ? level : 1;
        status->Id = id;
        status->function = function;
        status->file = file;
        status->line = line;
        (void) LALTrace(status, 0);
        if (exitcode) {
            LALPrepareAbort(status, -2,
                            "INITSTATUS: non-null status pointer", file,
                            line);
        } else if (xlalErrno) {
            LALPrepareAbort(status, -16, "INITSTATUS: non-zero xlalErrno",
                            file, line);
            exitcode = 1;
        }
    } else {
        lalAbortHook("Abort: function %s, file %s, line %d, %s\n"
                     "       Null status pointer passed to function\n",
                     function, file, line, id);
    }
    return exitcode;
}



int LALPrepareReturn(LALStatus * status, const char *file, const int line)
{
    status->file = file;
    status->line = line;
    if (status->statusCode) {
        (void) LALError(status, "RETURN:");
    }
    (void) LALTrace(status, 1);
    if (xlalErrno) {
        LALPrepareAbort(status, -32, "RETURN: untrapped XLAL error",
                        file, line);
    }
    return 1;
}



int
LALAttatchStatusPtr(LALStatus * status, const char *file, const int line)
{
    int exitcode = 0;
    if (status->statusPtr) {
        LALPrepareAbort(status, -2,
                        "ATTATCHSTATUSPTR: non-null status pointer", file,
                        line);
        exitcode = 1;
    } else {
        status->statusPtr = (LALStatus *) LALCalloc(1, sizeof(LALStatus));
        if (!status->statusPtr) {
            LALPrepareAbort(status, -4,
                            "ATTATCHSTATUSPTR: memory allocation error",
                            file, line);
            exitcode = 1;
        } else {
            status->statusPtr->level = status->level + 1;
        }
    }
    return exitcode;
}



int
LALDetatchStatusPtr(LALStatus * status, const char *file, const int line)
{
    int exitcode = 0;
    if (status->statusPtr) {
        FREESTATUSPTR(status);
        status->statusCode = 0;
        status->statusDescription = NULL;
    } else {
        LALPrepareAbort(status, -8,
                        "DETATCHSTATUSPTR: null status pointer", file,
                        line);
        exitcode = 1;
    }
    return exitcode;
}



int
LALPrepareAbort(LALStatus * status, const INT4 code, const char *mesg,
                const char *file, const int line)
{
    if (status->statusPtr) {
        FREESTATUSPTR(status);
    }
    status->file = file;
    status->line = line;
    status->statusCode = code;
    status->statusDescription = mesg;
    if (code) {
        (void) LALError(status, "ABORT:");
    }
    (void) LALTrace(status, 1);
    return 1;
}



int
LALPrepareAssertFail(LALStatus * status, const INT4 code, const char *mesg,
                     const char *statement, const char *file,
                     const int line)
{
    if (status->statusPtr) {
        FREESTATUSPTR(status);
    }
    status->file = file;
    status->line = line;
    status->statusCode = code;
    status->statusDescription = mesg;
    if (lalDebugLevel & LALERROR)
        (void) statement;
    (void) LALError(status, statement);
    (void) LALTrace(status, 1);
    return 1;
}



int
LALCheckStatusPtr(LALStatus * status, const char *statement,
                  const char *file, const int line)
{
    if (status->statusPtr->statusCode) {
        status->file = file;
        status->line = line;
        status->statusCode = -1;
        status->statusDescription = "Recursive error";
        if (lalDebugLevel & LALERROR)
            (void) statement;
        (void) LALError(status, statement);
        (void) LALTrace(status, 1);
        return 1;
    }
    return 0;
}



/*
 * This function is somewhat dangerous: need to check to see
 * if status->statusPtr is initially null before calling FREESTATUSPTR
 */

void FREESTATUSPTR(LALStatus * status)
{
    do {
        LALStatus *next = status->statusPtr->statusPtr;
        LALFree(status->statusPtr);
        status->statusPtr = next;
    }
    while (status->statusPtr);
    return;
}



void REPORTSTATUS(LALStatus * status)
{
    LALStatus *ptr;
    for (ptr = status; ptr; ptr = ptr->statusPtr) {
        LALPrintError("\nLevel %i: %s\n", ptr->level, ptr->Id);
        if (ptr->statusCode) {
            LALPrintError("\tStatus code %i: %s\n", ptr->statusCode,
                          ptr->statusDescription);
        } else {
            LALPrintError("\tStatus code 0: Nominal\n");
        }
        LALPrintError("\tfunction %s, file %s, line %i\n",
                      ptr->function, ptr->file, ptr->line);
    }
    return;
}




/*
 * Error handlers for LALApps applications
 */

#define FAILMSG( stat, func, file, line, id )                                  \
  do {                                                                         \
    if ( lalDebugLevel & LALERROR )                                            \
    {                                                                          \
      LALPrintError( "Error[0]: file %s, line %d, %s\n"                        \
          "\tLAL_CALL: Function call `%s' failed.\n", file, line, id, func );  \
    }                                                                          \
    if ( vrbflg )                                                              \
    {                                                                          \
      fprintf(stderr,"Level 0: %s\n\tFunction call `%s' failed.\n"             \
          "\tfile %s, line %d\n", id, func, file, line );                      \
      REPORTSTATUS( stat );                                                    \
    }                                                                          \
  } while( 0 )

int vrbflg = 0;

lal_errhandler_t lal_errhandler = LAL_ERR_DFLT;

int LAL_ERR_EXIT(
    LALStatus  *stat,
    const char *func,
    const char *file,
    const int   line,
    volatile const char *id
    )
{
  if ( stat->statusCode )
  {
    FAILMSG( stat, func, file, line, id );
    exit( 1 );
  }
  return stat->statusCode;
}

int LAL_ERR_ABRT(
    LALStatus  *stat,
    const char *func,
    const char *file,
    const int   line,
    volatile const char *id
    )
{
  if ( stat->statusCode )
  {
    FAILMSG( stat, func, file, line, id );
    abort();
  }
  return 0;
}

int LAL_ERR_RTRN(
    LALStatus  *stat,
    const char *func,
    const char *file,
    const int   line,
    volatile const char *id
    )
{
  if ( stat->statusCode )
  {
    FAILMSG( stat, func, file, line, id );
  }
  return stat->statusCode;
}
