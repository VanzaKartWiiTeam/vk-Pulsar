#include <kamek.hpp>
#include <include/c_stdio.h>
#include <include/c_string.h>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/os/OSthread.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/Thread.hpp>
#include <core/egg/mem/ExpHeap.hpp>
#include <core/nw4r/db.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <PulsarSystem.hpp>
#include <IO/IO.hpp>
#include <VanzaKartChannel.hpp>
#include <Debug/Diag.hpp>

#ifdef VKDIAG

extern "C" void VIWaitForRetrace();

namespace Pulsar {
namespace Diag {

enum Kind {
    KIND_STEP,
    KIND_BEGIN,
    KIND_END
};

struct Entry {
    u32 ms;
    const char* name;
    u32 value;
    u32 kind;
};

struct OpenStep {
    const char* name;
    u32 value;
    u32 startMs;
};

static const u32 trailSize = 64;
static const u32 maxOpen = 8;
static const u32 openStepTimeoutMs = 45000;
//Race and menu scene loads block the main thread, and a slow SD makes them long.
static const u32 frameTimeoutMs = 60000;
static const u32 menuTimeoutMs = 240000;
//The frame check only arms after the counter was seen moving this many times in a row, so a
//field that turns out not to be a frame counter can never trigger it.
static const u32 framesToArm = 120;
static const u32 reportSize = 0x2000;

static Entry trail[trailSize];
static u32 trailCount = 0;
static OpenStep openSteps[maxOpen];
static u32 openDepth = 0;
static u32 lastProgressMs = 0;
static u64 startTime = 0;
static bool started = false;
static bool crashed = false;
static bool fired = false;

static u32 frameCount = 0;
static bool frameArmed = false;
static s32 sectionId = -1;
static s32 pageId = -1;
static u32 sectionLoads = 0;
static bool menuReached = false;

static IO* reportIO = nullptr;
static char* reportBuffer = nullptr;
static char screenBuffer[0x800];

struct Text {
    char* buf;
    u32 size;
    u32 len;
};

#define APPEND(t, ...)                                                          \
    do {                                                                        \
        if((t).len + 1 < (t).size) {                                            \
            int n = snprintf((t).buf + (t).len, (t).size - (t).len, __VA_ARGS__); \
            if(n > 0) (t).len += n;                                             \
            if((t).len >= (t).size) (t).len = (t).size - 1;                      \
        }                                                                       \
    } while(0)

static u32 Now() {
    if(startTime == 0) startTime = OS::GetTime();
    return OS::TicksToMilliseconds(OS::GetTime() - startTime);
}

static void Record(u32 kind, const char* name, u32 value) {
    const int level = OS::DisableInterrupts();
    const u32 now = Now();
    Entry& entry = trail[trailCount % trailSize];
    entry.ms = now;
    entry.name = name;
    entry.value = value;
    entry.kind = kind;
    ++trailCount;
    lastProgressMs = now;
    if(kind == KIND_BEGIN) {
        if(openDepth < maxOpen) {
            openSteps[openDepth].name = name;
            openSteps[openDepth].value = value;
            openSteps[openDepth].startMs = now;
        }
        ++openDepth;
    }
    else if(kind == KIND_END && openDepth > 0) --openDepth;
    OS::RestoreInterrupts(level);
    OS::Report("[DIAG] %u.%03us %s %s 0x%08X\n", now / 1000, now % 1000,
               kind == KIND_BEGIN ? "BEGIN" : kind == KIND_END ? "END" : "STEP", name, value);
}

void Step(const char* name, u32 value) { Record(KIND_STEP, name, value); }
void Begin(const char* name, u32 value) { Record(KIND_BEGIN, name, value); }
void End() {
    const char* name = "?";
    if(openDepth > 0 && openDepth <= maxOpen) name = openSteps[openDepth - 1].name;
    Record(KIND_END, name, 0);
}

static bool IsRamPointer(u32 address) {
    return (address >= 0x80000000 && address < 0x81800000) || (address >= 0x90000000 && address < 0x94000000);
}

static const char* IOTypeName() {
    const IO* io = IO::sInstance;
    if(io == nullptr) return "none";
    switch(io->type) {
        case IOType_RIIVO: return "RIIVO";
        case IOType_ISO: return "ISO";
        case IOType_DOLPHIN: return "DOLPHIN";
        case IOType_SD: return "SD";
    }
    return "?";
}

static void AppendInfo(Text& t) {
    const u32 now = Now();
    const u32 ios = *reinterpret_cast<u32*>(0x80003140);
    APPEND(t, "up %us fr %u%s sec 0x%X(%u) pg 0x%X io %s\n", now / 1000, frameCount, frameArmed ? "" : "(wd off)",
           sectionId, sectionLoads, pageId, IOTypeName());
    APPEND(t, "%.4s ios %u r%u ch %08X hooks %d%s mys %02X base %08X\n",
           reinterpret_cast<const char*>(0x80000000), ios >> 16, ios & 0xFFFF,
           *reinterpret_cast<u32*>(RRC_SIGNATURE_ADDRESS), (int)BootHook::executed,
           BootHook::executedFromFallback ? "fb" : "", *reinterpret_cast<u8*>(0x80001200), (u32)&Start);
}

static void AppendOpenSteps(Text& t) {
    const u32 depth = openDepth > maxOpen ? maxOpen : openDepth;
    if(depth == 0) return;
    const u32 now = Now();
    APPEND(t, "inside:");
    for(u32 i = 0; i < depth; ++i) {
        APPEND(t, " %s %X (%us)%s", openSteps[i].name, openSteps[i].value, (now - openSteps[i].startMs) / 1000,
               i + 1 < depth ? " >" : "");
    }
    APPEND(t, "\n");
}

static void AppendTrail(Text& t, u32 maxEntries) {
    u32 count = trailCount < trailSize ? trailCount : trailSize;
    if(count > maxEntries) count = maxEntries;
    for(u32 i = trailCount - count; i < trailCount; ++i) {
        const Entry& entry = trail[i % trailSize];
        APPEND(t, "%4u.%u %c %s %X\n", entry.ms / 1000, (entry.ms % 1000) / 100,
               entry.kind == KIND_BEGIN ? '+' : entry.kind == KIND_END ? '-' : ' ', entry.name, entry.value);
    }
}

//Every thread the OS knows about, with where it is parked. A thread that is not running has its
//registers saved in its context, so the pc and the back chain say what it is waiting on.
static void AppendThreads(Text& t, u32 maxThreads, u32 frames) {
    const OS::Thread* self = OS::Thread::current;
    const OS::Thread* thread = *reinterpret_cast<OS::Thread**>(0x800000DC);
    for(u32 i = 0; i < maxThreads && thread != nullptr && IsRamPointer((u32)thread); ++i) {
        const OS::Context& context = thread->context;
        APPEND(t, "%08X s%u p%u pc %08X lr %08X%s", (u32)thread, thread->state, thread->priority,
               context.srr0, context.lr, thread == self ? " (watchdog)" : "");
        u32 sp = context.gpr[1];
        for(u32 frame = 0; frame < frames && IsRamPointer(sp); ++frame) {
            const u32 next = *reinterpret_cast<u32*>(sp);
            if(!IsRamPointer(next) || next <= sp) break;
            APPEND(t, " %08X", *reinterpret_cast<u32*>(next + 4));
            sp = next;
        }
        APPEND(t, "\n");
        thread = thread->activeThreads.next;
    }
}

static void Fire(const char* reason) {
    fired = true;
    Text t = { screenBuffer, sizeof(screenBuffer), 0 };
    APPEND(t, "VanzaKart DIAG - %s\nPhoto this screen and send it to the devs.\n", reason);
    AppendInfo(t);
    AppendOpenSteps(t);
    AppendTrail(t, 8);
    AppendThreads(t, 8, 3);
    OS::Report("%s", screenBuffer);
    GX::Color fg;
    fg.rgba = 0xFFFFFFFF;
    GX::Color bg;
    bg.rgba = 0x000080FF;
    OS::Fatal(fg, bg, screenBuffer);
}

static u32 ReadFrameCount() {
    const EGG::AsyncDisplay* display = RKSystem::mInstance.asyncDisplay;
    if(display == nullptr || !IsRamPointer((u32)display)) return 0;
    return display->frameCount;
}

static void WatchdogLoop(void*) {
    u32 lastFrame = ReadFrameCount();
    u32 lastFrameChangeMs = Now();
    u32 consecutive = 0;
    for(;;) {
        VIWaitForRetrace();
        if(crashed || fired) continue;
        const u32 now = Now();
        const u32 frame = ReadFrameCount();
        frameCount = frame;
        if(frame != lastFrame) {
            lastFrame = frame;
            lastFrameChangeMs = now;
            if(!frameArmed && ++consecutive >= framesToArm) {
                frameArmed = true;
                OS::Report("[DIAG] frame watchdog armed at %u ms\n", now);
            }
        }
        else if(!frameArmed) consecutive = 0;

        const char* reason = nullptr;
        if(frameArmed && now - lastFrameChangeMs > frameTimeoutMs) reason = "FREEZE: no frame for 60s";
        else if(openDepth > 0 && now - lastProgressMs > openStepTimeoutMs) reason = "STUCK: a step ran 45s";
        else if(!menuReached && now > menuTimeoutMs) reason = "main menu not reached in 4 min";
        if(reason != nullptr) Fire(reason);
    }
}

void Start() {
    if(started) return;
    started = true;
    Now();
    Step("diag start, thread", (u32)OS::Thread::current);
    EGG::Heap* heap = RKSystem::mInstance.EGGSystem;
    reportBuffer = EGG::Heap::alloc<char>(reportSize, 0x20, heap);
    EGG::TaskThread* watchdog = EGG::TaskThread::Create(2, 0, 0x2000, heap);
    if(watchdog == nullptr) {
        Step("watchdog NOT created", 0);
        return;
    }
    watchdog->Request(&WatchdogLoop, (void*)0, 0);
    Step("watchdog created", (u32)watchdog);
}

void PrintOnExceptionScreen() {
    crashed = true;
    Text t = { screenBuffer, sizeof(screenBuffer), 0 };
    APPEND(t, "VK DIAG: photo this, then A. Send Crash.pul+Diag.txt\n");
    AppendInfo(t);
    AppendOpenSteps(t);
    AppendTrail(t, 6);
    //One line per call: the console formats into a small buffer of its own.
    char* line = screenBuffer;
    for(char* c = screenBuffer; *c != '\0'; ++c) {
        if(*c != '\n') continue;
        *c = '\0';
        nw4r::db::Exception_Printf_("%s\n", line);
        line = c + 1;
    }
    if(*line != '\0') nw4r::db::Exception_Printf_("%s\n", line);
}

static void AppendHeap(Text& t, const char* name, EGG::ExpHeap* heap) {
    if(heap == nullptr || !IsRamPointer((u32)heap)) APPEND(t, " %s -", name);
    else APPEND(t, " %s %u", name, heap->getTotalFreeSize());
}

void WriteReportFile(const char* reason) {
    const System* system = System::sInstance;
    if(reportBuffer == nullptr || system == nullptr || IO::sInstance == nullptr) return;
    if(reportIO == nullptr) reportIO = IO::CreatePrivateInstance(IO::sInstance->type, system->heap, system->taskThread);
    if(reportIO == nullptr) return;

    Text t = { reportBuffer, reportSize, 0 };
    APPEND(t, "VanzaKart diagnostic report\nreason: %s\npack id %u version %u\n", reason,
           *reinterpret_cast<u32*>(0x800017D0), *reinterpret_cast<u32*>(0x800017D4));
    AppendInfo(t);
    APPEND(t, "channel abi %08X flags %02X, mem1 %08X mem2 %08X, riivo/dolphin io %s\n",
           *reinterpret_cast<u32*>(RRC_ABI_VERSION_ADDRESS), *reinterpret_cast<u8*>(RRC_BITFLAGS_ADDRESS),
           *reinterpret_cast<u32*>(0x80000028), *reinterpret_cast<u32*>(0x80003118), IOTypeName());
    if(!crashed) {
        APPEND(t, "free:");
        AppendHeap(t, "system", RKSystem::mInstance.EGGSystem);
        AppendHeap(t, "mem1", RKSystem::mInstance.EGGRootMEM1);
        AppendHeap(t, "mem2", RKSystem::mInstance.EGGRootMEM2);
        APPEND(t, "\n");
    }
    AppendOpenSteps(t);
    APPEND(t, "trail:\n");
    AppendTrail(t, trailSize);
    APPEND(t, "threads:\n");
    AppendThreads(t, 16, 6);
    //Overwrite does not truncate, so always write the whole buffer: a shorter report must not
    //leave the tail of a longer one behind.
    for(u32 i = t.len; i < reportSize; ++i) reportBuffer[i] = ' ';

    char path[IOS::ipcMaxPath];
    snprintf(path, IOS::ipcMaxPath, "%s/Diag.txt", system->GetModFolder());
    if(reportIO->CreateAndOpen(path, FILE_MODE_READ_WRITE)) {
        const s32 written = reportIO->Overwrite(reportSize, reportBuffer);
        reportIO->Close();
        OS::Report("[DIAG] report written to %s (%d)\n", path, written);
    }
    else OS::Report("[DIAG] could not open %s\n", path);
}

static void OnSectionLoad() {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if(sectionMgr == nullptr || sectionMgr->curSection == nullptr) return;
    sectionId = sectionMgr->curSection->sectionId;
    ++sectionLoads;
    Step("section", sectionId);
    if(!menuReached && sectionId >= SECTION_MAIN_MENU_FROM_BOOT && sectionId <= SECTION_MAIN_MENU_FROM_LICENSE) {
        menuReached = true;
        WriteReportFile("main menu reached, boot OK");
    }
}
static SectionLoadHook DiagSectionLoad(OnSectionLoad);

static void OnPageLoad() {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if(sectionMgr == nullptr || sectionMgr->curSection == nullptr) return;
    const Page* page = sectionMgr->curSection->GetTopLayerPage();
    if(page != nullptr) pageId = page->pageId;
}
static PageLoadHook DiagPageLoad(OnPageLoad);

}  // namespace Diag
}  // namespace Pulsar

#endif
