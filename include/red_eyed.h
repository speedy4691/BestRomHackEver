#ifndef GUARD_RED_EYED_H
#define GUARD_RED_EYED_H

struct ScriptContext;

bool32 IsBattlerRedEyed(enum BattlerId battler);
void RedEyed_ApplyTint(enum BattlerId battler, bool32 tintBgCopy);
void RedEyed_StartEntryFlash(enum BattlerId battler);
void RedEyed_SetWildLeadRedEyed(struct ScriptContext *ctx);

#endif // GUARD_RED_EYED_H
