#include "global.h"
#include "event_data.h"
#include "event_scripts.h"
#include "gym_gate.h"
#include "pokedex.h"
#include "script.h"
#include "string_util.h"
#include "constants/maps.h"

// G2: each gym's door only opens once the player has caught enough different species
// (National Pokédex, duplicates count once). Values come from the "Level caps and gates"
// tab of the content sheet; 0 means no gate yet (sheet not filled in).
struct GymGate
{
    u16 map;          // gym interior the door leads to
    u16 badgeFlag;    // this gym's badge: no gate once it's won
    u16 prevBadgeFlag; // gate only applies once the previous badge is won (0 = first gym)
    u16 speciesNeeded;
};

static const struct GymGate sGymGates[] =
{
    { MAP_RUSTBORO_CITY_GYM,      FLAG_BADGE01_GET, 0,                3 }, // Roxanne (slice test value)
    { MAP_DEWFORD_TOWN_GYM,       FLAG_BADGE02_GET, FLAG_BADGE01_GET, 0 },
    { MAP_MAUVILLE_CITY_GYM,      FLAG_BADGE03_GET, FLAG_BADGE02_GET, 0 },
    { MAP_LAVARIDGE_TOWN_GYM_1F,  FLAG_BADGE04_GET, FLAG_BADGE03_GET, 0 },
    // Petalburg: the story visits before badge 4 aren't gated, thanks to prevBadgeFlag.
    { MAP_PETALBURG_CITY_GYM,     FLAG_BADGE05_GET, FLAG_BADGE04_GET, 0 },
    { MAP_FORTREE_CITY_GYM,       FLAG_BADGE06_GET, FLAG_BADGE05_GET, 0 },
    { MAP_MOSSDEEP_CITY_GYM,      FLAG_BADGE07_GET, FLAG_BADGE06_GET, 0 },
    { MAP_SOOTOPOLIS_CITY_GYM_1F, FLAG_BADGE08_GET, FLAG_BADGE07_GET, 0 },
};

static const struct GymGate *GetGateForMap(u16 map)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sGymGates); i++)
    {
        if (sGymGates[i].map == map)
            return &sGymGates[i];
    }
    return NULL;
}

// Called before a door warp. Returns TRUE (and shows the refusal) if the door must stay shut.
bool32 GymGate_TryBlockWarp(u8 destMapGroup, u8 destMapNum)
{
    const struct GymGate *gate = GetGateForMap((destMapGroup << 8) | destMapNum);
    u32 caught;

    if (gate == NULL || gate->speciesNeeded == 0)
        return FALSE;
    if (FlagGet(gate->badgeFlag) || (gate->prevBadgeFlag != 0 && !FlagGet(gate->prevBadgeFlag)))
        return FALSE;

    caught = GetNationalPokedexCount(FLAG_GET_CAUGHT);
    if (caught >= gate->speciesNeeded)
        return FALSE;

    ConvertIntToDecimalStringN(gStringVar1, caught, STR_CONV_MODE_LEFT_ALIGN, 4);
    ConvertIntToDecimalStringN(gStringVar2, gate->speciesNeeded, STR_CONV_MODE_LEFT_ALIGN, 4);
    StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("The GYM's door won't open…\pCatch at least {STR_VAR_2} different kinds\nof POKéMON to take on this GYM.\pCaught so far: {STR_VAR_1}/{STR_VAR_2}."));
    ScriptContext_SetupScript(GymGate_EventScript_Refuse);
    return TRUE;
}
