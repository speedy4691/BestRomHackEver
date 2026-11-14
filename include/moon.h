#ifndef GUARD_MOON_H
#define GUARD_MOON_H

// Off-roadmap Q2: moon phases from the console's real calendar date.

enum MoonPhase
{
    MOON_NEW,
    MOON_WAXING_CRESCENT,
    MOON_FIRST_QUARTER,
    MOON_WAXING_GIBBOUS,
    MOON_FULL,
    MOON_WANING_GIBBOUS,
    MOON_LAST_QUARTER,
    MOON_WANING_CRESCENT,
    MOON_PHASE_COUNT,
};

extern const u8 *const gMoonPhaseNames[MOON_PHASE_COUNT];

enum MoonPhase GetMoonPhase(void);
bool32 IsFullMoonNight(void);
u32 GetMoonlitNightTint(void);
enum Species TryGetFullMoonSpecies(enum Species species);

// Debug only; not saved. MOON_PHASE_COUNT clears the override (use real date).
void SetMoonPhaseOverride(u32 phase);

#endif // GUARD_MOON_H
