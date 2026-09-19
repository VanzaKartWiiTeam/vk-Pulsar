#ifndef _PUL_DIAG_
#define _PUL_DIAG_

#include <types.hpp>
#include <core/rvl/os/OSContext.hpp>

#ifdef VKDIAG

namespace Pulsar {
namespace Diag {

void Start();                                   //idempotent, called before the BootHooks run
void Step(const char* name, u32 value);         //one-shot breadcrumb
void Begin(const char* name, u32 value);        //opens a step the watchdog times
void End();                                     //closes the innermost Begin
void HeapStep(const char* name);                //breadcrumb with the system heap's free size
void PrintOnExceptionScreen(u16 error, const OS::Context* context, u32 dsisr, u32 dar);                  //called from the exception header
void WriteReportFile(const char* reason);       //<mod folder>/Diag.txt

}  // namespace Diag
}  // namespace Pulsar

#define DIAG_STEP(name, value) Pulsar::Diag::Step(name, (u32)(value))
#define DIAG_BEGIN(name, value) Pulsar::Diag::Begin(name, (u32)(value))
#define DIAG_END() Pulsar::Diag::End()
#define DIAG_HEAP(name) Pulsar::Diag::HeapStep(name)

#else

#define DIAG_STEP(name, value) ((void)0)
#define DIAG_BEGIN(name, value) ((void)0)
#define DIAG_END() ((void)0)
#define DIAG_HEAP(name) ((void)0)

#endif

#endif
