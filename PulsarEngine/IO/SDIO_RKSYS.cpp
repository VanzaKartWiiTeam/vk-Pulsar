#include <IO/SDIO_RKSYS.hpp>
#include <PulsarSystem.hpp>
#include <IO/SDIO.hpp>
#include <VanzaKartChannel.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/os/OSthread.hpp>

namespace Pulsar {

static bool readingNAND = false;
static bool isNewNotSeparateSavegame = false;

/*
    The redirection runs on the game's NAND thread while Pulsar's task thread writes ghosts,
    the leaderboard and the settings through IO::sInstance. Both used to be the same object,
    and an IO backend keeps exactly one open file (fd / SDIO::fileData) plus one bound folder
    name, so the two threads clobbered each other: an rksys.dat save at the end of a time
    trial closes the .rkg the task thread just opened, and SDIO_RKSYS_CreatePath's
    CreateFolder calls rebind folderName to /riivolution/save/..., which is what
    CreateAndSaveFiles used to build the ghost path from.

    So keep a private backend here, of the same type as the global one. It is only ever
    touched by this file.
*/
static IO* rksysIO = nullptr;

static IO* GetRKSYSIO() {
    if(rksysIO == nullptr) {
        const System* system = System::sInstance;
        const IO* global = IO::sInstance;
        rksysIO = IO::CreatePrivateInstance(global->type, system->heap, system->taskThread);
    }
    return rksysIO;
}

char GetRegion() {
    return *(char*)0x80000003;
}

#define RKSYS_LOG_PATH "/VanzaKartRKSYS.log"

void SDIO_RKSYS_path(char* path, u32 pathlen);

static IO* logIO = nullptr;
static bool logging = false;
static bool loggedHeader = false;

/* The errno lives in the launcher's SD driver, so it only exists on the SD backend. */
static s32 RKSYSErrno() {
    if(IO::sInstance == nullptr || IO::sInstance->type != IOType_SD) return -1;
    return SDIO_LastErrno();
}

static void RKSYSLogLine(const char* line) {
    if(logging) return;
    logging = true;

    const System* system = System::sInstance;
    if(logIO == nullptr && system != nullptr && IO::sInstance != nullptr) {
        logIO = IO::CreatePrivateInstance(IO::sInstance->type, system->heap, system->taskThread);
    }

    if(logIO != nullptr) {
        if(!logIO->OpenFile(RKSYS_LOG_PATH, FILE_MODE_READ_WRITE)) {
            logIO->Close();
            logIO->CreateAndOpen(RKSYS_LOG_PATH, FILE_MODE_READ_WRITE);
        }
        const s32 size = logIO->GetFileSize();
        if(size > 0) logIO->Seek(size);
        logIO->Write(strlen(line), line);
        logIO->Close();
    }

    OS::Report("%s", line);
    logging = false;
}

/* Written once, on the first hook that runs, so the log says what configuration produced it. */
static void RKSYSLogHeader() {
    if(loggedHeader) return;
    loggedHeader = true;

    char path[64];
    SDIO_RKSYS_path(path, sizeof(path));

    char line[256];
    snprintf(line, sizeof(line),
             "[VK] RKSYS: backend %d, region %c, separate savegame %d, boot fallback %d\n"
             "[VK] RKSYS: path %s\n",
             IO::sInstance == nullptr ? -1 : (int)IO::sInstance->type, GetRegion(),
             NewChannel_UseSeparateSavegame() ? 1 : 0, BootHook::executedFromFallback ? 1 : 0,
             path);
    RKSYSLogLine(line);
}

/*
    The game loads RKSYS from a task running on its own NAND thread: the REL function at
    0x8054484C (PAL) calls NandMgr::CheckRKSYSLength with length 0x2BC000 and then
    NandMgr::ReadRKSYS, both of which land in the hooks below. That task is dispatched
    before the main thread reaches Pulsar's boot point, so IO::sInstance is still null.

    On the Riivolution route IsNewChannel() is false there, the vanilla NAND branch runs
    and nothing ever noticed. Through the launcher/channel the redirected branch is taken
    instead and the first IO::sInstance-> call reads the vtable pointer off address 0,
    which is the boot crash (DSI, DAR=0) right after the bootstrap screen.

    BootHook::Exec is idempotent, so bring Pulsar up here rather than skipping the
    redirection: it is the same fallback ArchiveDecompressSafety uses for the Font.szs
    decompress, only early enough for the save load. If IO still is not up afterwards,
    report the redirection as unavailable so the caller keeps the vanilla NAND behaviour
    instead of dereferencing null.
*/
static bool IsRKSYSRedirectionReady() {
    if(!IsNewChannel()) return false;
    if(IO::sInstance == nullptr) {
#ifdef VKDIAG
        if(!BootHook::executed) Pulsar::Diag::Step("boot point (RKSYS read)", 0);
#endif
        BootHook::Exec();

        /*
            Exec marks itself as done before it runs the hooks, so when the main thread is
            already inside it the call above returns right away, with System::Init still
            halfway through and IO::sInstance not assigned yet. Falling back to the NAND save
            in that window is worse than waiting: CheckRKSYSLength would answer from the NAND,
            where the vanilla save exists, and the read straight after it would land on the SD
            save, which may not exist yet - the game then gets a "file not found" one call
            after an OK and reports that it could not read the Wii system memory.

            So wait for whoever is initialising Pulsar to publish IO. The loop ends as soon as
            the pointer appears; the bound only exists so a genuine init failure still returns
            instead of spinning forever.
        */
        for(u32 i = 0; IO::sInstance == nullptr && i < 0x100000; ++i) OS::YieldThread();

        if(IO::sInstance == nullptr) {
            OS::Report("[VK] RKSYS: IO not initialised, falling back to the NAND save\n");
            return false;
        }
    }
    return true;
}

/*
    When separate savegame is disabled, use the save from the VanzaWFC folder.
    If it does not exist, copy the NAND save there and use it from then on.

    When enabled, use the VanzaWFC2 folder and create a blank save there if needed.
*/

bool useRedirectedRKSYS() {
    return NewChannel_UseSeparateSavegame() && IsNewChannel();
}

/* Must be preallocated */
void SDIO_RKSYS_path(char* path, u32 pathlen) {
    snprintf(path, pathlen, "/riivolution/save/%s/RMC%c/rksys.dat", useRedirectedRKSYS() ? "VanzaWFC2" : "VanzaWFC", GetRegion());
}

/* Logs the ones that fail; a missing folder is why the save file itself cannot be created. */
static void CreateFolderLogged(const char* path) {
    if(GetRKSYSIO()->CreateFolder(path)) return;

    char line[160];
    snprintf(line, sizeof(line), "[VK] RKSYS: CreateFolder(%s) failed (errno %d)\n",
             path, (int)RKSYSErrno());
    RKSYSLogLine(line);
}

void SDIO_RKSYS_CreatePath() {
    char path[64];

    CreateFolderLogged("/riivolution");
    CreateFolderLogged("/riivolution/save");
    snprintf(path, 64, "/riivolution/save/%s", useRedirectedRKSYS() ? "VanzaWFC2" : "VanzaWFC");
    CreateFolderLogged(path);
    snprintf(path, 64, "/riivolution/save/%s/RMC%c", useRedirectedRKSYS() ? "VanzaWFC2" : "VanzaWFC", GetRegion());
    CreateFolderLogged(path);
}

NandUtils::Result SDIO_ReadRKSYS(NandMgr* nm, void* buffer, u32 size, u32 offset, bool r7)  // 8052c0b0
{
    if (IsRKSYSRedirectionReady() && !readingNAND) {
        RKSYSLogHeader();

        bool res;
        char path[64];
        char line[160];
        SDIO_RKSYS_path(path, sizeof(path));
        int mode = GetRKSYSIO()->type == IOType_DOLPHIN ? FILE_MODE_READ : O_RDONLY;
        res = GetRKSYSIO()->OpenFile(path, mode);
        if (!res) {
            snprintf(line, sizeof(line), "[VK] RKSYS: read %d@%d, open failed (errno %d)\n",
                     (int)size, (int)offset, (int)RKSYSErrno());
            RKSYSLogLine(line);
            GetRKSYSIO()->Close();
            return NandUtils::NAND_RESULT_NOEXISTS;
        }

        const s32 fileSize = GetRKSYSIO()->GetFileSize();
        GetRKSYSIO()->Seek(offset);
        const s32 read = GetRKSYSIO()->Read(size, buffer);
        GetRKSYSIO()->Close();

        snprintf(line, sizeof(line), "[VK] RKSYS: read %d@%d -> %d (file %d)\n",
                 (int)size, (int)offset, (int)read, (int)fileSize);
        RKSYSLogLine(line);

        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        return nm->ReadRKSYS2ndInst(buffer, size, offset, r7);
    }
}
kmBranch(0x8052c0b0, SDIO_ReadRKSYS);

NandUtils::Result SDIO_CheckRKSYSLength(NandMgr* nm, u32 length)  // 8052c20c
{
    if (IsRKSYSRedirectionReady()) {
        RKSYSLogHeader();

        bool res;
        char path[64];
        char line[160];
        SDIO_RKSYS_path(path, sizeof(path));
        int mode = GetRKSYSIO()->type == IOType_DOLPHIN ? FILE_MODE_READ : O_RDONLY;
        res = GetRKSYSIO()->OpenFile(path, mode);
        if (!res) {
            snprintf(line, sizeof(line), "[VK] RKSYS: check %d, no save yet (errno %d), creating\n",
                     (int)length, (int)RKSYSErrno());
            RKSYSLogLine(line);
            GetRKSYSIO()->Close();
            NandUtils::Result cres = SDIO_CreateRKSYS(nm, length);
            snprintf(line, sizeof(line), "[VK] RKSYS: check -> create returned %d\n", (int)cres);
            RKSYSLogLine(line);
            return cres;
        }

        s32 size = GetRKSYSIO()->GetFileSize();
        GetRKSYSIO()->Close();

        snprintf(line, sizeof(line), "[VK] RKSYS: check %d, file is %d -> OK\n", (int)length, (int)size);
        RKSYSLogLine(line);

        if (size == length) {
            return NandUtils::NAND_RESULT_OK;
        } else {
            return NandUtils::NAND_RESULT_OK;
        }
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        return nm->CheckRKSYSLength2ndInst(length);
    }
}
kmBranch(0x8052c20c, SDIO_CheckRKSYSLength);

NandUtils::Result SDIO_WriteToRKSYS(NandMgr* nm, const void* buffer, u32 size, u32 offset, bool r7)  // 8052c2d0
{
    if (IsRKSYSRedirectionReady()) {
        RKSYSLogHeader();

        char line[160];

        /* After copying an existing RKSYS, skip the game's first blank-save write. */
        if (!isNewNotSeparateSavegame) {
            bool res;
            char path[64];
            SDIO_RKSYS_path(path, sizeof(path));
            int mode = GetRKSYSIO()->type == IOType_DOLPHIN ? FILE_MODE_READ_WRITE : O_RDWR;
            res = GetRKSYSIO()->OpenFile(path, mode);

            if (!res) {
                snprintf(line, sizeof(line), "[VK] RKSYS: write %d@%d, open failed (errno %d), creating\n",
                         (int)size, (int)offset, (int)RKSYSErrno());
                RKSYSLogLine(line);

                NandUtils::Result nres = SDIO_CreateRKSYS(nm, 0);
                if (nres != NandUtils::NAND_RESULT_OK) {
                    snprintf(line, sizeof(line), "[VK] RKSYS: write -> create failed with %d\n", (int)nres);
                    RKSYSLogLine(line);
                    return nres;
                }
                res = GetRKSYSIO()->OpenFile(path, O_RDWR);
                if (!res) {
                    snprintf(line, sizeof(line), "[VK] RKSYS: write, reopen after create failed (errno %d)\n",
                             (int)RKSYSErrno());
                    RKSYSLogLine(line);
                    return NandUtils::NAND_RESULT_NOEXISTS;
                }

                if (isNewNotSeparateSavegame) {
                    isNewNotSeparateSavegame = false;
                    GetRKSYSIO()->Close();
                    RKSYSLogLine("[VK] RKSYS: write skipped, the copied save stays as it is\n");
                    return NandUtils::NAND_RESULT_OK;
                }
            }

            GetRKSYSIO()->Seek(offset);
            const s32 written = GetRKSYSIO()->Write(size, buffer);
            GetRKSYSIO()->Close();

            snprintf(line, sizeof(line), "[VK] RKSYS: write %d@%d -> %d (errno %d)\n",
                     (int)size, (int)offset, (int)written,
                     written == (s32)size ? 0 : (int)RKSYSErrno());
            RKSYSLogLine(line);
        } else {
            isNewNotSeparateSavegame = false;
            RKSYSLogLine("[VK] RKSYS: write skipped once, right after copying the NAND save\n");
        }

        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        return nm->WriteToRKSYS2ndInst(buffer, size, offset, r7);
    }
}
kmBranch(0x8052c2d0, SDIO_WriteToRKSYS);

NandUtils::Result SDIO_CreateRKSYS(NandMgr* nm, u32 length)  // 8052c68c
{
    /* Separate savegame creates an empty file; shared savegame copies NAND RKSYS. */

    if (IsRKSYSRedirectionReady()) {
        RKSYSLogHeader();

        /* Create each folder level explicitly; SDIO does not create parent directories. */
        SDIO_RKSYS_CreatePath();

        bool res;
        char path[64];
        char line[160];
        SDIO_RKSYS_path(path, sizeof(path));

        int mode = GetRKSYSIO()->type == IOType_DOLPHIN ? O_RDWR : IOS::MODE_WRITE;

        res = GetRKSYSIO()->CreateAndOpen(path, mode);

        if (!res) {
            snprintf(line, sizeof(line), "[VK] RKSYS: create failed, CreateAndOpen(%s) (errno %d)\n",
                     path, (int)RKSYSErrno());
            RKSYSLogLine(line);
            return NandUtils::NAND_RESULT_ALLOC_FAILED;
        }

        snprintf(line, sizeof(line), "[VK] RKSYS: created %s, copy from NAND %d\n",
                 path, useRedirectedRKSYS() ? 0 : 1);
        RKSYSLogLine(line);

        /* If not separate savegame, copy existing NAND one */
        if (!useRedirectedRKSYS()) {
            isNewNotSeparateSavegame = true;

            /* Force SDIO_ReadRKSYS through the NAND path while copying the original save. */
            readingNAND = true;

            const int rksys_size = 0x2BC000;
            const int chunk_size = 1024*10;

            /*
                The chunk lives on the heap, not on the stack. These hooks run on the game's
                NAND thread, whose stack is nowhere near the 10KB this buffer takes, and
                overflowing it corrupts whatever sits below the stack instead of failing here.
            */
            char* chunk = EGG::Heap::alloc<char>(chunk_size, 0x20, System::sInstance->heap);
            if (chunk == nullptr) {
                GetRKSYSIO()->Close();
                readingNAND = false;
                isNewNotSeparateSavegame = false;
                RKSYSLogLine("[VK] RKSYS: create failed, no heap for the copy buffer\n");
                return NandUtils::NAND_RESULT_ALLOC_FAILED;
            }

            int read = 0;
            int i = 0;
            NandUtils::Result copyResult = NandUtils::NAND_RESULT_OK;

            while (read < rksys_size) {
                GetRKSYSIO()->Close();
                NandUtils::Result r = SDIO_ReadRKSYS(nm, (void*)chunk, chunk_size, chunk_size * i, true);

                /*
                    A console with no Mario Kart Wii save data on NAND is a normal case, and
                    there is nothing to copy: keep the empty file so the game creates a fresh
                    save in it. This has to be checked before the generic error below, which
                    would otherwise swallow it and report the create as failed.
                */
                if (r == NandUtils::NAND_RESULT_NOEXISTS) {
                    snprintf(line, sizeof(line), "[VK] RKSYS: no NAND save to copy, stopped at chunk %d\n", i);
                    RKSYSLogLine(line);
                    break;
                }

                if (r != NandUtils::NAND_RESULT_OK) {
                    snprintf(line, sizeof(line), "[VK] RKSYS: NAND read failed with %d at chunk %d\n",
                             (int)r, i);
                    RKSYSLogLine(line);
                    copyResult = r;
                    break;
                }

                if (!GetRKSYSIO()->OpenFile(path, mode)) {
                    snprintf(line, sizeof(line), "[VK] RKSYS: reopen during copy failed at chunk %d (errno %d)\n",
                             i, (int)RKSYSErrno());
                    RKSYSLogLine(line);
                    copyResult = NandUtils::NAND_RESULT_ALLOC_FAILED;
                    break;
                }

                GetRKSYSIO()->Seek(chunk_size * i);
                const s32 written = GetRKSYSIO()->Write(chunk_size, (void*)chunk);
                if (written != chunk_size) {
                    snprintf(line, sizeof(line), "[VK] RKSYS: copy write %d of %d at chunk %d (errno %d)\n",
                             (int)written, chunk_size, i, (int)RKSYSErrno());
                    RKSYSLogLine(line);
                    copyResult = NandUtils::NAND_RESULT_ALLOC_FAILED;
                    break;
                }

                i++;
                read += chunk_size;
            }

            EGG::Heap::free(chunk, System::sInstance->heap);
            readingNAND = false;

            if (copyResult != NandUtils::NAND_RESULT_OK) {
                GetRKSYSIO()->Close();
                isNewNotSeparateSavegame = false;
                return copyResult;
            }

            snprintf(line, sizeof(line), "[VK] RKSYS: copied %d bytes from the NAND save\n", read);
            RKSYSLogLine(line);
        }

        GetRKSYSIO()->Close();
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        return nm->CreateRKSYS2ndInst(length);
    }

    return NandUtils::NAND_RESULT_OK;
}
kmBranch(0x8052c68c, SDIO_CreateRKSYS);

NandUtils::Result SDIO_DeleteRKSYS(NandMgr* nm, u32 length, bool r5)  // 8052c7e4
{
    if (IsRKSYSRedirectionReady()) {
        /* The SD backend has no delete hook here; the next write will replace the file. */
        RKSYSLogLine("[VK] RKSYS: delete requested, kept the file\n");
        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x0030(sp););
        return nm->DeleteRKSYS2ndInst(length, r5);
    }
}
kmBranch(0x8052c7e4, SDIO_DeleteRKSYS);
}  // namespace Pulsar
