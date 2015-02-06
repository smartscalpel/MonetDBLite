/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.monetdb.org/Legal/MonetDBLicense
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the MonetDB Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
 * Copyright August 2008-2015 MonetDB B.V.
 * All Rights Reserved.
 */

/*
 * @-
 * @+ Implementation
 * The commands merely encapsulate the functionality provided by
 * mal_profiler, which should be explicitly compiled with the kernel, because
 * it generates a noticable overhead.
 */

#ifndef _PROFILER_
#define _PROFILER_

#include "gdk.h"
#include <stdarg.h>
#include <time.h>
#include "mal_stack.h"
#include "mal_resolve.h"
#include "mal_exception.h"
#include "mal_client.h"
#include "mal_profiler.h"
#include "mal_interpreter.h"
#include "mal_runtime.h"

#ifdef WIN32
#if !defined(LIBMAL) && !defined(LIBATOMS) && !defined(LIBKERNEL) && !defined(LIBMAL) && !defined(LIBOPTIMIZER) && !defined(LIBSCHEDULER) && !defined(LIBMONETDB5)
#define profiler_export extern __declspec(dllimport)
#else
#define profiler_export extern __declspec(dllexport)
#endif
#else
#define profiler_export extern
#endif

profiler_export str CMDsetProfilerFile(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci);
profiler_export str CMDsetProfilerStream (Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci);
profiler_export str CMDstopProfiler(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci);
profiler_export str CMDstartStethoscope(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci);
profiler_export str CMDstartTomograph(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci);
profiler_export str CMDnoopProfiler(void *res);
profiler_export str CMDclearTrace(void *res);
profiler_export str CMDdumpTrace(void *res);
profiler_export str CMDgetTrace(bat *res, str *ev);
profiler_export str CMDopenProfilerStream(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci);
profiler_export str CMDcloseProfilerStream(void *res);
profiler_export str CMDcleanup(void *ret);
profiler_export str CMDgetDiskReads(lng *ret);
profiler_export str CMDgetDiskWrites(lng *ret);
profiler_export str CMDgetUserTime(lng *ret);
profiler_export str CMDgetSystemTime(lng *ret);
profiler_export str CMDstethoscope(void *ret, int *beat);
profiler_export str CMDtomograph(void *ret, int *beat);
profiler_export str CMDcpustats(lng *user, lng *nice, lng *sys, lng *idle, lng *iowait);
profiler_export str CMDcpuloadPercentage(int *cycles, int *io, lng *user, lng *nice, lng *sys, lng *idle, lng *iowait);
#endif  /* _PROFILER_*/
