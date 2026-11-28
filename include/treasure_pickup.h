#ifndef GUARD_TREASURE_PICKUP_H
#define GUARD_TREASURE_PICKUP_H

// BestRomHackEver Core #42: treasure ZIGZAGOON (include/config/treasure_pickup.h).
void TreasurePickup_TryFindItem(struct Pokemon *mon);
void GiveTreasureZigzagoon(void); // special: VAR_RESULT = MON_GIVEN_TO_PARTY / MON_GIVEN_TO_PC / MON_CANT_GIVE

#endif // GUARD_TREASURE_PICKUP_H
