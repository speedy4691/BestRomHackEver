#ifndef GUARD_ROUTE_LOCK_H
#define GUARD_ROUTE_LOCK_H

// L1 route lock. Each locked route is one entry in gRouteLocks (src/route_lock.c):
// adding a route means adding a table entry, not new code.

struct RouteLockTile
{
    s16 x, y; // map coordinates as shown in Porymap
};

struct RouteLock
{
    const u8 *name; // shown by the debug menu
    u16 map;        // MAP_* constant
    // The locked segment. While locked the player can't step out of this rectangle;
    // doors inside it (e.g. the flower shop) still work.
    s16 areaLeft, areaTop, areaRight, areaBottom;
    const struct RouteLockTile *triggerTiles; // stepping here starts the lock
    const struct RouteLockTile *clearTiles;   // stepping here while locked clears the route for good
    u8 triggerTileCount;
    u8 clearTileCount;
    const u8 *lockText;
    const u8 *retreatText;
    const u8 *clearText;
};

enum RouteLockDebugAction
{
    ROUTE_LOCK_DEBUG_RESET = 1, // starts at 1: the debug menu treats NULL params as "no params"
    ROUTE_LOCK_DEBUG_LOCK,
    ROUTE_LOCK_DEBUG_CLEAR,
    ROUTE_LOCK_DEBUG_RESET_ALL,
};

bool32 RouteLock_IsLocked(void);
bool32 RouteLock_TryTriggerAtPlayer(void);
bool32 RouteLock_TryBlockStep(enum Direction direction);
void RouteLock_OnWhiteOut(void);
const u8 *RouteLock_GetCantUseText(void);
void RouteLock_DebugAction(enum RouteLockDebugAction action);

#endif // GUARD_ROUTE_LOCK_H
