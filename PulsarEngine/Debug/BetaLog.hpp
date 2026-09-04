#ifndef _PUL_BETALOG_
#define _PUL_BETALOG_

#include <core/rvl/OS/OS.hpp>

/*
Traces that only exist in a beta build. compile_and_link.py passes -DBETA when it
is invoked with -beta, so these lines reach the Dolphin console through OSReport
while a tester plays, and disappear completely from the stable build: the macro
expands to (void)0, so neither the call nor the format string is emitted.

Keep them on cold paths (page load, race load, mode toggles, one-shot events).
Anything that runs every frame for every player will drown the console.
*/

#ifdef BETA
#define PUL_BETA_LOG(...) OS::Report(__VA_ARGS__)
#else
#define PUL_BETA_LOG(...) ((void)0)
#endif

#endif
