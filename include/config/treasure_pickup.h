#ifndef GUARD_CONFIG_TREASURE_PICKUP_H
#define GUARD_CONFIG_TREASURE_PICKUP_H

// BestRomHackEver Core #42: the treasure ZIGZAGOON from the PETALBURG GYM aide.
// After each battle it's in the party and holding nothing, it has this % chance to dig up a type-boosting
// item. Which item depends on the weather first, then the map. Anywhere not listed: nothing.
#define TREASURE_PICKUP_CHANCE 1 // Percent per battle (starting value; future aides sell upgrades).

// Weather overrides the map (overworld weather where the battle happened).
#define TREASURE_PICKUP_WEATHER_ITEMS                        \
    TREASURE_WEATHER(WEATHER_RAIN,              ITEM_MYSTIC_WATER)   \
    TREASURE_WEATHER(WEATHER_DOWNPOUR,          ITEM_MYSTIC_WATER)   \
    TREASURE_WEATHER(WEATHER_RAIN_THUNDERSTORM, ITEM_MAGNET)         \
    TREASURE_WEATHER(WEATHER_DROUGHT,           ITEM_CHARCOAL)       \
    TREASURE_WEATHER(WEATHER_SANDSTORM,         ITEM_SOFT_SAND)      \
    TREASURE_WEATHER(WEATHER_SNOW,              ITEM_NEVER_MELT_ICE) \
    TREASURE_WEATHER(WEATHER_FOG_HORIZONTAL,    ITEM_SPELL_TAG)      \
    TREASURE_WEATHER(WEATHER_FOG_DIAGONAL,      ITEM_SPELL_TAG)

// Item by map. ROUTE 104 is split: south of PETALBURG WOODS (y >= TREASURE_ROUTE104_SOUTH_MIN_Y) finds SOFT SAND,
// the north part finds nothing (weather only).
#define TREASURE_PICKUP_MAP_ITEMS                         \
    TREASURE_MAP(MAP_ROUTE101,       ITEM_SILK_SCARF)     \
    TREASURE_MAP(MAP_ROUTE102,       ITEM_MIRACLE_SEED)   \
    TREASURE_MAP(MAP_ROUTE103,       ITEM_SHARP_BEAK)     \
    TREASURE_MAP(MAP_PETALBURG_WOODS, ITEM_SILVER_POWDER)

#define TREASURE_ROUTE104_SOUTH_MIN_Y 39
#define TREASURE_ROUTE104_SOUTH_ITEM  ITEM_SOFT_SAND

#endif // GUARD_CONFIG_TREASURE_PICKUP_H
