#ifndef _PULSARIO_
#define _PULSARIO_

#include <kamek.hpp>
#include <core/rvl/devfs/isfs.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/egg/Thread.hpp>

namespace Pulsar {

const int maxFileCount = 100;
typedef char FileName[255];

enum IOType {
    IOType_RIIVO = 0,
    IOType_ISO = 1,
    IOType_DOLPHIN = 2,
    IOType_SD = 3
};

enum FileMode {
    FILE_MODE_NONE,
    FILE_MODE_READ,
    FILE_MODE_WRITE,
    FILE_MODE_READ_WRITE = FILE_MODE_READ | FILE_MODE_WRITE
};

class IO {
    struct CreateRequest {
        CreateRequest() : isFree(true) {};
        bool isFree;
        char path[IOS::ipcMaxPath];
    };
    static void CreateFolderAsync(CreateRequest* request);

public:
    static inline s32 OpenFix(const char* path, IOS::Mode mode) {
        asmVolatile(stwu sp, -0x0020 (sp););
        IOS::Open2ndInst(path, mode);
        register s32 result;
        asm(mr result, r3;);
        return result;
    }

    virtual bool OpenFile(const char* path, u32 mode) = 0;
    virtual bool CreateAndOpen(const char* path, u32 mode) = 0;
    virtual void GetCorrectPath(char* realPath, const char* path) const = 0;
    virtual bool RenameFile(const char* oldPath, const char* newPath) const = 0;

    virtual bool FolderExists(const char* path) const = 0;
    virtual bool CreateFolder(const char* path) = 0;
    virtual void ReadFolder(const char* path) = 0;

    static IO* sInstance;
    static IO* CreateInstance(IOType type, EGG::Heap* heap, EGG::TaskThread* const taskThread);
    //Same backends, but the result is NOT installed as sInstance. Needed by subsystems that
    //run on another thread and must not share fd/fileData/folderName with the global IO.
    static IO* CreatePrivateInstance(IOType type, EGG::Heap* heap, EGG::TaskThread* const taskThread);
    template<typename T>
    T* Alloc(u32 size) const { return EGG::Heap::alloc<T>(nw4r::ut::RoundUp(size, 0x20), 0x20, this->heap); }
    virtual s32 GetFileSize();

    bool OpenFileDirectly(const char* path, u32 mode);
    virtual s32 Read(u32 size, void* bufferIn);
    virtual void Seek(u32 offset) { IOS::Seek(this->fd, offset, IOS::SEEK_START); }
    virtual s32 Write(u32 length, const void* buffer);
    virtual s32 Overwrite(u32 length, const void* buffer);
    virtual void Close();

    const int GetFileCount() const { return this->fileCount; }
    const char* GetFolderName() const { return this->folderName; };
    //void RequestCreateFolder(const char* path); //up to 2 simultaneous
    virtual void CloseFolder();
    void PrintFullFilePath(char* path, const char* fileName) const {
        snprintf(path, IOS::ipcMaxPath, "%s/%s", &this->folderName, fileName);
    }
    void GetFolderFilePath(char* dest, u32 index) const {
        this->PrintFullFilePath(dest, reinterpret_cast<const char*>(&this->fileNames[index]));
    }
    const char* GetFileName(u32 index) const {
        return reinterpret_cast<const char*>(&this->fileNames[index]);
    }

    s32 ReadFolderFileFromPath(void* buffer, const char* path, u32 maxLength);

    s32 ReadFolderFile(void* bufferIn, u32 index, u32 maxLength) {
        char path[IOS::ipcMaxPath];
        this->GetFolderFilePath(path, index);
        return this->ReadFolderFileFromPath(bufferIn, path, maxLength);
    }
    s32 ReadFolderFileFromName(void* bufferIn, const char* name, u32 maxLength) {
        char path[IOS::ipcMaxPath];
        this->PrintFullFilePath(path, name);
        return this->ReadFolderFileFromName(bufferIn, path, maxLength);
    }

    const IOType type;

protected:
    /*
        Every member is initialised here, in declaration order. A backend is built with
        placement new on an EGG heap and that memory is not cleared: on Dolphin it happens to
        be zeroes, on a real console it is whatever IOS and the apploader left in MEM1.

        fileNames was the dangerous one. ReadFolder only assigns it inside its
        `folder_fd >= 0 && !isBusy` branch, so a garbage isBusy skipped the read and left the
        garbage pointer in place - and the CloseFolder right after it ran delete[] on that
        pointer, corrupting the heap. Everything that allocated or wrote a file afterwards,
        the save included, then died somewhere unrelated. fileSize matters too: GetFileSize
        only computes a size when the field is negative, so garbage was returned as the size.

        rr-pulsar initialises the same set (fileCount/fileNames in IO, isBusy/fd/fileSize in
        its IOSIO), which is why it never showed this.
    */
    IO(IOType type, EGG::Heap* heap, EGG::TaskThread* taskThread)
        : type(type), heap(heap), taskThread(taskThread), isBusy(false), fd(-1), fileSize(-1),
          fileCount(0), fileNames(nullptr) {
        filePath[0] = '\0';
        folderName[0] = '\0';
    }
    void Bind(const char* path) { strncpy(this->folderName, path, IOS::ipcMaxPath); }
    void CloseFile() { this->Close(); }

    EGG::Heap* heap;
    EGG::TaskThread* const taskThread;
    bool isBusy;
    s32 fd;
    s32 fileSize;
    char filePath[IOS::ipcMaxPath];
    char folderName[IOS::ipcMaxPath];
    u32 fileCount;
    IOS::IPCPath* fileNames;
    CreateRequest requests[2];
};



}//namespace Pulsar

#endif