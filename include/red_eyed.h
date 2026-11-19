#ifndef GUARD_RED_EYED_H
#define GUARD_RED_EYED_H

struct ScriptContext;

bool32 IsBattlerRedEyed(enum BattlerId battler);
void RedEyed_ApplyTint(enum BattlerId battler, bool32 tintBgCopy);
void RedEyed_StartEntryFlash(enum BattlerId battler);
void RedEyed_SetWildLeadRedEyed(struct ScriptContext *ctx);
void RedEyed_SetMonRedEyed(struct Pokemon *mon);
void RedEyed_TintColors(const u16 *src, u16 *dst, u32 count);
void RedEyed_TintColorsCoeff(const u16 *src, u16 *dst, u32 count, u32 coeff);

// Overworld red tint: a base blend, plus a pulse on top (one full pulse = RED_EYED_OBJ_PULSE_FRAMES).
#define RED_EYED_OBJ_BASE_COEFF   5
#define RED_EYED_OBJ_PULSE_COEFF  6
#define RED_EYED_OBJ_PULSE_FRAMES 96

// Palette tag for a red-eyed overworld object: one per species, kept apart from the normal tag
// so other objects of that species stay untinted. Species fit below OBJ_EVENT_MON (1 << 14).
#define RED_EYED_OBJ_PAL_TAG_BASE 0x3800

#endif // GUARD_RED_EYED_H
