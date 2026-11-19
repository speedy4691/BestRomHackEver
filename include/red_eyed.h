#ifndef GUARD_RED_EYED_H
#define GUARD_RED_EYED_H

struct ScriptContext;

bool32 IsBattlerRedEyed(enum BattlerId battler);
void RedEyed_ApplyTint(enum BattlerId battler, bool32 tintBgCopy);
void RedEyed_StartEntryFlash(enum BattlerId battler);
void RedEyed_SetWildLeadRedEyed(struct ScriptContext *ctx);
void RedEyed_SetMonRedEyed(struct Pokemon *mon);
void RedEyed_TintColors(const u16 *src, u16 *dst, u32 count);

// Palette tag for a red-eyed overworld object: one per species, kept apart from the normal tag
// so other objects of that species stay untinted. Species fit below OBJ_EVENT_MON (1 << 14).
#define RED_EYED_OBJ_PAL_TAG_BASE 0x3800

#endif // GUARD_RED_EYED_H
