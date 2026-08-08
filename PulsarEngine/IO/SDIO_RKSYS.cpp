#include <IO/SDIO_RKSYS.hpp>
#include <PulsarSystem.hpp>
#include <IO/SDIO.hpp>
#include <VanzaKartChannel.hpp>
#include <core/rvl/os/OS.hpp>

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
        BootHook::Exec();
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

void SDIO_RKSYS_CreatePath() {
    char path[64];

    GetRKSYSIO()->CreateFolder("/riivolution");
    GetRKSYSIO()->CreateFolder("/riivolution/save");
    snprintf(path, 64, "/riivolution/save/%s", useRedirectedRKSYS() ? "VanzaWFC2" : "VanzaWFC");
    GetRKSYSIO()->CreateFolder(path);
    snprintf(path, 64, "/riivolution/save/%s/RMC%c", useRedirectedRKSYS() ? "VanzaWFC2" : "VanzaWFC", GetRegion());
    GetRKSYSIO()->CreateFolder(path);
}

NandUtils::Result SDIO_ReadRKSYS(NandMgr* nm, void* buffer, u32 size, u32 offset, bool r7)  // 8052c0b0
{
    if (IsRKSYSRedirectionReady() && !readingNAND) {
        bool res;
        char path[64];
        SDIO_RKSYS_path(path, sizeof(path));
        int mode = GetRKSYSIO()->type == IOType_DOLPHIN ? FILE_MODE_READ : O_RDONLY;
        res = GetRKSYSIO()->OpenFile(path, mode);
        if (!res) {
            GetRKSYSIO()->Close();
            return NandUtils::NAND_RESULT_NOEXISTS;
        }

        GetRKSYSIO()->Seek(offset);
        GetRKSYSIO()->Read(size, buffer);
        GetRKSYSIO()->Close();

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
        bool res;
        char path[64];
        SDIO_RKSYS_path(path, sizeof(path));
        int mode = GetRKSYSIO()->type == IOType_DOLPHIN ? FILE_MODE_READ : O_RDONLY;
        res = GetRKSYSIO()->OpenFile(path, mode);
        if (!res) {
            GetRKSYSIO()->Close();
            NandUtils::Result cres = SDIO_CreateRKSYS(nm, length);
            return cres;
        }

        s32 size = GetRKSYSIO()->GetFileSize();
        GetRKSYSIO()->Close();

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
        /* After copying an existing RKSYS, skip the game's first blank-save write. */
        if (!isNewNotSeparateSavegame) {
            bool res;
            char path[64];
            SDIO_RKSYS_path(path, sizeof(path));
            int mode = GetRKSYSIO()->type == IOType_DOLPHIN ? FILE_MODE_READ_WRITE : O_RDWR;
            res = GetRKSYSIO()->OpenFile(path, mode);

            if (!res) {
                NandUtils::Result nres = SDIO_CreateRKSYS(nm, 0);
                if (nres != NandUtils::NAND_RESULT_OK) {
                    return nres;
                }
                res = GetRKSYSIO()->OpenFile(path, O_RDWR);
                if (!res) {
                    return NandUtils::NAND_RESULT_NOEXISTS;
                }

                if (isNewNotSeparateSavegame) {
                    isNewNotSeparateSavegame = false;
                    GetRKSYSIO()->Close();
                    return NandUtils::NAND_RESULT_OK;
                }
            }

            GetRKSYSIO()->Seek(offset);
            GetRKSYSIO()->Write(size, buffer);
            GetRKSYSIO()->Close();
        } else {
            isNewNotSeparateSavegame = false;
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
        /* Create each folder level explicitly; SDIO does not create parent directories. */
        SDIO_RKSYS_CreatePath();

        bool res;
        char path[64];
        SDIO_RKSYS_path(path, sizeof(path));

        int mode = GetRKSYSIO()->type == IOType_DOLPHIN ? O_RDWR : IOS::MODE_WRITE;

        res = GetRKSYSIO()->CreateAndOpen(path, mode);

        if (!res) {
            return NandUtils::NAND_RESULT_ALLOC_FAILED;
        }

        /* If not separate savegame, copy existing NAND one */
        if (!useRedirectedRKSYS()) {
            isNewNotSeparateSavegame = true;

            /* Force SDIO_ReadRKSYS through the NAND path while copying the original save. */
            readingNAND = true;

            const int rksys_size = 0x2BC000;
            const int chunk_size = 1024*10;

            char chunk[chunk_size];
            int read = 0;
            int i = 0;

            while (read < rksys_size) {
                GetRKSYSIO()->Close();
                NandUtils::Result r = SDIO_ReadRKSYS(nm, (void*)chunk, chunk_size, chunk_size * i, true);

                GetRKSYSIO()->OpenFile(path, mode);

                if (r != NandUtils::NAND_RESULT_OK) {
                    GetRKSYSIO()->Close();
                    readingNAND = false;
                    return r;
                }

                if (r == NandUtils::NAND_RESULT_NOEXISTS) {
                    break;
                }

                GetRKSYSIO()->Seek(chunk_size * i);
                GetRKSYSIO()->Write(chunk_size, (void*)chunk);

                i++;
                read += chunk_size;
            }

            readingNAND = false;
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
        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x0030(sp););
        return nm->DeleteRKSYS2ndInst(length, r5);
    }
}
kmBranch(0x8052c7e4, SDIO_DeleteRKSYS);
}  // namespace Pulsar
