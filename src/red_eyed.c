// Red-eyed Pokémon (prototype): a red aura on the battle sprite.
//
// The flag is a spare bit on the Pokémon (MON_DATA_IS_RED_EYED), so whatever
// creates the wild mon (debug script now, per-area encounter data later) only
// has to set that bit on gEnemyParty.
//
// Static tint: same trick as the Dynamax/Tera tints. After the battler's palette
// is (re)loaded, red is blended in and copied into the *unfaded* buffer, which is
// the "base" palette that move/status anims and fades restore to. So the tint
// survives anything that restores the palette, and is re-applied by every code
// path that reloads it (send-out, switch-in, Bag/party return, Transform,
// substitute removal).
//
// Pulse + entry flash: one task blends a little extra red on top (faded buffer
// only), and pauses whenever something else may be using the palette.
#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_main.h"
#include "palette.h"
#include "pokemon.h"
#include "script.h"
#include "task.h"
#include "util.h"
#include "red_eyed.h"
#include "constants/rgb.h"

#define RED_EYED_COLOR          RGB(31, 2, 2)
#define RED_EYED_BASE_COEFF     4   // static tint baked into the palette (0-16)
#define RED_EYED_PULSE_COEFF    4   // extra tint at the top of a pulse
#define RED_EYED_PULSE_FRAMES   96  // one full pulse, about 1.6 s
#define RED_EYED_FLASH_COEFF    12  // entry flash strength
#define RED_EYED_FLASH_FRAMES   30

#define tTimer      data[0]
#define tFlash(b)   data[1 + (b)]   // frames of entry flash left, per battler

static void Task_RedEyedPulse(u8 taskId);

bool32 IsBattlerRedEyed(enum BattlerId battler)
{
    if (battler >= gBattlersCount)
        return FALSE;
    return GetMonData(GetBattlerMon(battler), MON_DATA_IS_RED_EYED);
}

static u8 GetOrCreatePulseTask(void)
{
    u8 taskId = FindTaskIdByFunc(Task_RedEyedPulse);
    if (taskId == TASK_NONE)
        taskId = CreateTask(Task_RedEyedPulse, 10);
    return taskId;
}

void RedEyed_ApplyTint(enum BattlerId battler, bool32 tintBgCopy)
{
    u32 offset;

    if (!IsBattlerRedEyed(battler))
        return;

    offset = OBJ_PLTT_ID(battler);
    BlendPalette(offset, 16, RED_EYED_BASE_COEFF, RED_EYED_COLOR);
    CpuCopy32(gPlttBufferFaded + offset, gPlttBufferUnfaded + offset, PLTT_SIZEOF(16));

    // Some move anims draw the mon on a BG layer using this copy of its palette.
    if (tintBgCopy)
    {
        offset = BG_PLTT_ID(8) + BG_PLTT_ID(battler);
        BlendPalette(offset, 16, RED_EYED_BASE_COEFF, RED_EYED_COLOR);
        CpuCopy32(gPlttBufferFaded + offset, gPlttBufferUnfaded + offset, PLTT_SIZEOF(16));
    }

    GetOrCreatePulseTask();
}

void RedEyed_StartEntryFlash(enum BattlerId battler)
{
    if (!IsBattlerRedEyed(battler))
        return;
    gTasks[GetOrCreatePulseTask()].tFlash(battler) = RED_EYED_FLASH_FRAMES;
}

// callnative: marks the wild mon made by setwildbattle as red-eyed.
void RedEyed_SetWildLeadRedEyed(struct ScriptContext *ctx)
{
    bool8 isRedEyed = TRUE;
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_IS_RED_EYED, &isRedEyed);
}

static bool32 IsPaletteFree(enum BattlerId battler)
{
    struct BattleHealthboxInfo *healthbox = &gBattleSpritesDataPtr->healthBoxesData[battler];

    if (!IsBattlerSpritePresent(battler) || gSprites[gBattlerSpriteIds[battler]].invisible)
        return FALSE;
    if (gBattleSpritesDataPtr->battlerData[battler].behindSubstitute) // substitute doll uses this palette slot
        return FALSE;
    if (healthbox->ballAnimActive || healthbox->statusAnimActive || healthbox->specialAnimActive)
        return FALSE;
    return TRUE;
}

static void Task_RedEyedPulse(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    u32 battler, half, wave, coeff, flash;

    if (!gMain.inBattle || gBattleOutcome != 0)
    {
        DestroyTask(taskId);
        return;
    }

    if (++tTimer >= RED_EYED_PULSE_FRAMES)
        tTimer = 0;

    // Only touch palettes on the battle screen itself (not the Bag/party menu),
    // and never while a move anim or a screen fade owns them.
    if (gMain.callback2 != BattleMainCB2 || gPaletteFade.active || gAnimScriptActive)
        return;

    half = RED_EYED_PULSE_FRAMES / 2;
    wave = (tTimer < half) ? tTimer : RED_EYED_PULSE_FRAMES - tTimer;  // 0 -> half -> 0
    for (battler = 0; battler < gBattlersCount; battler++)
    {
        if (!IsBattlerRedEyed(battler) || !IsPaletteFree(battler))
            continue;

        coeff = wave * RED_EYED_PULSE_COEFF / half;
        if (tFlash(battler) > 0)
        {
            flash = tFlash(battler) * RED_EYED_FLASH_COEFF / RED_EYED_FLASH_FRAMES;
            if (flash > coeff)
                coeff = flash;
            tFlash(battler)--;
        }
        // Blends from the unfaded (already tinted) palette, so coeff 0 = static tint.
        BlendPalette(OBJ_PLTT_ID(battler), 16, coeff, RED_EYED_COLOR);
    }
}
