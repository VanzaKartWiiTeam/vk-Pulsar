#include <kamek.hpp>
#include <Debug/BetaLog.hpp>
#include <Race/RoomContext.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

//ItemSlotData::sInstance is documented as a function but 0x809c3670 is the pointer itself.
extern "C" Item::ItemSlotData* itemSlotData;

namespace VanzaKart {
namespace Race {

/*
Thundercloud odds in small lobbies.

With a full room the cloud is a fine item; with four people or fewer it comes around often enough
that players report pulling it out of several boxes in a row, and there is nobody to pass it to.
So in worldwides and friend rooms with fewer than five racers its slice of the item table is cut
to a quarter and the freed probability goes to the Mushroom, which is the least disruptive place
to put it. Mogi mode is left completely alone - it is supposed to play vanilla.

This edits the processed table in place rather than the raw ItemSlot.bin, so it follows whatever
table the game (or the pack, or LE-CODE) actually ended up loading. The table is rebuilt whenever
the online player count changes, so Update() re-checks every frame and re-applies if the bytes
are no longer the ones it wrote.
*/

static const u32 kItemRows = 19;   //one row per ItemId
static const u32 kMaxColumns = 12; //one column per position
static const u32 kTCRow = THUNDER_CLOUD;
static const u32 kSinkRow = MUSHROOM; //where the probability taken off the cloud goes
static const u32 kKeepNumerator = 1;
static const u32 kKeepDenominator = 4;
static const u8 kMinPlayers = 5;

static u16 sBackup[kMaxColumns * kItemRows];
static const u16* sBackupTable = nullptr;
static u32 sBackupColumns = 0;
static u32 sAppliedChecksum = 0;
static bool sApplied = false;

static u32 Checksum(const u16* table, u32 count) {
    u32 sum = 0;
    for(u32 i = 0; i < count; ++i) sum = sum * 31u + table[i];
    return sum;
}

static bool ShouldReduce() {
    if(RoomContext::IsMogi()) return false;
    if(!RoomContext::IsOnline()) return false; //offline keeps vanilla odds
    const u8 players = RoomContext::GetRacePlayerCount();
    return players != 0 && players < kMinPlayers;
}

/*
A column is either a running total - every entry at least as large as the one before it, the last
entry being the grand total - or one independent weight per item. Which one you get depends on how
the slot table was processed, and the two layouts need opposite edits, so ask the data.
*/
static bool IsCumulative(const u16* column) {
    for(u32 i = 1; i < kItemRows; ++i) {
        if(column[i] < column[i - 1]) return false;
    }
    return column[kItemRows - 1] >= 100;
}

static void ReduceColumn(u16* column) {
    if(IsCumulative(column)) {
        const u16 share = column[kTCRow] - column[kTCRow - 1];
        if(share == 0) return;
        const u16 moved = share - (u16)(share * kKeepNumerator / kKeepDenominator);
        //Widening every entry from the sink up to (not including) the cloud grows the sink's
        //slice by exactly what the cloud loses, and leaves every other slice - and the grand
        //total - untouched.
        for(u32 i = kSinkRow; i < kTCRow; ++i) column[i] += moved;
    }
    else {
        const u16 share = column[kTCRow];
        if(share == 0) return;
        const u16 keep = (u16)(share * kKeepNumerator / kKeepDenominator);
        column[kTCRow] = keep;
        column[kSinkRow] += share - keep;
    }
}

static void Apply(Item::ItemSlotData* data) {
    u16* table = data->playerChances.probabilities;
    const u32 columns = data->playerChances.rowCount;
    if(table == nullptr || columns == 0 || columns > kMaxColumns) return;

    const u32 count = columns * kItemRows;
    for(u32 i = 0; i < count; ++i) sBackup[i] = table[i];
    sBackupTable = table;
    sBackupColumns = columns;

    for(u32 c = 0; c < columns; ++c) ReduceColumn(&table[c * kItemRows]);

    sAppliedChecksum = Checksum(table, count);
    sApplied = true;
    PUL_BETA_LOG("[TCOdds] cut to 1/%d, %d players, %d columns, cumulative=%d\n",
                 (int)kKeepDenominator, (int)RoomContext::GetRacePlayerCount(), (int)columns,
                 (int)IsCumulative(table));
}

static void Restore(Item::ItemSlotData* data) {
    u16* table = data->playerChances.probabilities;
    if(table != nullptr && table == sBackupTable) {
        const u32 count = sBackupColumns * kItemRows;
        for(u32 i = 0; i < count; ++i) table[i] = sBackup[i];
        PUL_BETA_LOG("[TCOdds] vanilla odds restored\n");
    }
    sApplied = false;
    sBackupTable = nullptr;
}

static void Update() {
    Item::ItemSlotData* data = itemSlotData;
    if(data == nullptr) {
        sApplied = false;
        sBackupTable = nullptr;
        return;
    }

    const u16* table = data->playerChances.probabilities;
    const u32 columns = data->playerChances.rowCount;

    if(sApplied) {
        //The game rebuilds this table when the online player count changes. If the bytes are no
        //longer the ones we left behind, our backup describes a table that does not exist any
        //more - drop it rather than writing stale numbers over a fresh table.
        const bool stillOurs = table != nullptr && table == sBackupTable && columns == sBackupColumns
                            && columns <= kMaxColumns
                            && Checksum(table, columns * kItemRows) == sAppliedChecksum;
        if(!stillOurs) {
            sApplied = false;
            sBackupTable = nullptr;
        }
    }

    const bool wanted = ShouldReduce();
    if(wanted && !sApplied) Apply(data);
    else if(!wanted && sApplied) Restore(data);
}

static PageLoadHook ThunderCloudOddsPage(Update);
static RaceLoadHook ThunderCloudOddsRace(Update);
static RaceFrameHook ThunderCloudOddsFrame(Update);

}  // namespace Race
}  // namespace VanzaKart
