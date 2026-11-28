#include "global.h"
#include "event_data.h"
#include "field_weather.h"
#include "pokemon.h"
#include "random.h"
#include "treasure_pickup.h"
#include "config/treasure_pickup.h"
#include "constants/items.h"
#include "constants/map_groups.h"
#include "constants/species.h"
#include "constants/weather.h"

// BestRomHackEver Core #42: the treasure ZIGZAGOON. It can't gain EXP (battle EXP, RARE CANDY and the Daycare check
// MON_DATA_IS_TREASURE) and its Pickup finds only the type-boosting items below.

static enum Item GetTreasureWeatherItem(u32 weather)
{
#define TREASURE_WEATHER(w, item) if (weather == (w)) return (item);
    TREASURE_PICKUP_WEATHER_ITEMS
#undef TREASURE_WEATHER
    return ITEM_NONE;
}

static enum Item GetTreasureMapItem(void)
{
    u32 mapGroup = gSaveBlock1Ptr->location.mapGroup;
    u32 mapNum = gSaveBlock1Ptr->location.mapNum;

    if (mapGroup == MAP_GROUP(MAP_ROUTE104) && mapNum == MAP_NUM(MAP_ROUTE104))
        return gSaveBlock1Ptr->pos.y >= TREASURE_ROUTE104_SOUTH_MIN_Y ? TREASURE_ROUTE104_SOUTH_ITEM : ITEM_NONE;
#define TREASURE_MAP(map, item) if (mapGroup == MAP_GROUP(map) && mapNum == MAP_NUM(map)) return (item);
    TREASURE_PICKUP_MAP_ITEMS
#undef TREASURE_MAP
    return ITEM_NONE;
}

void TreasurePickup_TryFindItem(struct Pokemon *mon)
{
    enum Species species = GetMonData(mon, MON_DATA_SPECIES_OR_EGG);
    enum Item item;

    if (species == SPECIES_NONE || species == SPECIES_EGG || GetMonData(mon, MON_DATA_HELD_ITEM) != ITEM_NONE)
        return;
    if (Random() % 100 >= TREASURE_PICKUP_CHANCE)
        return;
    item = GetTreasureWeatherItem(GetSavedWeather());
    if (item == ITEM_NONE)
        item = GetTreasureMapItem();
    if (item != ITEM_NONE)
        SetMonData(mon, MON_DATA_HELD_ITEM, &item);
}

// Petalburg 01: the gym aide's gift. Lvl 5, normal ability slot 1 (PICKUP), marked as treasure.
void GiveTreasureZigzagoon(void)
{
    struct Pokemon mon;
    bool8 isTreasure = TRUE;
    u8 abilityNum = 0;

    CreateRandomMon(&mon, SPECIES_ZIGZAGOON, 5);
    SetMonData(&mon, MON_DATA_ABILITY_NUM, &abilityNum);
    SetMonData(&mon, MON_DATA_IS_TREASURE, &isTreasure);
    gSpecialVar_Result = GiveScriptedMonToPlayer(&mon, PARTY_SIZE);
}
