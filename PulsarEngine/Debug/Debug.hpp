#ifndef _PUL_Debug_
#define _PUL_Debug_

#include <core/rvl/os/OSError.hpp>
#include <core/rvl/os/OScontext.hpp>

namespace Pulsar {
namespace Debug {

void FatalError(const char* string);
void LaunchSoftware();
struct GPR {
    void Set(const OS::Context& context, u32 idx, u32 regValue) {
        gpr = context.gpr[idx];
        name = 'r00:' + regValue;
    }
    u32 name;
    u32 gpr;
};

struct FPR {
    void Set(const OS::Context& context, u32 idx, u32 regValue) {
        fpr = context.fpr[idx];
        name = 'f00:' + regValue;
    }
    u32 name;
    double fpr;
};

struct StackFrame {
    StackFrame() : spName('sp: '), sp(0), lrName('lr: '), lr(0) {};
    u32 spName;
    u32 sp;
    u32 lrName;
    u32 lr;
};

enum {
    EXCEPTION_FILE_VERSION = 4,
    EXCEPTION_FLAG_LOOSE_ARCHIVE_OVERRIDES_ENABLED = 1 << 0,
    EXCEPTION_FLAG_CUSTOM_CHARACTER_ENABLED = 1 << 1,
    EXCEPTION_MAX_TRACK_SZS_LENGTH = 64,
    EXCEPTION_MYSTUFF_DISABLED = 0,
    EXCEPTION_MYSTUFF_ENABLED = 1,
    EXCEPTION_MYSTUFF_MUSIC_ONLY = 2
};

struct CrashExtra {
    CrashExtra() : version(EXCEPTION_FILE_VERSION), sectionId(-1), pageId(-1), context(0), context2(0), flags(0),
                   looseOverrideFileCount(0), myStuffState(EXCEPTION_MYSTUFF_DISABLED) {
        lastTrackSzs[0] = '\0';
    }

    u32 version;
    s32 sectionId;
    s32 pageId;
    u32 context;
    u32 context2;
    u32 flags;
    u32 looseOverrideFileCount;
    u32 myStuffState;
    char lastTrackSzs[EXCEPTION_MAX_TRACK_SZS_LENGTH];
};

struct ExceptionFile {
    explicit ExceptionFile(const OS::Context& context);

    u32 magic;
    u32 region;
    u32 version;
    OS::Error error;
    GPR srr0;
    GPR srr1;
    GPR msr;
    GPR cr;
    GPR lr;
    GPR gprs[32];
    FPR fprs[32];
    FPR fpscr;
    StackFrame frames[10];
    CrashExtra extra;
    /*
        Appended in version 4, at the end so an older reader still finds everything it knew
        about. Without these a DSI is unreadable: srr0 says which instruction faulted but not
        which address it touched, and that is the only thing separating a null pointer from a
        wild one. Both are self labelled so they show up in a plain text dump of the file.
    */
    GPR dsisr;
    GPR dar;
};

}//namespace Debug
}//namespace Pulsar

#endif