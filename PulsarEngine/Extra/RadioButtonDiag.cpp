#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/rvl/os/OS.hpp>

namespace Pulsar {
namespace Diag {

typedef void* (*GetAnimGroupFunc)(const void* animator, u32 id);
typedef void (*CtrlLoadFunc)(void* loader, const char* folder, const char* brctr, const char* variant, const char** bmg);
typedef u32 (*LoadAnimFunc)(void* animator, u32 a, u32 b, u32 c, u32 d, u32 e, float frame);

kmRuntimeUse(0x8063c820);
kmRuntimeUse(0x805c2c60);
kmRuntimeUse(0x8063c714);
static const GetAnimGroupFunc realGetAnimGroup = reinterpret_cast<GetAnimGroupFunc>(kmRuntimeAddr(0x8063c820));
static const CtrlLoadFunc realCtrlLoad = reinterpret_cast<CtrlLoadFunc>(kmRuntimeAddr(0x805c2c60));
static const LoadAnimFunc realLoadAnim = reinterpret_cast<LoadAnimFunc>(kmRuntimeAddr(0x8063c714));

static const char* lastVariant = "?";
static bool inControlLoad = false;
static u32 loadIndex = 0;
static u32 animOk = 0;
static u32 animFailed = 0;

static u32 Allocatable(EGG::Heap* heap) {
    if(heap == nullptr) return 0;
    return heap->getAllocatableSize(0x20);
}

static void LogCtrlLoad(void* loader, const char* folder, const char* brctr, const char* variant, const char** bmg) {
    lastVariant = variant;

    const GameScene* const scene = GameScene::GetCurrent();
    EGG::Heap* structsMem2 = nullptr;
    if(scene != nullptr) structsMem2 = scene->structsMem2;
    const u32 structs2Before = Allocatable(structsMem2);

    animOk = 0;
    animFailed = 0;
    inControlLoad = true;
    realCtrlLoad(loader, folder, brctr, variant, bmg);
    inControlLoad = false;

    if(scene == nullptr) {
        return;
    }
    const u32 structs2After = Allocatable(structsMem2);
        loadIndex++, folder, brctr, variant, animOk, animFailed,
        structs2Before, structs2After, static_cast<int>(structs2Before - structs2After),
        Allocatable(scene->structsMem1),
        Allocatable(scene->archiveHeapMem1), Allocatable(scene->archiveHeapMem2));
}
kmCall(0x80605a44, LogCtrlLoad);

static u32 LogLoadAnim(void* animator, u32 a, u32 b, u32 c, u32 d, u32 e, float frame) {
    const u32 ret = realLoadAnim(animator, a, b, c, d, e, frame);
    if(inControlLoad) {
        if(ret == 0) {
            ++animFailed;
            OS::Report("[VK ANIM] KO %s group=%u anim=%u name=%s\n",
                lastVariant, a, b, reinterpret_cast<const char*>(c));
        }
        else ++animOk;
    }
    return ret;
}
kmCall(0x805c31b4, LogLoadAnim);

static void* LogGetAnimGroup(const void* animator, u32 id) {
    void* group = realGetAnimGroup(animator, id);
    const u8* const groupsArray = *reinterpret_cast<const u8* const*>(animator);
    if(groupsArray == nullptr) {
        return group;
    }
    const u8* const anims = *reinterpret_cast<const u8* const*>(group);
    if(anims != nullptr && *reinterpret_cast<const u32*>(anims) != 0) return group;

        lastVariant, id, reinterpret_cast<u32>(animator),
        reinterpret_cast<u32>(groupsArray), reinterpret_cast<u32>(group));
    for(u32 i = 0; i <= id; ++i) {
        const u8* const g = groupsArray + i * 0x44;
        const u8* const gAnims = *reinterpret_cast<const u8* const*>(g);
            i, reinterpret_cast<u32>(gAnims),
            gAnims == nullptr ? 0 : *reinterpret_cast<const u32*>(gAnims));
    }
    return group;
}
kmCall(0x80605a94, LogGetAnimGroup);

}//namespace Diag
}//namespace Pulsar
