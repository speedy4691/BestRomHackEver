#include "global.h"
#include "fpmath.h"
#include "moon.h"
#include "random.h"
#include "rtc.h"
#include "constants/map_groups.h"
#include "constants/species.h"

// Off-roadmap Q2: moon phases.
// The phase comes from the console's real date (raw RTC), not the save's shifted
// game clock, so it always matches the real sky and debug time jumps don't move it.

// 0 = no override; otherwise phase + 1. Debug only, so it lives in RAM, not the save.
EWRAM_DATA static u8 sMoonPhaseOverride = 0;

const u8 *const gMoonPhaseNames[MOON_PHASE_COUNT] =
{
    [MOON_NEW]             = COMPOUND_STRING("New Moon"),
    [MOON_WAXING_CRESCENT] = COMPOUND_STRING("Waxing Crescent"),
    [MOON_FIRST_QUARTER]   = COMPOUND_STRING("First Quarter"),
    [MOON_WAXING_GIBBOUS]  = COMPOUND_STRING("Waxing Gibbous"),
    [MOON_FULL]            = COMPOUND_STRING("Full Moon"),
    [MOON_WANING_GIBBOUS]  = COMPOUND_STRING("Waning Gibbous"),
    [MOON_LAST_QUARTER]    = COMPOUND_STRING("Last Quarter"),
    [MOON_WANING_CRESCENT] = COMPOUND_STRING("Waning Crescent"),
};

// All times below are in hours, counted from the RTC's day 1 (2000-01-01).
#define SYNODIC_MONTH_HOURS_X100 70873  // 29.5306 days * 24 h, in hundredths of an hour
#define REFERENCE_NEW_MOON_HOURS (5 * 24 + 18) // 2000-01-06 18:14 UTC (RTC day 6, 18h)

enum MoonPhase GetMoonPhase(void)
{
    struct SiiRtcInfo rtc;
    u32 hours, age, phase;

    if (sMoonPhaseOverride != 0)
        return sMoonPhaseOverride - 1;

    RtcGetInfo(&rtc);
    hours = (RtcGetDayCount(&rtc) - 1) * 24 + ConvertBcdToBinary(rtc.hour);
    if (hours < REFERENCE_NEW_MOON_HOURS)
        hours = REFERENCE_NEW_MOON_HOURS; // RTC error fallback (dummy date 2000-01-01)

    // Moon age in hundredths of an hour, then rounded to the nearest of 8 phases.
    age = ((hours - REFERENCE_NEW_MOON_HOURS) * 100) % SYNODIC_MONTH_HOURS_X100;
    phase = (age * MOON_PHASE_COUNT + SYNODIC_MONTH_HOURS_X100 / 2) / SYNODIC_MONTH_HOURS_X100;
    return phase % MOON_PHASE_COUNT;
}

void SetMoonPhaseOverride(u32 phase)
{
    sMoonPhaseOverride = (phase < MOON_PHASE_COUNT) ? phase + 1 : 0;
}

bool32 IsFullMoonNight(void)
{
    return GetMoonPhase() == MOON_FULL && GetTimeOfDay() == TIME_NIGHT;
}

// Night tint multipliers per channel (Q8.8, one byte each): darkest at new moon,
// lightest at full moon. Vanilla expansion night is 0.456/0.456/0.615 (about a quarter moon).
#define NIGHT_NEW_RG   Q_8_8(0.27)
#define NIGHT_NEW_B    Q_8_8(0.39)
#define NIGHT_FULL_RG  Q_8_8(0.60)
#define NIGHT_FULL_B   Q_8_8(0.74)

u32 GetMoonlitNightTint(void)
{
    u32 phase = GetMoonPhase();
    u32 light = (phase <= MOON_FULL) ? phase : MOON_PHASE_COUNT - phase; // 0 (new) .. 4 (full)
    u32 rg = NIGHT_NEW_RG + (NIGHT_FULL_RG - NIGHT_NEW_RG) * light / MOON_FULL;
    u32 b  = NIGHT_NEW_B  + (NIGHT_FULL_B  - NIGHT_NEW_B)  * light / MOON_FULL;
    return rg | (rg << 8) | (b << 16);
}

// Full-moon nights: on these maps, each wild encounter has `chance`% to become `species`
// (level unchanged). Species decided per area in C2 (Species by area).
struct FullMoonEncounter
{
    u16 map;
    u16 species;
    u8 chance;
};

static const struct FullMoonEncounter sFullMoonEncounters[] =
{
    // Example: { MAP_ROUTE101, SPECIES_CLEFAIRY, 30 },
};

enum Species TryGetFullMoonSpecies(enum Species species)
{
    u32 i;
    u16 map;

    if (ARRAY_COUNT(sFullMoonEncounters) == 0 || !IsFullMoonNight())
        return species;

    map = (gSaveBlock1Ptr->location.mapGroup << 8) | gSaveBlock1Ptr->location.mapNum;
    for (i = 0; i < ARRAY_COUNT(sFullMoonEncounters); i++)
    {
        if (sFullMoonEncounters[i].map == map && Random() % 100 < sFullMoonEncounters[i].chance)
            return sFullMoonEncounters[i].species;
    }
    return species;
}
