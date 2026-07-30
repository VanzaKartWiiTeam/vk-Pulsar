#include <kamek.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/rvl/os/OS.hpp>

// Diagnostica temporanea per il crash in RadioButtonControl::Load (DSI, SRR0=0x8063CA74).
// Catena verificata: ExpSection::CreateAndInitPage -> Page::Init -> VSSettings::OnInit ->
// Menu::OnInit -> VSSettings::CreateControl -> RadioButtonControl::Load ->
// UIAnimator::GetAnimationGroupById(4) -> AnimationGroup::PlayAnimationAtFrame(0, frame),
// dove animations[0] e' NULL.
//
// Il primo round ha mostrato che le prime 4 opzioni (OptionClass0..3) allocano il loro
// AnimTransform in MEM2 a passo costante di 0x3164, e che la quinta (OptionLevel0) riceve NULL.
// Questo round misura lo spazio allocabile dei due root heap attorno a ogni caricamento e
// intercetta la singola call site di UIAnimator::LoadAnimation, per distinguere fra
// esaurimento memoria e fallimento della lookup.
//
// Nessuno degli indirizzi agganciati ha rilocazioni REL (verificato sulla tabella del modulo),
// quindi gli hook non vengono sovrascritti da OSLink. Da rimuovere prima di una release.

namespace Pulsar {
namespace Diag {

typedef void* (*GetAnimGroupFunc)(const void* animator, u32 id);
typedef void (*CtrlLoadFunc)(void* loader, const char* folder, const char* brctr, const char* variant, const char** bmg);
typedef u32 (*LoadAnimFunc)(void* animator, u32 a, u32 b, u32 c, u32 d, u32 e, float frame);

static const GetAnimGroupFunc realGetAnimGroup = reinterpret_cast<GetAnimGroupFunc>(0x8063c820);
static const CtrlLoadFunc realCtrlLoad = reinterpret_cast<CtrlLoadFunc>(0x805c2c60);
static const LoadAnimFunc realLoadAnim = reinterpret_cast<LoadAnimFunc>(0x8063c714);

static const char* lastVariant = "?";
static bool inControlLoad = false;
static u32 loadIndex = 0;
static u32 animOk = 0;
static u32 animFailed = 0;

static u32 Allocatable(EGG::Heap* heap) {
    if(heap == nullptr) return 0;
    return heap->getAllocatableSize(0x20);
}

// bl a 0x80605a44: ControlLoader::Load per ogni controllo-opzione del radio button.
// I root heap di RKSystem riportano sempre 0 (sono interamente suddivisi in figli, come si vede
// dai "Candidate 1/2: available=0" nei log di LooseArchiveOverrides): gli heap veri sono quelli
// della scena. structsMem1/2 servono controlli e pagine, archiveHeapMem1/2 sono dimensionati
// sugli archivi caricati.
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
        OS::Report("[VK RADIO] #%u %s: GameScene::GetCurrent() e' NULL\n", loadIndex++, variant);
        return;
    }
    const u32 structs2After = Allocatable(structsMem2);
    OS::Report("[VK RADIO] #%u %s/%s %s | anim ok=%u ko=%u | structMEM2 %u -> %u (-%d) structMEM1=%u archMEM1=%u archMEM2=%u\n",
        loadIndex++, folder, brctr, variant, animOk, animFailed,
        structs2Before, structs2After, static_cast<int>(structs2Before - structs2After),
        Allocatable(scene->structsMem1),
        Allocatable(scene->archiveHeapMem1), Allocatable(scene->archiveHeapMem2));
}
kmCall(0x80605a44, LogCtrlLoad);

// bl a 0x805c31b4: unica call site in tutto il REL di UIAnimator::LoadAnimation (0x8063c714).
// Gated dal flag, altrimenti stampa ogni animazione di ogni controllo della UI.
static u32 LogLoadAnim(void* animator, u32 a, u32 b, u32 c, u32 d, u32 e, float frame) {
    const u32 ret = realLoadAnim(animator, a, b, c, d, e, frame);
    if(inControlLoad) {
        if(ret == 0) {
            ++animFailed;
            // c e' il puntatore al nome del .brlan dentro l'archivio: stamparlo solo sui
            // fallimenti tiene il log leggibile e dice quale file non e' stato agganciato.
            OS::Report("[VK ANIM] KO %s group=%u anim=%u name=%s\n",
                lastVariant, a, b, reinterpret_cast<const char*>(c));
        }
        else ++animOk;
    }
    return ret;
}
kmCall(0x805c31b4, LogLoadAnim);

// bl a 0x80605a94: UIAnimator::GetAnimationGroupById(4), subito prima del crash.
// Dumpa tutti i gruppi, non solo quello richiesto: se il controllo non ha 5 gruppi la
// GetAnimationGroupById legge fuori array (e' groups + id*0x44, senza controllo di limiti).
static void* LogGetAnimGroup(const void* animator, u32 id) {
    void* group = realGetAnimGroup(animator, id);
    const u8* const groupsArray = *reinterpret_cast<const u8* const*>(animator);
    if(groupsArray == nullptr) {
        OS::Report("[VK RADIO] %s: array dei gruppi NULL\n", lastVariant);
        return group;
    }
    // Stampa solo quando qualcosa e' NULL: sui controlli sani il dump non aggiunge nulla.
    const u8* const anims = *reinterpret_cast<const u8* const*>(group);
    if(anims != nullptr && *reinterpret_cast<const u32*>(anims) != 0) return group;

    OS::Report("[VK RADIO] %s: gruppo %u rotto, animator=%08x groups=%08x group=%08x\n",
        lastVariant, id, reinterpret_cast<u32>(animator),
        reinterpret_cast<u32>(groupsArray), reinterpret_cast<u32>(group));
    for(u32 i = 0; i <= id; ++i) {
        const u8* const g = groupsArray + i * 0x44;
        const u8* const gAnims = *reinterpret_cast<const u8* const*>(g);
        OS::Report("[VK RADIO]   group%u anims=%08x anims[0]=%08x\n",
            i, reinterpret_cast<u32>(gAnims),
            gAnims == nullptr ? 0 : *reinterpret_cast<const u32*>(gAnims));
    }
    return group;
}
kmCall(0x80605a94, LogGetAnimGroup);

}//namespace Diag
}//namespace Pulsar
