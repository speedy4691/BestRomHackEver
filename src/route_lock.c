#include "global.h"
#include "battle_setup.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "event_scripts.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "overworld.h"
#include "route_lock.h"
#include "script.h"
#include "string_util.h"
#include "constants/maps.h"
#include "constants/script_commands.h"
#include "constants/trainers.h"

// Cleared state is one bit per route across two vars.
#define ROUTE_LOCK_MAX 32

static const struct RouteLockTile sRoute104North_TriggerTiles[] = { {10, 29}, {11, 29} }; // Petalburg Woods north exit
static const struct RouteLockTile sRoute104North_ClearTiles[]   = { {24, 8},  {25, 8} };  // first land after the bridge

static const struct RouteLock sRouteLocks[] =
{
    {
        .name = COMPOUND_STRING("Route 104 North"),
        .map = MAP_ROUTE104,
        .areaLeft = 0,
        .areaTop = 0,
        .areaRight = 39,
        .areaBottom = 29,
        .triggerTiles = sRoute104North_TriggerTiles,
        .triggerTileCount = ARRAY_COUNT(sRoute104North_TriggerTiles),
        .clearTiles = sRoute104North_ClearTiles,
        .clearTileCount = ARRAY_COUNT(sRoute104North_ClearTiles),
        .lockText = COMPOUND_STRING("No turning back until you reach\nRUSTBORO CITY!"),
        .retreatText = COMPOUND_STRING("You can't turn back until you\nreach RUSTBORO CITY!"),
        .clearText = COMPOUND_STRING("ROUTE 104 cleared!"),
    },
};

STATIC_ASSERT(ARRAY_COUNT(sRouteLocks) <= ROUTE_LOCK_MAX, TooManyLockedRoutes);

static u32 GetCurrentMap(void)
{
    return (gSaveBlock1Ptr->location.mapGroup << 8) | gSaveBlock1Ptr->location.mapNum;
}

static s32 GetLockedRouteId(void)
{
    u32 value = VarGet(VAR_ROUTE_LOCK_ACTIVE);

    if (value == 0 || value > ARRAY_COUNT(sRouteLocks))
        return -1;
    return value - 1;
}

static u16 GetClearedVar(u32 routeId)
{
    return routeId < 16 ? VAR_ROUTE_LOCK_CLEARED_1 : VAR_ROUTE_LOCK_CLEARED_2;
}

static bool32 IsRouteCleared(u32 routeId)
{
    return (VarGet(GetClearedVar(routeId)) >> (routeId % 16)) & 1;
}

static void SetRouteCleared(u32 routeId, bool32 cleared)
{
    u16 var = GetClearedVar(routeId);
    u16 bit = 1 << (routeId % 16);

    VarSet(var, cleared ? (VarGet(var) | bit) : (VarGet(var) & ~bit));
}

static bool32 IsOnTile(const struct RouteLockTile *tiles, u32 count, s16 x, s16 y)
{
    u32 i;

    for (i = 0; i < count; i++)
    {
        if (tiles[i].x == x && tiles[i].y == y)
            return TRUE;
    }
    return FALSE;
}

static bool32 IsInArea(const struct RouteLock *route, s16 x, s16 y)
{
    return x >= route->areaLeft && x <= route->areaRight && y >= route->areaTop && y <= route->areaBottom;
}

static void GetPlayerMapCoords(s16 *x, s16 *y)
{
    PlayerGetDestCoords(x, y);
    *x -= MAP_OFFSET;
    *y -= MAP_OFFSET;
}

static void RunTextScript(const u8 *script, const u8 *text)
{
    StringExpandPlaceholders(gStringVar4, text);
    ScriptContext_SetupScript(script);
}

static void LockRoute(u32 routeId)
{
    VarSet(VAR_ROUTE_LOCK_ACTIVE, routeId + 1);
    // Repels are blocked while locked, so switch off any that are running.
    FlagClear(FLAG_INFINITE_REPEL);
    VarSet(VAR_REPEL_STEP_COUNT, 0);
}

// Trainers inside the area can be fought again. Items stay picked up.
static void ResetRouteTrainers(const struct RouteLock *route)
{
    const struct MapHeader *header = Overworld_GetMapHeaderByGroupAndId(MAP_GROUP(route->map), MAP_NUM(route->map));
    u32 i;

    for (i = 0; i < header->events->objectEventCount; i++)
    {
        const struct ObjectEventTemplate *template = &header->events->objectEvents[i];
        const TrainerBattleParameter *params;

        if (!IsInArea(route, template->x, template->y) || template->script == NULL || template->script[0] != SCR_OP_TRAINERBATTLE)
            continue;
        params = (const TrainerBattleParameter *)(template->script + TRAINERBATTLE_OPCODE_OFFSET);
        ClearTrainerFlag(params->params.opponentA);
        if (params->params.opponentB != TRAINER_NONE)
            ClearTrainerFlag(params->params.opponentB);
    }
}

bool32 RouteLock_IsLocked(void)
{
    return GetLockedRouteId() >= 0;
}

// Called every frame the player has control: starts a lock on a trigger tile, or clears it on a clear tile.
bool32 RouteLock_TryTriggerAtPlayer(void)
{
    u32 map = GetCurrentMap();
    s32 lockedId = GetLockedRouteId();
    s16 x, y;
    u32 i;

    GetPlayerMapCoords(&x, &y);
    if (lockedId >= 0)
    {
        const struct RouteLock *route = &sRouteLocks[lockedId];

        if (route->map != map || !IsOnTile(route->clearTiles, route->clearTileCount, x, y))
            return FALSE;
        VarSet(VAR_ROUTE_LOCK_ACTIVE, 0);
        SetRouteCleared(lockedId, TRUE);
        RunTextScript(RouteLock_EventScript_Cleared, route->clearText);
        return TRUE;
    }

    for (i = 0; i < ARRAY_COUNT(sRouteLocks); i++)
    {
        const struct RouteLock *route = &sRouteLocks[i];

        if (route->map == map && !IsRouteCleared(i) && IsOnTile(route->triggerTiles, route->triggerTileCount, x, y))
        {
            LockRoute(i);
            RunTextScript(RouteLock_EventScript_Message, route->lockText);
            return TRUE;
        }
    }
    return FALSE;
}

// No retreat: stops a step from inside the locked area to a walkable tile (or warp) outside it.
bool32 RouteLock_TryBlockStep(enum Direction direction)
{
    s32 lockedId = GetLockedRouteId();
    const struct RouteLock *route;
    s16 x, y;

    if (lockedId < 0 || direction == DIR_NONE)
        return FALSE;
    route = &sRouteLocks[lockedId];
    if (route->map != GetCurrentMap())
        return FALSE;

    GetPlayerMapCoords(&x, &y);
    if (!IsInArea(route, x, y))
        return FALSE;

    PlayerGetDestCoords(&x, &y);
    MoveCoords(direction, &x, &y);
    if (IsInArea(route, x - MAP_OFFSET, y - MAP_OFFSET) || MapGridGetCollisionAt(x, y) != 0)
        return FALSE;

    RunTextScript(RouteLock_EventScript_Message, route->retreatText);
    return TRUE;
}

// Blacking out before clearing: the route resets and the lock re-arms on the trigger tile.
void RouteLock_OnWhiteOut(void)
{
    s32 lockedId = GetLockedRouteId();

    if (lockedId < 0)
        return;
    ResetRouteTrainers(&sRouteLocks[lockedId]);
    VarSet(VAR_ROUTE_LOCK_ACTIVE, 0);
}

const u8 *RouteLock_GetCantUseText(void)
{
    return COMPOUND_STRING("You can't use that until this\nroute is cleared!{PAUSE_UNTIL_PRESS}");
}

// Debug: the route on the current map (preferring one whose area holds the player).
static s32 FindRouteHere(bool32 mustBeInArea)
{
    u32 map = GetCurrentMap();
    s32 found = -1;
    s16 x, y;
    u32 i;

    GetPlayerMapCoords(&x, &y);
    for (i = 0; i < ARRAY_COUNT(sRouteLocks); i++)
    {
        if (sRouteLocks[i].map != map)
            continue;
        if (IsInArea(&sRouteLocks[i], x, y))
            return i;
        if (found < 0 && !mustBeInArea)
            found = i;
    }
    return found;
}

void RouteLock_DebugAction(enum RouteLockDebugAction action)
{
    const u8 *text;
    s32 routeId;
    u32 i;

    if (action == ROUTE_LOCK_DEBUG_RESET_ALL)
    {
        for (i = 0; i < ARRAY_COUNT(sRouteLocks); i++)
        {
            SetRouteCleared(i, FALSE);
            ResetRouteTrainers(&sRouteLocks[i]);
        }
        VarSet(VAR_ROUTE_LOCK_ACTIVE, 0);
        StringCopy(gStringVar4, COMPOUND_STRING("All locked routes reset."));
        return;
    }

    routeId = FindRouteHere(action == ROUTE_LOCK_DEBUG_LOCK);
    if (routeId < 0)
    {
        StringCopy(gStringVar4, action == ROUTE_LOCK_DEBUG_LOCK
                                ? COMPOUND_STRING("Stand inside a locked route's\narea to lock it.")
                                : COMPOUND_STRING("No locked route on this map."));
        return;
    }

    switch (action)
    {
    case ROUTE_LOCK_DEBUG_LOCK:
        SetRouteCleared(routeId, FALSE);
        LockRoute(routeId);
        text = COMPOUND_STRING("{STR_VAR_1}: locked.");
        break;
    case ROUTE_LOCK_DEBUG_CLEAR:
        SetRouteCleared(routeId, TRUE);
        if (GetLockedRouteId() == routeId)
            VarSet(VAR_ROUTE_LOCK_ACTIVE, 0);
        text = COMPOUND_STRING("{STR_VAR_1}: cleared.");
        break;
    case ROUTE_LOCK_DEBUG_RESET:
    default:
        SetRouteCleared(routeId, FALSE);
        if (GetLockedRouteId() == routeId)
            VarSet(VAR_ROUTE_LOCK_ACTIVE, 0);
        ResetRouteTrainers(&sRouteLocks[routeId]);
        text = COMPOUND_STRING("{STR_VAR_1}: reset (not locked,\nnot cleared, trainers back).");
        break;
    }
    StringCopy(gStringVar1, sRouteLocks[routeId].name);
    StringExpandPlaceholders(gStringVar4, text);
}
