#include "global.h"
#include "photography.h"
#include "battle.h"
#include "battle_main.h"
#include "battle_util.h"
#include "battle_environment.h"
#include "battle_setup.h"
#include "bg.h"
#include "decompress.h"
#include "event_data.h"
#include "gpu_regs.h"
#include "item_menu.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "battle_message.h"
#include "battle_bg.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_animation.h"
#include "random.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "text.h"
#include "trainer_pokemon_sprites.h"
#include "window.h"
#include "config/photography.h"
#include "constants/items.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// BestRomHackEver Photography prototype (Feature Plan, Photography tab).
// Part B: best grade per species, upgrade-only. Part D: viewfinder screen. Tuning lives in include/config/photography.h.

// ---------------------------------------------------------------------------
// Grades (SaveBlock3): 4 per byte, species N uses bits (N % 4) * 2 of byte N / 4.

enum PhotoGrade GetPhotoGrade(enum Species species)
{
    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return PHOTO_GRADE_NONE;
    return (gSaveBlock3Ptr->photoGrades[species / 4] >> ((species % 4) * 2)) & 3;
}

// Overwrites the grade (debug). Gameplay uses TryUpgradePhotoGrade.
void SetPhotoGrade(enum Species species, enum PhotoGrade grade)
{
    u32 shift = (species % 4) * 2;
    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return;
    gSaveBlock3Ptr->photoGrades[species / 4] &= ~(3 << shift);
    gSaveBlock3Ptr->photoGrades[species / 4] |= (grade & 3) << shift;
}

// A better photo upgrades the stored grade; a worse one never downgrades it.
bool32 TryUpgradePhotoGrade(enum Species species, enum PhotoGrade grade)
{
    if (grade <= GetPhotoGrade(species))
        return FALSE;
    SetPhotoGrade(species, grade);
    return TRUE;
}

void ClearAllPhotoGrades(void)
{
    memset(gSaveBlock3Ptr->photoGrades, 0, sizeof(gSaveBlock3Ptr->photoGrades));
}

const u8 *GetPhotoGradeName(enum PhotoGrade grade)
{
    switch (grade)
    {
    case PHOTO_GRADE_PHOTOGRAPHED: return COMPOUND_STRING("Photographed");
    case PHOTO_GRADE_GOOD:         return COMPOUND_STRING("Good");
    case PHOTO_GRADE_AMAZING:      return COMPOUND_STRING("Amazing");
    default:                       return COMPOUND_STRING("---");
    }
}

// Pokédex entry line, e.g. "Photo: Good (60-84)". Only the grade is saved (2 bits per species), so the
// line shows the grade's score range, taken from the thresholds in include/config/photography.h.
void GetPhotoGradeDexText(u8 *dest, enum Species species)
{
    u32 low, high;

    switch (GetPhotoGrade(species))
    {
    case PHOTO_GRADE_PHOTOGRAPHED:
        dest = StringCopy(dest, COMPOUND_STRING("Photo: Taken ("));
        low = 0;
        high = PHOTO_GOOD_SCORE - 1;
        break;
    case PHOTO_GRADE_GOOD:
        dest = StringCopy(dest, COMPOUND_STRING("Photo: Good ("));
        low = PHOTO_GOOD_SCORE;
        high = PHOTO_AMAZING_SCORE - 1;
        break;
    case PHOTO_GRADE_AMAZING:
        dest = StringCopy(dest, COMPOUND_STRING("Photo: Amazing ("));
        low = PHOTO_AMAZING_SCORE;
        high = 100;
        break;
    default:
        StringCopy(dest, COMPOUND_STRING("Photo: ---"));
        return;
    }
    dest = ConvertIntToDecimalStringN(dest, low, STR_CONV_MODE_LEFT_ALIGN, 3);
    dest = StringCopy(dest, COMPOUND_STRING("-"));
    dest = ConvertIntToDecimalStringN(dest, high, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringCopy(dest, COMPOUND_STRING(")"));
}

// ---------------------------------------------------------------------------
// Rewards for a species' best grade

static const u8 sBestStatOrder[] = { STAT_ATK, STAT_DEF, STAT_SPATK, STAT_SPDEF, STAT_SPEED };

static u32 GetBaseStatForReward(enum Species species, u32 stat)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];
    switch (stat)
    {
    case STAT_ATK:   return info->baseAttack;
    case STAT_DEF:   return info->baseDefense;
    case STAT_SPATK: return info->baseSpAttack;
    case STAT_SPDEF: return info->baseSpDefense;
    default:         return info->baseSpeed;
    }
}

// TRUE if the nature lowers the species' highest base stat (HP ignored; with a tie, any of the tied stats).
static bool32 DoesNatureLowerBestStat(enum Species species, u32 nature)
{
    u32 i, best = 0, statDown = gNaturesInfo[nature].statDown;

    if (gNaturesInfo[nature].statUp == statDown) // Neutral nature.
        return FALSE;
    for (i = 0; i < ARRAY_COUNT(sBestStatOrder); i++)
        best = max(best, GetBaseStatForReward(species, sBestStatOrder[i]));
    return GetBaseStatForReward(species, statDown) == best;
}

// Good or better: a nature that lowers the best stat is re-rolled (with the game's own nature picker,
// so Synchronize still works).
u32 PhotoReward_PickNature(enum Species species, u32 nature, u32 (*pickNature)(enum Species))
{
    u32 i;

    if (GetPhotoGrade(species) < PHOTO_GRADE_GOOD)
        return nature;
    for (i = 0; i < PHOTO_NATURE_REROLLS && DoesNatureLowerBestStat(species, nature); i++)
        nature = pickNature(species);
    return nature;
}

// Amazing: each IV is the best of PHOTO_AMAZING_IV_ROLLS rolls (the one already rolled counts as the first).
void PhotoReward_BoostIvs(struct Pokemon *mon)
{
    u32 i, roll;

    if (GetPhotoGrade(GetMonData(mon, MON_DATA_SPECIES)) < PHOTO_GRADE_AMAZING)
        return;
    for (i = 0; i < NUM_STATS; i++)
    {
        u32 iv = GetMonData(mon, MON_DATA_HP_IV + i);
        for (roll = 1; roll < PHOTO_AMAZING_IV_ROLLS; roll++)
            iv = max(iv, Random() % (MAX_PER_STAT_IVS + 1));
        SetMonData(mon, MON_DATA_HP_IV + i, &iv);
    }
    CalculateMonStats(mon);
}

static u8 *AppendIv(u8 *str, const u8 *label, struct Pokemon *mon, u32 data)
{
    str = StringCopy(str, label);
    return ConvertIntToDecimalStringN(str, GetMonData(mon, data), STR_CONV_MODE_LEFT_ALIGN, 2);
}

// Info panel over the battle scene, opened with SELECT on the action menu of a wild battle.
//   Any wild POKéMON: name and type(s).
//   Photographed+:    also nature and ability.
//   Good+:            also IVs.
// It borrows the move description window (B_WIN_MOVE_DESCRIPTION, only used from the move menu),
// moved up into view while open.
#define PHOTO_INFO_WINDOW       B_WIN_MOVE_DESCRIPTION
#define PHOTO_INFO_TILEMAP_TOP  27 // Action menu shows BG0 from row 20, so this is 56 px down the screen.
#define PHOTO_INFO_FONT         FONT_SMALL_NARROW
#define PHOTO_INFO_LINE(n)      (1 + (n) * 9) // The font is 8 px tall; 5 lines fit the 48 px window.
// The font leaves blank rows above each glyph, and text can't start above y 0, so the drawn text is
// moved up this many pixels afterwards to centre it vertically in the window.
#define PHOTO_INFO_RAISE        2

static EWRAM_DATA u8 sPhotoInfoBattlerIdx = 0;   // Which opponent is shown (0 = left, 1 = right).
static EWRAM_DATA u8 sPhotoInfoSavedTop = 0;

// The opponent's battler on the left (0) or right (1), or MAX_BATTLERS_COUNT if there's none to show.
static enum BattlerId GetPhotoInfoBattler(u32 idx)
{
    enum BattlerId battler;

    if (idx > 0 && !IsDoubleBattle())
        return MAX_BATTLERS_COUNT;
    battler = GetBattlerAtPosition(idx == 0 ? B_POSITION_OPPONENT_LEFT : B_POSITION_OPPONENT_RIGHT);
    if (battler >= gBattlersCount || (gAbsentBattlerFlags & (1u << battler)) || gBattleMons[battler].hp == 0)
        return MAX_BATTLERS_COUNT;
    return battler;
}

static void PrintPhotoInfoLine(u32 lineNum, const u8 *text)
{
    // Transparent background: lines are closer together than the font's cell, so a painted background
    // would cover the bottom of the line above.
    static const u8 sColors[] = { TEXT_COLOR_TRANSPARENT, TEXT_DYNAMIC_COLOR_4, TEXT_DYNAMIC_COLOR_6 };
    AddTextPrinterParameterized4(PHOTO_INFO_WINDOW, PHOTO_INFO_FONT, 2, PHOTO_INFO_LINE(lineNum), 0, 0, sColors, TEXT_SKIP_DRAW, text);
}

// Moves the window's pixels up by `rows` (4bpp tiles, 4 bytes per tile row); the bottom is refilled.
static void RaisePhotoInfoText(u32 rows)
{
    u8 *buf = (u8 *)GetWindowAttribute(PHOTO_INFO_WINDOW, WINDOW_TILE_DATA);
    u32 widthTiles = GetWindowAttribute(PHOTO_INFO_WINDOW, WINDOW_WIDTH);
    u32 height = GetWindowAttribute(PHOTO_INFO_WINDOW, WINDOW_HEIGHT) * 8;
    u32 x, y;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < widthTiles; x++)
        {
            u32 *dst = (u32 *)(buf + ((y / 8) * widthTiles + x) * TILE_SIZE_4BPP + (y % 8) * 4);
            u32 srcY = y + rows;
            if (srcY < height)
                *dst = *(u32 *)(buf + ((srcY / 8) * widthTiles + x) * TILE_SIZE_4BPP + (srcY % 8) * 4);
            else
                *dst = 0xEEEEEEEE; // PIXEL_FILL(0xE), the window background.
        }
    }
}

static void DrawPhotoInfo(enum BattlerId battler)
{
    struct Pokemon *mon = GetBattlerMon(battler);
    enum Species species = GetMonData(mon, MON_DATA_SPECIES);
    enum PhotoGrade grade = GetPhotoGrade(species);
    u8 line[64];
    u8 *str;

    FillWindowPixelBuffer(PHOTO_INFO_WINDOW, PIXEL_FILL(0xE));

    // Line 1: name and current type(s) (battle types, so form or type changes show).
    str = StringCopy(line, GetSpeciesName(species));
    str = StringCopy(str, COMPOUND_STRING("  "));
    str = StringCopy(str, gTypesInfo[gBattleMons[battler].types[0]].name);
    if (gBattleMons[battler].types[1] != gBattleMons[battler].types[0])
    {
        str = StringCopy(str, COMPOUND_STRING("/"));
        StringCopy(str, gTypesInfo[gBattleMons[battler].types[1]].name);
    }
    PrintPhotoInfoLine(0, line);

    if (grade == PHOTO_GRADE_NONE)
    {
        PrintPhotoInfoLine(1, COMPOUND_STRING("Photograph it to learn more."));
    }
    else
    {
        // Lines 2-3: nature and ability.
        StringCopy(StringCopy(line, COMPOUND_STRING("Nature: ")), gNaturesInfo[GetNature(mon)].name);
        PrintPhotoInfoLine(1, line);
        StringCopy(StringCopy(line, COMPOUND_STRING("Ability: ")), gAbilitiesInfo[GetMonAbility(mon)].name);
        PrintPhotoInfoLine(2, line);

        // Lines 4-5: IVs.
        if (grade >= PHOTO_GRADE_GOOD)
        {
            str = AppendIv(line, COMPOUND_STRING("IVs  HP "), mon, MON_DATA_HP_IV);
            str = AppendIv(str, COMPOUND_STRING("  Atk "), mon, MON_DATA_ATK_IV);
            AppendIv(str, COMPOUND_STRING("  Def "), mon, MON_DATA_DEF_IV);
            PrintPhotoInfoLine(3, line);
            str = AppendIv(line, COMPOUND_STRING("SpA "), mon, MON_DATA_SPATK_IV);
            str = AppendIv(str, COMPOUND_STRING("  SpD "), mon, MON_DATA_SPDEF_IV);
            AppendIv(str, COMPOUND_STRING("  Spe "), mon, MON_DATA_SPEED_IV);
            PrintPhotoInfoLine(4, line);
        }
        else
        {
            PrintPhotoInfoLine(3, COMPOUND_STRING("IVs: take a Good photo."));
        }
    }
    RaisePhotoInfoText(PHOTO_INFO_RAISE);
    CopyWindowToVram(PHOTO_INFO_WINDOW, COPYWIN_FULL);
}

static bool32 IsPhotoInfoBattle(void)
{
    return !(gBattleTypeFlags & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED_LINK
                               | BATTLE_TYPE_GHOST | BATTLE_TYPE_CATCH_TUTORIAL | BATTLE_TYPE_SAFARI));
}

// L shortcut: same rules as CAMERA from the Bag (ItemUseInBattle_Camera), and the CAMERA must be in the Bag.
bool32 PhotoCamera_CanUseShortcut(void)
{
    return !(gBattleTypeFlags & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED_LINK | BATTLE_TYPE_SAFARI))
        && gBattleStruct->photoShotsUsed < PHOTO_SHOTS_PER_BATTLE
        && CheckBagHasItem(ITEM_CAMERA, 1);
}

u32 PhotoReward_CatchRatePercent(enum Species species)
{
    switch (GetPhotoGrade(species))
    {
    case PHOTO_GRADE_PHOTOGRAPHED: return PHOTO_CATCH_PERCENT_PHOTOGRAPHED;
    case PHOTO_GRADE_GOOD:         return PHOTO_CATCH_PERCENT_GOOD;
    case PHOTO_GRADE_AMAZING:      return PHOTO_CATCH_PERCENT_AMAZING;
    default:                       return 100;
    }
}

bool32 ShouldShowPhotoGradeIcon(void)
{
    return !(gBattleTypeFlags & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED_LINK
                               | BATTLE_TYPE_CATCH_TUTORIAL | BATTLE_TYPE_GHOST));
}

// TRUE if this attacker is a wild POKéMON and a camera use hasn't been paid for with a crit yet.
bool32 PhotoCrit_IsPending(enum BattlerId battlerAtk)
{
    return gBattleStruct->photoCritsPending != 0
        && !IsOnPlayerSide(battlerAtk)
        && !(gBattleTypeFlags & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED_LINK));
}

void PhotoCrit_Consume(void)
{
    if (gBattleStruct->photoCritsPending != 0)
        gBattleStruct->photoCritsPending--;
}

// Opens the panel in a wild battle (left opponent first). FALSE = not a wild battle.
bool32 PhotoInfo_TryOpen(void)
{
    u32 i;

    if (!IsPhotoInfoBattle())
        return FALSE;
    for (i = 0; i < 2; i++)
    {
        enum BattlerId battler = GetPhotoInfoBattler(i);
        if (battler != MAX_BATTLERS_COUNT)
        {
            sPhotoInfoBattlerIdx = i;
            sPhotoInfoSavedTop = GetWindowAttribute(PHOTO_INFO_WINDOW, WINDOW_TILEMAP_TOP);
            SetWindowAttribute(PHOTO_INFO_WINDOW, WINDOW_TILEMAP_TOP, PHOTO_INFO_TILEMAP_TOP);
            LoadMessageBoxAndBorderGfx();
            DrawStdWindowFrame(PHOTO_INFO_WINDOW, FALSE);
            DrawPhotoInfo(battler);
            PlaySE(SE_SELECT);
            return TRUE;
        }
    }
    return FALSE;
}

// SELECT shows the other opponent in a double battle; A or B (or SELECT with one opponent) closes.
bool32 PhotoInfo_HandleInput(void)
{
    if (JOY_NEW(SELECT_BUTTON))
    {
        enum BattlerId other = GetPhotoInfoBattler(sPhotoInfoBattlerIdx ^ 1);
        if (other != MAX_BATTLERS_COUNT)
        {
            sPhotoInfoBattlerIdx ^= 1;
            PlaySE(SE_SELECT);
            DrawPhotoInfo(other);
            return FALSE;
        }
    }
    if (JOY_NEW(A_BUTTON | B_BUTTON | SELECT_BUTTON))
    {
        PlaySE(SE_SELECT);
        FillWindowPixelBuffer(PHOTO_INFO_WINDOW, PIXEL_FILL(0));
        ClearStdWindowAndFrame(PHOTO_INFO_WINDOW, TRUE);
        CopyWindowToVram(PHOTO_INFO_WINDOW, COPYWIN_GFX);
        SetWindowAttribute(PHOTO_INFO_WINDOW, WINDOW_TILEMAP_TOP, sPhotoInfoSavedTop);
        return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Viewfinder

#define TAG_PHOTO_CORNER 0x5A10
#define TAG_PHOTO_CROSS  0x5A11
#define TAG_PHOTO_CENTRE 0x5A12
#define TAG_PHOTO_PAL    0x5A10
#define MON_PAL_SLOT     15   // OBJ palette slot for the POKéMON (the frame's palette takes a tag slot from 0).
#define UNLIMITED_SHOTS  0xFF
#define WIN_TEXT         0

enum { VF_STATE_RUN, VF_STATE_EXIT };

struct PhotoScore
{
    u8 centre, size, pose, total;
    u8 grade;
    bool8 inFrame;
};

struct PhotoViewfinder
{
    MainCallback returnCallback;
    u16 species;
    bool8 inBattle;
    u16 savedGpuRegs[7]; // Window/blend registers on entry, put back on exit.
    u8 state;
    u8 monSpriteId;
    u8 cornerSpriteIds[4];
    u8 crossSpriteId;
    u8 centreSpriteId; // Marker on the POKéMON's centre (PHOTO_SHOW_MON_CENTRE).
    u8 dispMatrixNum;
    u8 zoomStep;
    u8 shotsLeft;
    bool8 facingRight;
    bool8 animRunning;
    bool8 poseAtPeak;
    u16 animTimer;
    u16 cooldown;
    u16 closeTimer;      // Counts down after the last shot, then the camera closes.
    u8 shotsThisVisit;
    u16 animPeak;      // Biggest movement in the current animation loop.
    u16 lastAnimPeak;  // ...and in the previous loop.
    s32 monX, monY;    // Centre, 1/256 px.
    s32 velX, velY;    // 1/256 px per frame.
    s16 frameX, frameY; // Frame centre.
    s16 dispX2, dispY2; // Animation offset as displayed (zoomed, flipped).
    s8 boxLeft, boxRight, boxTop, boxBottom; // Opaque pixels of the pic, relative to its centre.
    u16 *tilemapBuf;
};

static EWRAM_DATA struct PhotoViewfinder *sVf = NULL;

static const s16 sZoomSteps[PHOTO_ZOOM_STEP_COUNT] = PHOTO_ZOOM_STEPS;
static const u8 sSizePoints[PHOTO_ZOOM_STEP_COUNT] = PHOTO_SIZE_POINTS;

// 16x16 frame corner (top-left; the others are flips) and 8x8 crosshair. Colour 1 white, 2 dark.
static const u32 sCornerGfx[] = {
    0x11111111, 0x11111111, 0x22222211, 0x00000211, 0x00000211, 0x00000211, 0x00000211, 0x00000211,
    0x11111111, 0x11111111, 0x22222222, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000211, 0x00000211, 0x00000211, 0x00000211, 0x00000211, 0x00000211, 0x00000211, 0x00000211,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};
static const u32 sCrossGfx[] = {
    0x00000000, 0x00011000, 0x00011000, 0x01111110, 0x01111110, 0x00011000, 0x00011000, 0x00000000,
};
// Red "x" centred on pixel (4,4), the sprite's position (colour 3).
static const u32 sCentreGfx[] = {
    0x00000000, 0x00000000, 0x03000300, 0x00303000, 0x00030000, 0x00303000, 0x03000300, 0x00000000,
};
static const u16 sFramePal[16] = { RGB(0, 0, 0), RGB_WHITE, RGB(5, 5, 6), RGB(31, 3, 3) };

static const struct SpriteSheet sCornerSheet = { sCornerGfx, sizeof(sCornerGfx), TAG_PHOTO_CORNER };
static const struct SpriteSheet sCrossSheet = { sCrossGfx, sizeof(sCrossGfx), TAG_PHOTO_CROSS };
static const struct SpriteSheet sCentreSheet = { sCentreGfx, sizeof(sCentreGfx), TAG_PHOTO_CENTRE };
static const struct SpritePalette sFrameSpritePal = { sFramePal, TAG_PHOTO_PAL };

static const struct OamData sOam_Corner = { .shape = SPRITE_SHAPE(16x16), .size = SPRITE_SIZE(16x16), .priority = 0 };
static const struct OamData sOam_Cross = { .shape = SPRITE_SHAPE(8x8), .size = SPRITE_SIZE(8x8), .priority = 0 };

static const struct SpriteTemplate sCornerTemplate =
{
    .tileTag = TAG_PHOTO_CORNER, .paletteTag = TAG_PHOTO_PAL, .oam = &sOam_Corner,
    .anims = gDummySpriteAnimTable, .images = NULL, .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy,
};
static const struct SpriteTemplate sCrossTemplate =
{
    .tileTag = TAG_PHOTO_CROSS, .paletteTag = TAG_PHOTO_PAL, .oam = &sOam_Cross,
    .anims = gDummySpriteAnimTable, .images = NULL, .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy,
};
static const struct SpriteTemplate sCentreTemplate =
{
    .tileTag = TAG_PHOTO_CENTRE, .paletteTag = TAG_PHOTO_PAL, .oam = &sOam_Cross,
    .anims = gDummySpriteAnimTable, .images = NULL, .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy,
};

static const struct BgTemplate sBgTemplates[] =
{
    { .bg = 0, .charBaseIndex = 0, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
    // Same layout as the battle background (src/battle_bg.c), so its tileset/tilemap load as-is.
    { .bg = 3, .charBaseIndex = 2, .mapBaseIndex = 26, .screenSize = 1, .paletteMode = 0, .priority = 3, .baseTile = 0 },
};

static const struct WindowTemplate sWindowTemplates[] =
{
    [WIN_TEXT] = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 15, .width = 30, .height = 5, .paletteNum = 15, .baseBlock = 1 },
    DUMMY_WIN_TEMPLATE,
};

static void UpdateMonCentreMarker(void);
static void CB2_InitViewfinder(void);
static void CB2_Viewfinder(void);

// The viewfinder uses WIN0 and blending. On exit these go back to what they were, not to 0: the battle keeps WIN0
// on with these settings, and screens that re-enable WIN0 (e.g. the new-Pokédex-entry page after a catch) were
// left black when WININ/WINOUT/WIN0H/WIN0V had been zeroed.
static const u8 sSavedGpuRegOffsets[] =
{
    REG_OFFSET_WININ, REG_OFFSET_WINOUT, REG_OFFSET_WIN0H, REG_OFFSET_WIN0V,
    REG_OFFSET_BLDCNT, REG_OFFSET_BLDALPHA, REG_OFFSET_BLDY,
};

static void StartViewfinder(enum Species species, bool32 isShiny, u32 personality, bool32 inBattle, u32 shotsLeft, MainCallback returnCallback)
{
    sVf = AllocZeroed(sizeof(*sVf));
    for (u32 i = 0; i < ARRAY_COUNT(sSavedGpuRegOffsets); i++)
        sVf->savedGpuRegs[i] = GetGpuReg(sSavedGpuRegOffsets[i]);
    sVf->species = SanitizeSpeciesId(species);
    sVf->inBattle = inBattle;
    sVf->shotsLeft = shotsLeft;
    sVf->returnCallback = returnCallback;
    // Stash the pic parameters until the sprite is created in CB2_InitViewfinder.
    sVf->velX = isShiny;
    sVf->velY = personality;
    gMain.state = 0;
    SetMainCallback2(CB2_InitViewfinder);
}

void OpenPhotoViewfinder_Debug(enum Species species, MainCallback returnCallback)
{
    StartViewfinder(species, FALSE, 0, FALSE, UNLIMITED_SHOTS, returnCallback);
}

// From the Bag in a wild battle (ItemUseInBattle_Camera). Photographs the opponent on the left.
void CB2_OpenPhotoViewfinderFromBattle(void)
{
    struct Pokemon *mon = GetBattlerMon(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT));
    // Each opening (however many shots) makes the wild POKéMON's next damaging move a critical hit.
    if (gBattleStruct->photoCritsPending < 0xFF)
        gBattleStruct->photoCritsPending++;
    StartViewfinder(GetMonData(mon, MON_DATA_SPECIES), IsMonShiny(mon), GetMonData(mon, MON_DATA_PERSONALITY),
                    TRUE, PHOTO_SHOTS_PER_BATTLE - gBattleStruct->photoShotsUsed, gBagPosition.exitCallback);
}

static void VBlankCB_Viewfinder(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void LoadViewfinderBackground(void)
{
    u32 env = sVf->inBattle ? gBattleEnvironment : BattleSetup_GetEnvironmentId();
    if (env >= BATTLE_ENVIRONMENT_COUNT || gBattleEnvironmentInfo[env].background.tileset == NULL)
        env = BATTLE_ENVIRONMENT_PLAIN;
    DecompressDataWithHeaderVram(gBattleEnvironmentInfo[env].background.tileset, (void *)BG_CHAR_ADDR(2));
    DecompressDataWithHeaderVram(gBattleEnvironmentInfo[env].background.tilemap, (void *)BG_SCREEN_ADDR(26));
    LoadPalette(gBattleEnvironmentInfo[env].palette, BG_PLTT_ID(2), 3 * PLTT_SIZE_4BPP);
}

// Finds the opaque pixels of frame 0 of the front pic (64x64, 4bpp tiles).
static void MeasureMonPic(void)
{
    const u8 *pic = gSprites[sVf->monSpriteId].images[0].data;
    s32 x, y, minX = 64, maxX = -1, minY = 64, maxY = -1;

    for (y = 0; y < 64; y++)
    {
        for (x = 0; x < 64; x++)
        {
            u32 byte = pic[((y / 8) * 8 + (x / 8)) * TILE_SIZE_4BPP + (y % 8) * 4 + (x % 8) / 2];
            if ((x & 1 ? byte >> 4 : byte & 0xF) != 0)
            {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (maxX < 0)
    {
        minX = minY = 16;
        maxX = maxY = 47;
    }
    sVf->boxLeft = minX - 32;
    sVf->boxRight = maxX + 1 - 32;
    sVf->boxTop = minY - 32;
    sVf->boxBottom = maxY + 1 - 32;
}

// The POKéMON's on-screen box: pic box zoomed, mirrored when facing right, plus the animation offset.
static void GetMonScreenBox(s32 *left, s32 *right, s32 *top, s32 *bottom, bool32 withAnimOffset)
{
    s32 z = sZoomSteps[sVf->zoomStep];
    s32 l = sVf->boxLeft, r = sVf->boxRight;
    s32 cx = sVf->monX >> 8, cy = sVf->monY >> 8;

    if (sVf->facingRight)
    {
        l = -sVf->boxRight;
        r = -sVf->boxLeft;
    }
    if (withAnimOffset)
    {
        cx += sVf->dispX2;
        cy += sVf->dispY2;
    }
    *left = cx + l * z / 256;
    *right = cx + r * z / 256;
    *top = cy + sVf->boxTop * z / 256;
    *bottom = cy + sVf->boxBottom * z / 256;
}

static void UpdateFrameGfx(void)
{
    s32 l = sVf->frameX - PHOTO_FRAME_WIDTH / 2, t = sVf->frameY - PHOTO_FRAME_HEIGHT / 2;
    s32 r = l + PHOTO_FRAME_WIDTH, b = t + PHOTO_FRAME_HEIGHT;

    gSprites[sVf->cornerSpriteIds[0]].x = l + 8; gSprites[sVf->cornerSpriteIds[0]].y = t + 8;
    gSprites[sVf->cornerSpriteIds[1]].x = r - 8; gSprites[sVf->cornerSpriteIds[1]].y = t + 8;
    gSprites[sVf->cornerSpriteIds[2]].x = l + 8; gSprites[sVf->cornerSpriteIds[2]].y = b - 8;
    gSprites[sVf->cornerSpriteIds[3]].x = r - 8; gSprites[sVf->cornerSpriteIds[3]].y = b - 8;
    gSprites[sVf->crossSpriteId].x = sVf->frameX;
    gSprites[sVf->crossSpriteId].y = sVf->frameY;
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(l, r));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(t, b));
}

static void PrintLines(const u8 *line2, const u8 *line3)
{
    static const u8 sColors[] = { TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY };
    u8 *str = gStringVar4;

    if (sVf->shotsLeft == UNLIMITED_SHOTS)
    {
        str = StringCopy(str, COMPOUND_STRING("Shots: no limit (debug)"));
    }
    else
    {
        str = StringCopy(str, COMPOUND_STRING("Shots left: "));
        str = ConvertIntToDecimalStringN(str, sVf->shotsLeft, STR_CONV_MODE_LEFT_ALIGN, 1);
    }
    str = StringCopy(str, COMPOUND_STRING("   Zoom: "));
    str = ConvertIntToDecimalStringN(str, sVf->zoomStep + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
    str = StringCopy(str, COMPOUND_STRING("/"));
    ConvertIntToDecimalStringN(str, PHOTO_ZOOM_STEP_COUNT, STR_CONV_MODE_LEFT_ALIGN, 1);

    FillWindowPixelBuffer(WIN_TEXT, PIXEL_FILL(1));
    AddTextPrinterParameterized3(WIN_TEXT, FONT_SMALL, 4, 1, sColors, TEXT_SKIP_DRAW, gStringVar4);
    if (line2)
        AddTextPrinterParameterized3(WIN_TEXT, FONT_SMALL, 4, 13, sColors, TEXT_SKIP_DRAW, line2);
    if (line3)
        AddTextPrinterParameterized3(WIN_TEXT, FONT_SMALL, 4, 25, sColors, TEXT_SKIP_DRAW, line3);
    CopyWindowToVram(WIN_TEXT, COPYWIN_FULL);
}

static void PrintControls(void)
{
    PrintLines(COMPOUND_STRING("D-pad: move frame   L/R: zoom in/out"),
               sVf->shotsLeft == 0 ? COMPOUND_STRING("No shots left.   B: back") : COMPOUND_STRING("A: take photo   B: back"));
}

static void CB2_InitViewfinder(void)
{
    u32 i;

    switch (gMain.state)
    {
    case 0:
        SetVBlankCallback(NULL);
        SetHBlankCallback(NULL);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        ScanlineEffect_Stop();
        ResetVramOamAndBgCntRegs();
        ResetAllBgsCoordinates();
        DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000);
        DmaClear32(3, (void *)OAM, OAM_SIZE);
        DmaClear16(3, (void *)PLTT, PLTT_SIZE);
        ResetSpriteData();
        ResetPaletteFade();
        FreeAllSpritePalettes();
        ResetAllPicSprites();
        gMain.state++;
        break;
    case 1:
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
        sVf->tilemapBuf = AllocZeroed(BG_SCREEN_SIZE);
        SetBgTilemapBuffer(0, sVf->tilemapBuf);
        InitWindows(sWindowTemplates);
        DeactivateAllTextPrinters();
        LoadViewfinderBackground();
        Menu_LoadStdPalAt(BG_PLTT_ID(15));
        PutWindowTilemap(WIN_TEXT);
        gMain.state++;
        break;
    case 2:
    {
        bool32 isShiny = sVf->velX;
        u32 personality = sVf->velY;
        u32 speed = PHOTO_SPEED_NUMBER(gSpeciesInfo[sVf->species].baseSpeed, gSpeciesInfo[sVf->species].catchRate);

        LoadSpriteSheet(&sCornerSheet);
        LoadSpriteSheet(&sCrossSheet);
        LoadSpritePalette(&sFrameSpritePal);
        for (i = 0; i < 4; i++)
        {
            struct Sprite *corner;
            sVf->cornerSpriteIds[i] = CreateSprite(&sCornerTemplate, 0, 0, 0);
            corner = &gSprites[sVf->cornerSpriteIds[i]];
            corner->hFlip = i & 1;
            corner->vFlip = i >> 1;
            corner->oam.matrixNum = (corner->hFlip ? ST_OAM_HFLIP : 0) | (corner->vFlip ? ST_OAM_VFLIP : 0);
        }
        sVf->crossSpriteId = CreateSprite(&sCrossTemplate, 0, 0, 0);
        sVf->centreSpriteId = MAX_SPRITES;
        if (PHOTO_SHOW_MON_CENTRE)
        {
            LoadSpriteSheet(&sCentreSheet);
            sVf->centreSpriteId = CreateSprite(&sCentreTemplate, 0, 0, 0);
        }

        sVf->monX = (40 + Random() % 160) << 8;
        sVf->monY = (40 + Random() % 40) << 8;
        sVf->monSpriteId = CreateMonPicSprite_Affine(sVf->species, isShiny, personality, F_MON_PIC_NO_AFFINE | MON_PIC_AFFINE_FRONT,
                                                     sVf->monX >> 8, sVf->monY >> 8, MON_PAL_SLOT, TAG_NONE);
        gSprites[sVf->monSpriteId].oam.priority = 1;
        gSprites[sVf->monSpriteId].callback = SpriteCallbackDummy;
        sVf->dispMatrixNum = AllocOamMatrix();
        MeasureMonPic();

        if (speed < PHOTO_MIN_SPEED_NUMBER)
            speed = PHOTO_MIN_SPEED_NUMBER;
        // Diagonal: each axis gets speed / sqrt(2).
        sVf->velX = (s32)(speed * PHOTO_SPEED_TO_SUBPIXELS * 181 / 256);
        sVf->velY = sVf->velX;
        if (Random() & 1)
            sVf->velX = -sVf->velX;
        if (Random() & 1)
            sVf->velY = -sVf->velY;
        sVf->facingRight = sVf->velX > 0;
        sVf->animTimer = gSpeciesInfo[sVf->species].frontAnimDelay + 30;

        sVf->frameX = DISPLAY_WIDTH / 2;
        sVf->frameY = PHOTO_PLAY_AREA_BOTTOM / 2;
        UpdateFrameGfx();
        PrintControls();
        gMain.state++;
        break;
    }
    case 3:
        // Outside the frame (window 0) the background and sprites are darkened; the text box isn't.
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG_ALL | WININ_WIN0_OBJ);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_ALL);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG3 | BLDCNT_TGT1_OBJ | BLDCNT_EFFECT_DARKEN);
        SetGpuReg(REG_OFFSET_BLDY, PHOTO_OUTSIDE_DARKEN);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP | DISPCNT_WIN0_ON);
        ShowBg(0);
        ShowBg(3);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB_Viewfinder);
        SetMainCallback2(CB2_Viewfinder);
        break;
    }
}

static void UpdateMovement(void)
{
    s32 left, right, top, bottom;

    sVf->monX += sVf->velX;
    sVf->monY += sVf->velY;
    GetMonScreenBox(&left, &right, &top, &bottom, FALSE);
    if ((left < 0 && sVf->velX < 0) || (right > DISPLAY_WIDTH && sVf->velX > 0))
        sVf->velX = -sVf->velX;
    if ((top < 0 && sVf->velY < 0) || (bottom > PHOTO_PLAY_AREA_BOTTOM && sVf->velY > 0))
        sVf->velY = -sVf->velY;
    // Front pics face left; flip to face the way it's moving.
    sVf->facingRight = sVf->velX > 0;
    gSprites[sVf->monSpriteId].x = sVf->monX >> 8;
    gSprites[sVf->monSpriteId].y = sVf->monY >> 8;
}

// Loops the species' own front animation (the Summary screen version of it).
static void UpdateFrontAnim(void)
{
    struct Sprite *sprite = &gSprites[sVf->monSpriteId];
    u32 i;

    if (sVf->animRunning)
    {
        if (sprite->callback == SpriteCallbackDummy)
        {
            sVf->animRunning = FALSE;
            sVf->lastAnimPeak = sVf->animPeak;
            sVf->animPeak = 0;
            sVf->animTimer = PHOTO_ANIM_REPEAT_FRAMES;
        }
        return;
    }
    if (sVf->animTimer)
    {
        sVf->animTimer--;
        return;
    }
    for (i = 0; i < ARRAY_COUNT(sprite->data); i++)
        sprite->data[i] = 0;
    sprite->data[1] = TRUE; // sDontFlip: the animation stays unmirrored; the viewfinder mirrors it.
    if (HasTwoFramesAnimation(sVf->species))
        StartSpriteAnim(sprite, 1);
    StartMonSummaryAnimation(sprite, gSpeciesInfo[sVf->species].frontAnimId);
    sVf->animRunning = TRUE;
}

static bool32 IsOnSecondFrame(struct Sprite *sprite)
{
    return sprite->anims[sprite->animNum][sprite->animCmdIndex].frame.imageValue == 1;
}

static bool32 IsFacingFrameCentre(void)
{
    s32 x = sVf->monX >> 8;
    return (sVf->facingRight && x < sVf->frameX) || (!sVf->facingRight && x > sVf->frameX);
}

// Moves the red "x" to the point the Centre score measures from: the middle of the POKéMON's visible
// pixels, zoomed, including its animation's movement (same box as ScoreShot).
static void UpdateMonCentreMarker(void)
{
    s32 left, right, top, bottom;

    if (sVf->centreSpriteId == MAX_SPRITES)
        return;
    GetMonScreenBox(&left, &right, &top, &bottom, TRUE);
    gSprites[sVf->centreSpriteId].x = (left + right) / 2;
    gSprites[sVf->centreSpriteId].y = (top + bottom) / 2;
}

static void ScoreShot(struct PhotoScore *score)
{
    s32 left, right, top, bottom, dx, dy, n;
    s32 fl = sVf->frameX - PHOTO_FRAME_WIDTH / 2, fr = sVf->frameX + PHOTO_FRAME_WIDTH / 2;
    s32 ft = sVf->frameY - PHOTO_FRAME_HEIGHT / 2, fb = sVf->frameY + PHOTO_FRAME_HEIGHT / 2;

    memset(score, 0, sizeof(*score));
    GetMonScreenBox(&left, &right, &top, &bottom, TRUE);
    score->inFrame = left < fr && right > fl && top < fb && bottom > ft;
    if (!score->inFrame)
        return;

    // Centring: distance from the frame centre, 256 = at the frame's edge (the further of x and y counts).
    dx = abs((left + right) / 2 - sVf->frameX) * 256 / (PHOTO_FRAME_WIDTH / 2);
    dy = abs((top + bottom) / 2 - sVf->frameY) * 256 / (PHOTO_FRAME_HEIGHT / 2);
    n = max(dx, dy);
    if (n > 256)
        n = 256;
    score->centre = PHOTO_CENTRE_MAX * (256 - n) / 256;

    // Size: needs at least PHOTO_SIZE_MIN_INSIDE_PERCENT of the POKéMON's box inside the frame. Tall or wide
    // POKéMON at full zoom fill most of the frame, and their animation (e.g. Hoothoot's slide) moves them
    // a few pixels, so requiring every pixel made full-zoom size points nearly impossible.
    {
        s32 boxArea = (right - left) * (bottom - top);
        s32 insideArea = (min(right, fr) - max(left, fl)) * (min(bottom, fb) - max(top, ft));
        if (boxArea > 0 && insideArea * 100 >= boxArea * PHOTO_SIZE_MIN_INSIDE_PERCENT)
            score->size = sSizePoints[sVf->zoomStep];
    }

    if (sVf->poseAtPeak || IsOnSecondFrame(&gSprites[sVf->monSpriteId]))
        score->pose = PHOTO_POSE_PEAK;
    else if (IsFacingFrameCentre())
        score->pose = PHOTO_POSE_FACING_CENTRE;

    score->total = score->centre + score->size + score->pose;
    if (score->total >= PHOTO_AMAZING_SCORE)
        score->grade = PHOTO_GRADE_AMAZING;
    else if (score->total >= PHOTO_GOOD_SCORE)
        score->grade = PHOTO_GRADE_GOOD;
    else
        score->grade = PHOTO_GRADE_PHOTOGRAPHED;
}

static u8 *AppendNum(u8 *str, const u8 *label, u32 num)
{
    str = StringCopy(str, label);
    return ConvertIntToDecimalStringN(str, num, STR_CONV_MODE_LEFT_ALIGN, 3);
}

static void TakePhoto(void)
{
    struct PhotoScore score;
    u8 line2[64], line3[64];
    u8 *str;
    bool32 upgraded;

    ScoreShot(&score);
    upgraded = TryUpgradePhotoGrade(sVf->species, score.grade);
    if (sVf->shotsLeft != UNLIMITED_SHOTS)
    {
        sVf->shotsLeft--;
        if (sVf->inBattle)
            gBattleStruct->photoShotsUsed++;
    }
    sVf->shotsThisVisit++;
    sVf->cooldown = PHOTO_SHOT_COOLDOWN;
    if (sVf->shotsLeft == 0)
        sVf->closeTimer = PHOTO_AUTO_CLOSE_FRAMES;
    PlaySE(SE_CLICK);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 12, 0, RGB_WHITE);

    str = AppendNum(line2, COMPOUND_STRING("Centre "), score.centre);
    str = AppendNum(str, COMPOUND_STRING(" + Size "), score.size);
    str = AppendNum(str, COMPOUND_STRING(" + Pose "), score.pose);
    AppendNum(str, COMPOUND_STRING(" = "), score.total);

    if (!score.inFrame)
        str = StringCopy(line3, COMPOUND_STRING("Missed! Not in the frame."));
    else
        str = StringCopy(StringCopy(line3, COMPOUND_STRING("Grade: ")), GetPhotoGradeName(score.grade));
    str = StringCopy(str, COMPOUND_STRING("   Best: "));
    str = StringCopy(str, GetPhotoGradeName(GetPhotoGrade(sVf->species)));
    if (upgraded)
        StringCopy(str, COMPOUND_STRING(" (new!)"));
    PrintLines(line2, line3);
}

static void StartClosing(void)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    sVf->state = VF_STATE_EXIT;
}

static void HandleInput(void)
{
    // Out of shots: the camera closes by itself after the result has been shown (A or B closes it sooner).
    if (sVf->closeTimer)
    {
        if (--sVf->closeTimer == 0 || (sVf->cooldown == 0 && JOY_NEW(A_BUTTON | B_BUTTON)))
        {
            sVf->closeTimer = 0;
            StartClosing();
            return;
        }
    }
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        StartClosing();
        return;
    }
    if (JOY_HELD(DPAD_LEFT))
        sVf->frameX -= PHOTO_FRAME_MOVE_SPEED;
    else if (JOY_HELD(DPAD_RIGHT))
        sVf->frameX += PHOTO_FRAME_MOVE_SPEED;
    if (JOY_HELD(DPAD_UP))
        sVf->frameY -= PHOTO_FRAME_MOVE_SPEED;
    else if (JOY_HELD(DPAD_DOWN))
        sVf->frameY += PHOTO_FRAME_MOVE_SPEED;
    sVf->frameX = max(PHOTO_FRAME_WIDTH / 2, min(sVf->frameX, DISPLAY_WIDTH - PHOTO_FRAME_WIDTH / 2));
    sVf->frameY = max(PHOTO_FRAME_HEIGHT / 2, min(sVf->frameY, PHOTO_PLAY_AREA_BOTTOM - PHOTO_FRAME_HEIGHT / 2));
    UpdateFrameGfx();

    if (JOY_NEW(L_BUTTON) && sVf->zoomStep < PHOTO_ZOOM_STEP_COUNT - 1)
    {
        sVf->zoomStep++;
        PlaySE(SE_SELECT);
        PrintControls();
    }
    else if (JOY_NEW(R_BUTTON) && sVf->zoomStep > 0)
    {
        sVf->zoomStep--;
        PlaySE(SE_SELECT);
        PrintControls();
    }

    if (sVf->cooldown)
        sVf->cooldown--;
    else if (JOY_NEW(A_BUTTON) && sVf->shotsLeft != 0)
        TakePhoto();
}

// Display transform, applied just for building OAM so the animation code never sees it:
// the animation's own matrix (if any) x zoom, mirrored when facing right, on a second matrix.
// Also tracks how far the animation currently moves the POKéMON, for the pose score.
static void BuildViewfinderOam(void)
{
    struct Sprite *sprite = &gSprites[sVf->monSpriteId];
    struct OamMatrix m = { 256, 0, 0, 256 };
    s32 z = sZoomSteps[sVf->zoomStep];
    u8 savedMode = sprite->oam.affineMode, savedMatrix = sprite->oam.matrixNum;
    s8 savedCcX = sprite->centerToCornerVecX, savedCcY = sprite->centerToCornerVecY;
    s16 savedX2 = sprite->x2, savedY2 = sprite->y2;

    if (sVf->animRunning && savedMode == ST_OAM_AFFINE_DOUBLE)
        m = gOamMatrices[savedMatrix];

    if (sVf->animRunning)
    {
        u32 move = abs(savedX2) + abs(savedY2) + (abs(m.a - 256) + abs(m.b) + abs(m.c) + abs(m.d - 256)) / 8;
        u32 peak;
        if (move > sVf->animPeak)
            sVf->animPeak = move;
        peak = max(sVf->animPeak, sVf->lastAnimPeak);
        sVf->poseAtPeak = move >= PHOTO_POSE_PEAK_MIN && move * 100 >= peak * PHOTO_POSE_PEAK_PERCENT;
    }
    else
    {
        sVf->poseAtPeak = FALSE;
    }

    m.a = m.a * 256 / z;
    m.b = m.b * 256 / z;
    m.c = m.c * 256 / z;
    m.d = m.d * 256 / z;
    sVf->dispX2 = savedX2 * z / 256;
    sVf->dispY2 = savedY2 * z / 256;
    if (sVf->facingRight)
    {
        m.a = -m.a;
        m.b = -m.b;
        sVf->dispX2 = -sVf->dispX2;
    }
    gOamMatrices[sVf->dispMatrixNum] = m;

    sprite->x2 = sVf->dispX2;
    sprite->y2 = sVf->dispY2;
    sprite->oam.affineMode = ST_OAM_AFFINE_DOUBLE;
    sprite->oam.matrixNum = sVf->dispMatrixNum;
    CalcCenterToCornerVec(sprite, sprite->oam.shape, sprite->oam.size, ST_OAM_AFFINE_DOUBLE);

    BuildOamBuffer();

    sprite->x2 = savedX2;
    sprite->y2 = savedY2;
    sprite->oam.affineMode = savedMode;
    sprite->oam.matrixNum = savedMatrix;
    sprite->centerToCornerVecX = savedCcX;
    sprite->centerToCornerVecY = savedCcY;
}

static void ExitViewfinder(void)
{
    struct Sprite *sprite = &gSprites[sVf->monSpriteId];
    MainCallback returnCallback = sVf->returnCallback;
    bool32 inBattle = sVf->inBattle;
    bool32 tookPhotos = sVf->shotsThisVisit != 0;

    // An animation still running holds its own matrix.
    if (sVf->animRunning && (sprite->oam.affineMode & ST_OAM_AFFINE_ON_MASK))
        FreeOamMatrix(sprite->oam.matrixNum);
    sprite->oam.affineMode = ST_OAM_AFFINE_OFF;
    sprite->oam.matrixNum = 0;
    sprite->callback = SpriteCallbackDummy;
    FreeOamMatrix(sVf->dispMatrixNum);
    FreeAndDestroyMonPicSprite(sVf->monSpriteId);
    ResetSpriteData();
    FreeAllSpritePalettes();
    FreeAllWindowBuffers();
    Free(sVf->tilemapBuf);
    for (u32 i = 0; i < ARRAY_COUNT(sSavedGpuRegOffsets); i++)
        SetGpuReg(sSavedGpuRegOffsets[i], sVf->savedGpuRegs[i]);
    // The battle screen redraw keeps DISPCNT as it finds it: turn the frame window off (as the Bag does),
    // or everything stays hidden and the battle shows black.
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    FREE_AND_SET_NULL(sVf);
    // Grades were saved shot by shot. In battle, taking photos uses the player's turn
    // (HandleAction_UseItem runs BattleScript_UseCamera); backing out without a shot doesn't.
    if (inBattle)
        gSpecialVar_ItemId = tookPhotos ? ITEM_CAMERA : ITEM_NONE;
    SetMainCallback2(returnCallback);
}

static void CB2_Viewfinder(void)
{
    if (sVf->state == VF_STATE_EXIT)
    {
        if (!gPaletteFade.active)
        {
            ExitViewfinder();
            return;
        }
    }
    else
    {
        HandleInput();
    }
    if (sVf->state == VF_STATE_RUN)
    {
        UpdateMovement();
        UpdateFrontAnim();
    }
    AnimateSprites();
    BuildViewfinderOam();
    UpdateMonCentreMarker();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}
