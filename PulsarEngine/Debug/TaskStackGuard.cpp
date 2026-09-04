#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Debug/BetaLog.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>

/*
Watchdog for Pulsar's task thread, beta builds only.

Two crashes in a row were a DSI inside OSWakeupThread, reached from OSReceiveMessage inside
EGG::TaskThread::Run: the thread was idle, waiting for work, and the head of its own message
queue held a small integer instead of a thread pointer. Both times the damaged word was the
first one of EGG::Thread::messageQueue, at object+0x0C.

That crash is useless on its own, because it happens whenever the next task is posted - minutes
and several menus after whatever actually did the damage. So two things are watched here:

- the bottom of the stack, because EGG::TaskThread::Create puts the stack directly above the
  object and an overrun lands on exactly that field. The stack is painted and the smallest
  surviving margin is reported.
- the queue itself, on every page load, so the moment it stops making sense is reported together
  with the page and section that were being loaded. That names the transition instead of the
  wreckage.

If the paint survives while the queue is already broken, the stack is innocent and something else
is writing to that address.
*/

#ifdef BETA

namespace Pulsar {
namespace Debug {

static const u32 paintWord = 0xA5A5A5A5;
static const u32 paintBytes = 0x800;

static u32* paintedFrom = nullptr;
static u32 smallestMargin = 0xFFFFFFFF;
static bool queueReported = false;

static EGG::TaskThread* GetTaskThread() {
    const System* system = System::sInstance;
    if(system == nullptr) return nullptr;
    return system->taskThread;
}

//A queue head is either empty or a real thread, and every heap the game allocates a thread from
//lives above 0x80000000. The values actually seen in the crashes were 1 and 2.
static bool IsQueueHeadSane(const void* head) {
    const u32 value = reinterpret_cast<u32>(head);
    return value == 0 || value >= 0x80000000;
}

static void PaintStack() {
    const EGG::TaskThread* task = GetTaskThread();
    if(task == nullptr) return;
    //stackEnd is the low address: the stack starts above it and grows down onto it.
    u32* limit = reinterpret_cast<u32*>(task->stackEnd);
    if(limit == nullptr) return;
    for(u32 i = 0; i < paintBytes / sizeof(u32); ++i) limit[i] = paintWord;
    paintedFrom = limit;
    PUL_BETA_LOG("[StackGuard] task thread at 0x%08X, stack painted from 0x%08X for %d bytes\n",
                 (u32)task, (u32)limit, (int)paintBytes);
}
static BootHook PaintTaskStack(PaintStack, 2);

static void ReportQueue(EGG::TaskThread& task) {
    SectionMgr* sectionMgr = SectionMgr::sInstance;
    Section* section = nullptr;
    Page* page = nullptr;
    if(sectionMgr != nullptr) section = sectionMgr->curSection;
    if(section != nullptr) page = section->GetTopLayerPage();
    const int sectionId = section == nullptr ? -1 : (int)section->sectionId;
    const int pageId = page == nullptr ? -1 : (int)page->pageId;
    OS::Report("[StackGuard] task thread queue corrupt at section %d page %d\n", sectionId, pageId);
    //The object plus the two words in front of it, so a neighbour that ran over its own
    //allocation is visible as well as one that wrote into the middle of this one.
    const u32* raw = reinterpret_cast<const u32*>(&task) - 2;
    for(u32 i = 0; i < (0x58 / 4) + 2; i += 4) {
        OS::Report("[StackGuard]   %08X: %08X %08X %08X %08X\n", (u32)(raw + i),
                   raw[i], raw[i + 1], raw[i + 2], raw[i + 3]);
    }
}

static void Check() {
    EGG::TaskThread* task = GetTaskThread();
    if(task == nullptr) return;
    //The boot hook may well run before the System exists, so the paint is retried here.
    if(paintedFrom == nullptr) PaintStack();

    if(paintedFrom != nullptr) {
        u32 margin = 0;
        while(margin < paintBytes / sizeof(u32) && paintedFrom[margin] == paintWord) ++margin;
        margin *= sizeof(u32);
        if(margin < smallestMargin) {
            smallestMargin = margin;
            if(margin == 0) OS::Report("[StackGuard] stack paint gone: the thread overflowed past 0x%08X\n", (u32)paintedFrom);
            else PUL_BETA_LOG("[StackGuard] stack came within %d bytes of the bottom\n", (int)margin);
        }
    }

    if(queueReported) return;
    //EGG::TaskThread declares a messageQueue pointer of its own that hides the embedded one in
    //EGG::Thread; the queue the thread actually receives on is the base class member.
    const OS::MessageQueue& queue = task->EGG::Thread::messageQueue;
    if(IsQueueHeadSane(queue.sendQueue.head) && IsQueueHeadSane(queue.sendQueue.tail)
       && IsQueueHeadSane(queue.recvQueue.head) && IsQueueHeadSane(queue.recvQueue.tail)
       && queue.msgCount == (s32)task->msgCount) {
        return;
    }
    queueReported = true;
    ReportQueue(*task);
}
static PageLoadHook CheckTaskThread(Check);

}  // namespace Debug
}  // namespace Pulsar

#endif
