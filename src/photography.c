#include "global.h"
#include "photography.h"
#include "battle.h"
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

// Pokédex entry line.
const u8 *GetPhotoGradeDexText(enum Species species)
{
    switch (GetPhotoGrade(species))
    {
    case PHOTO_GRADE_PHOTOGRAPHED: return COMPOUND_STRING("Photo: Taken");
    case PHOTO_GRADE_GOOD:         return COMPOUND_STRING("Photo: Good");
    case PHOTO_GRADE_AMAZING:      return COMPOUND_STRING("Photo: Amazing");
    default:                       return COMPOUND_STRING("Photo: ---");
    }
}

// ---------------------------------------------------------------------------
// Viewfinder

#define TAG_PHOTO_CORNER 0x5A10
#define TAG_PHOTO_CROSS  0x5A11
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
    u8 state;
    u8 monSpriteId;
    u8 cornerSpriteIds[4];
    u8 crossSpriteId;
    u8 dispMatrixNum;
    u8 zoomStep;
    u8 shotsLeft;
    bool8 facingRight;
    bool8 animRunning;
    bool8 poseAtPeak;
    u16 animTimer;
    u16 cooldown;
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
static const u16 sFramePal[16] = { RGB(0, 0, 0), RGB_WHITE, RGB(5, 5, 6) };

static const struct SpriteSheet sCornerSheet = { sCornerGfx, sizeof(sCornerGfx), TAG_PHOTO_CORNER };
static const struct SpriteSheet sCrossSheet = { sCrossGfx, sizeof(sCrossGfx), TAG_PHOTO_CROSS };
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

static void CB2_InitViewfinder(void);
static void CB2_Viewfinder(void);

static void StartViewfinder(enum Species species, bool32 isShiny, u32 personality, bool32 inBattle, u32 shotsLeft, MainCallback returnCallback)
{
    sVf = AllocZeroed(sizeof(*sVf));
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

    if (left >= fl && right <= fr && top >= ft && bottom <= fb)
        score->size = sSizePoints[sVf->zoomStep];

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
    sVf->cooldown = PHOTO_SHOT_COOLDOWN;
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

static void HandleInput(void)
{
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        sVf->state = VF_STATE_EXIT;
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
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDY, 0);
    SetGpuReg(REG_OFFSET_WININ, 0);
    SetGpuReg(REG_OFFSET_WINOUT, 0);
    FREE_AND_SET_NULL(sVf);
    if (inBattle)
        gSpecialVar_ItemId = ITEM_NONE; // Back to the action menu; no turn used.
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
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}
