#include "global.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "event_data.h"
#include "gpu_regs.h"
#include "international_string_util.h"
#include "main.h"
#include "overworld.h"
#include "script.h"
#include "string_util.h"
#include "menu.h"
#include "palette.h"
#include "pokedex.h"
#include "pokemon.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "starter_choose.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "trainer_pokemon_sprites.h"
#include "trig.h"
#include "window.h"
#include "move.h"
#include "constants/songs.h"
#include "constants/rgb.h"



// Position of the sprite of the selected starter Pokémon
#define STARTER_PKMN_POS_X (DISPLAY_WIDTH / 2)
#define STARTER_PKMN_POS_Y 64

#define TAG_POKEBALL_SELECT 0x1000
#define TAG_STARTER_CIRCLE  0x1001

static void CB2_StarterChoose(void);
static void ClearStarterLabel(void);
static void Task_StarterChoose(u8 taskId);
static void Task_HandleStarterChooseInput(u8 taskId);
static void Task_WaitForStarterSprite(u8 taskId);
static void Task_AskConfirmStarter(u8 taskId);
static void Task_HandleConfirmStarterInput(u8 taskId);
static void Task_DeclineStarter(u8 taskId);
static void Task_MoveStarterChooseCursor(u8 taskId);
static void Task_CreateStarterLabel(u8 taskId);
static void CreateStarterPokemonLabel(u8 region, u8 selection);
static void Task_ShowRegionMenu(u8 taskId);
static void Task_HandleRegionMenuInput(u8 taskId);
static void SetBallSpritesVisible(u8 taskId, bool32 visible);
static u8 CreatePokemonFrontSprite(enum Species species, u8 x, u8 y);
static void SpriteCB_SelectionHand(struct Sprite *sprite);
static void SpriteCB_Pokeball(struct Sprite *sprite);
static void SpriteCB_StarterPokemon(struct Sprite *sprite);

static u16 sStarterLabelWindowId;

const u16 gBirchBagGrass_Pal[] = INCGFX_U16("graphics/starter_choose/tiles.png", ".gbapal");
static const u16 sPokeballSelection_Pal[] = INCGFX_U16("graphics/starter_choose/pokeball_selection.png", ".gbapal");
static const u16 sStarterCircle_Pal[] = INCGFX_U16("graphics/starter_choose/starter_circle.png", ".gbapal");
const u32 gBirchBagTilemap[] = INCGFX_U32("graphics/starter_choose/birch_bag.bin", ".smolTM");
const u32 gBirchGrassTilemap[] = INCGFX_U32("graphics/starter_choose/birch_grass.bin", ".smolTM");
const u32 gBirchBagGrass_Gfx[] = INCGFX_U32("graphics/starter_choose/tiles.png", ".4bpp.smol");
const u32 gPokeballSelection_Gfx[] = INCGFX_U32("graphics/starter_choose/pokeball_selection.png", ".4bpp.smol");
static const u32 sStarterCircle_Gfx[] = INCGFX_U32("graphics/starter_choose/starter_circle.png", ".4bpp.smol");

static const struct WindowTemplate sWindowTemplates[] =
{
    {
        .bg = 0,
        .tilemapLeft = 3,
        .tilemapTop = 15,
        .width = 24,
        .height = 4,
        .paletteNum = 14,
        .baseBlock = 0x0200
    },
    DUMMY_WIN_TEMPLATE,
};

static const struct WindowTemplate sWindowTemplate_ConfirmStarter =
{
    .bg = 0,
    .tilemapLeft = 24,
    .tilemapTop = 9,
    .width = 5,
    .height = 4,
    .paletteNum = 14,
    .baseBlock = 0x0260
};

static const struct WindowTemplate sWindowTemplate_StarterLabel =
{
    .bg = 0,
    .tilemapLeft = 0,
    .tilemapTop = 0,
    .width = 13,
    .height = 4,
    .paletteNum = 14,
    .baseBlock = 0x0274
};

static const u8 sPokeballCoords[STARTER_MON_COUNT][2] =
{
    {60, 64},
    {120, 88},
    {180, 64},
};

static const u8 sStarterLabelCoords[STARTER_MON_COUNT][2] =
{
    {0, 9},
    {16, 10},
    {8, 4},
};

// Starter regions. To add a region: add a STARTER_REGION_* constant (starter_choose.h) and a row here.
// Each region lists its starters in slot order Grass, Fire, Water (the slot is saved in VAR_STARTER_MON),
// with each line's evolution stages so rival teams can swap in the matching stage.
struct StarterRegion
{
    const u8 *name;
    u16 lines[STARTER_MON_COUNT][STARTER_LINE_STAGES];
};

static const struct StarterRegion sStarterRegions[STARTER_REGION_COUNT] =
{
    [STARTER_REGION_HOENN] =
    {
        .name = COMPOUND_STRING("HOENN"),
        .lines = {
            { SPECIES_TREECKO, SPECIES_GROVYLE,   SPECIES_SCEPTILE },
            { SPECIES_TORCHIC, SPECIES_COMBUSKEN, SPECIES_BLAZIKEN },
            { SPECIES_MUDKIP,  SPECIES_MARSHTOMP, SPECIES_SWAMPERT },
        },
    },
    [STARTER_REGION_KANTO] =
    {
        .name = COMPOUND_STRING("KANTO"),
        .lines = {
            { SPECIES_BULBASAUR,  SPECIES_IVYSAUR,    SPECIES_VENUSAUR  },
            { SPECIES_CHARMANDER, SPECIES_CHARMELEON, SPECIES_CHARIZARD },
            { SPECIES_SQUIRTLE,   SPECIES_WARTORTLE,  SPECIES_BLASTOISE },
        },
    },
    [STARTER_REGION_JOHTO] =
    {
        .name = COMPOUND_STRING("JOHTO"),
        .lines = {
            { SPECIES_CHIKORITA, SPECIES_BAYLEEF,  SPECIES_MEGANIUM   },
            { SPECIES_CYNDAQUIL, SPECIES_QUILAVA,  SPECIES_TYPHLOSION },
            { SPECIES_TOTODILE,  SPECIES_CROCONAW, SPECIES_FERALIGATR },
        },
    },
};

static const struct WindowTemplate sWindowTemplate_Region =
{
    .bg = 0,
    .tilemapLeft = 22,
    .tilemapTop = 7,
    .width = 7,
    .height = STARTER_REGION_COUNT * 2,
    .paletteNum = 14,
    .baseBlock = 0x02B4 // after the frame tiles at 0x2A8
};

static const struct BgTemplate sBgTemplates[3] =
{
    {
        .bg = 0,
        .charBaseIndex = 2,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0
    },
    {
        .bg = 2,
        .charBaseIndex = 0,
        .mapBaseIndex = 7,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 3,
        .baseTile = 0
    },
    {
        .bg = 3,
        .charBaseIndex = 0,
        .mapBaseIndex = 6,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0
    },
};

static const u8 sTextColors[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_LIGHT_GRAY};

static const struct OamData sOam_Hand =
{
    .y = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x32),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct OamData sOam_Pokeball =
{
    .y = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x32),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct OamData sOam_StarterCircle =
{
    .y = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_DOUBLE,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const u8 sCursorCoords[][2] =
{
    {60, 32},
    {120, 56},
    {180, 32},
};

static const union AnimCmd sAnim_Hand[] =
{
    ANIMCMD_FRAME(48, 30),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_Pokeball_Still[] =
{
    ANIMCMD_FRAME(0, 30),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_Pokeball_Moving[] =
{
    ANIMCMD_FRAME(16, 4),
    ANIMCMD_FRAME(0, 4),
    ANIMCMD_FRAME(32, 4),
    ANIMCMD_FRAME(0, 4),
    ANIMCMD_FRAME(16, 4),
    ANIMCMD_FRAME(0, 4),
    ANIMCMD_FRAME(32, 4),
    ANIMCMD_FRAME(0, 4),
    ANIMCMD_FRAME(0, 32),
    ANIMCMD_FRAME(16, 8),
    ANIMCMD_FRAME(0, 8),
    ANIMCMD_FRAME(32, 8),
    ANIMCMD_FRAME(0, 8),
    ANIMCMD_FRAME(16, 8),
    ANIMCMD_FRAME(0, 8),
    ANIMCMD_FRAME(32, 8),
    ANIMCMD_FRAME(0, 8),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sAnim_StarterCircle[] =
{
    ANIMCMD_FRAME(0, 8),
    ANIMCMD_END,
};

static const union AnimCmd *const sAnims_Hand[] =
{
    sAnim_Hand,
};

static const union AnimCmd *const sAnims_Pokeball[] =
{
    sAnim_Pokeball_Still,
    sAnim_Pokeball_Moving,
};

static const union AnimCmd *const sAnims_StarterCircle[] =
{
    sAnim_StarterCircle,
};

static const union AffineAnimCmd sAffineAnim_StarterPokemon[] =
{
    AFFINEANIMCMD_FRAME(16, 16, 0, 0),
    AFFINEANIMCMD_FRAME(16, 16, 0, 15),
    AFFINEANIMCMD_END,
};

static const union AffineAnimCmd sAffineAnim_StarterCircle[] =
{
    AFFINEANIMCMD_FRAME(20, 20, 0, 0),
    AFFINEANIMCMD_FRAME(20, 20, 0, 15),
    AFFINEANIMCMD_END,
};

static const union AffineAnimCmd *const sAffineAnims_StarterPokemon = {sAffineAnim_StarterPokemon};
static const union AffineAnimCmd *const sAffineAnims_StarterCircle[] = {sAffineAnim_StarterCircle};

static const struct CompressedSpriteSheet sSpriteSheet_PokeballSelect[] =
{
    {
        .data = gPokeballSelection_Gfx,
        .size = 0x0800,
        .tag = TAG_POKEBALL_SELECT
    },
    {}
};

static const struct CompressedSpriteSheet sSpriteSheet_StarterCircle[] =
{
    {
        .data = sStarterCircle_Gfx,
        .size = 0x0800,
        .tag = TAG_STARTER_CIRCLE
    },
    {}
};

static const struct SpritePalette sSpritePalettes_StarterChoose[] =
{
    {
        .data = sPokeballSelection_Pal,
        .tag = TAG_POKEBALL_SELECT
    },
    {
        .data = sStarterCircle_Pal,
        .tag = TAG_STARTER_CIRCLE
    },
    {},
};

static const struct SpriteTemplate sSpriteTemplate_Hand =
{
    .tileTag = TAG_POKEBALL_SELECT,
    .paletteTag = TAG_POKEBALL_SELECT,
    .oam = &sOam_Hand,
    .anims = sAnims_Hand,
    .callback = SpriteCB_SelectionHand
};

static const struct SpriteTemplate sSpriteTemplate_Pokeball =
{
    .tileTag = TAG_POKEBALL_SELECT,
    .paletteTag = TAG_POKEBALL_SELECT,
    .oam = &sOam_Pokeball,
    .anims = sAnims_Pokeball,
    .callback = SpriteCB_Pokeball
};

static const struct SpriteTemplate sSpriteTemplate_StarterCircle =
{
    .tileTag = TAG_STARTER_CIRCLE,
    .paletteTag = TAG_STARTER_CIRCLE,
    .oam = &sOam_StarterCircle,
    .anims = sAnims_StarterCircle,
    .affineAnims = sAffineAnims_StarterCircle,
    .callback = SpriteCB_StarterPokemon
};

// .text
static u32 SanitizeRegion(u32 region)
{
    return (region < STARTER_REGION_COUNT) ? region : STARTER_REGION_HOENN;
}

u16 GetRegionStarterPokemon(u32 region, u32 slot)
{
    if (slot >= STARTER_MON_COUNT)
        slot = 0;
    return sStarterRegions[SanitizeRegion(region)].lines[slot][0];
}

u32 GetStarterRegion(void)
{
    if (IS_FRLG)
        return STARTER_REGION_KANTO;
    return SanitizeRegion(VarGet(VAR_STARTER_REGION));
}

// The player's starter, from the saved region and slot.
u16 GetStarterPokemon(u16 chosenStarterId)
{
    return GetRegionStarterPokemon(GetStarterRegion(), chosenStarterId);
}

const u8 *GetStarterRegionName(u32 region)
{
    return sStarterRegions[SanitizeRegion(region)].name;
}

// Rival's slot: the type that beats the player's (Grass -> Fire -> Water -> Grass).
u32 GetRivalStarterSlot(u32 playerSlot)
{
    return (playerSlot + 1) % STARTER_MON_COUNT;
}

u16 GetRivalStarterPokemon(void)
{
    return GetRegionStarterPokemon(GetStarterRegion(), GetRivalStarterSlot(VarGet(VAR_STARTER_MON)));
}

// Rival teams in trainers.party are written with the Hoenn starter lines. If the player picked
// another region, swap each Hoenn starter-line mon for the same slot and stage of that region.
// The rest of the team is unchanged.
void SwapRivalStarterLineForRegion(struct Pokemon *party, u32 monsCount)
{
    u32 region = GetStarterRegion();
    u32 i, slot, stage;

    if (region == STARTER_REGION_HOENN)
        return;

    for (i = 0; i < monsCount; i++)
    {
        enum Species species = GetMonData(&party[i], MON_DATA_SPECIES);
        for (slot = 0; slot < STARTER_MON_COUNT; slot++)
        {
            for (stage = 0; stage < STARTER_LINE_STAGES; stage++)
            {
                if (sStarterRegions[STARTER_REGION_HOENN].lines[slot][stage] == species)
                {
                    u16 newSpecies = sStarterRegions[region].lines[slot][stage];
                    SetMonData(&party[i], MON_DATA_SPECIES, &newSpecies);
                    SetMonData(&party[i], MON_DATA_NICKNAME, GetSpeciesName(newSpecies));
                    CalculateMonStats(&party[i]);
                    GiveMonInitialMoveset(&party[i]); // rival teams use default level-up moves
                    slot = STARTER_MON_COUNT; // done with this mon
                    break;
                }
            }
        }
    }
}

// Trainers row 1827: the first rival's starter gets no move of its own type this early, only its
// Normal-type level-up moves (every starter has Tackle, Scratch or Pound by then).
static void KeepOnlyNormalMoves(struct Pokemon *mon)
{
    u16 keep[MAX_MON_MOVES];
    u32 i, count = 0;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move move = GetMonData(mon, MON_DATA_MOVE1 + i);
        if (move != MOVE_NONE && GetMoveType(move) == TYPE_NORMAL)
            keep[count++] = move;
    }
    if (count == 0)
        return;
    for (i = 0; i < MAX_MON_MOVES; i++)
        SetMonMoveSlot(mon, i < count ? keep[i] : MOVE_NONE, i);
}

// Opening 09: the Littleroot rival's first Pokémon is the rival's starter (counter to the player's,
// same region), whatever trainers.party lists in that slot.
void SetMonToRivalStarter(struct Pokemon *mon)
{
    u16 species = GetRivalStarterPokemon();
    SetMonData(mon, MON_DATA_SPECIES, &species);
    SetMonData(mon, MON_DATA_NICKNAME, GetSpeciesName(species));
    CalculateMonStats(mon);
    GiveMonInitialMoveset(mon);
    KeepOnlyNormalMoves(mon);
}

// Debug: re-run the starter screen without the Route 101 scene or battle, then report the result
// (region, player's starter, rival's starter) in gStringVar1-3. The Pokémon is not given.
static void CB2_DebugStarterChosen(void)
{
    VarSet(VAR_STARTER_MON, gSpecialVar_Result); // the screen already saved VAR_STARTER_REGION
    StringCopy(gStringVar1, GetStarterRegionName(GetStarterRegion()));
    StringCopy(gStringVar2, GetSpeciesName(GetStarterPokemon(gSpecialVar_Result)));
    StringCopy(gStringVar3, GetSpeciesName(GetRivalStarterPokemon()));
    SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
}

void Debug_ChooseStarterAgain(struct ScriptContext *ctx)
{
    VarSet(VAR_STARTER_REGION, STARTER_REGION_HOENN);
    VarSet(VAR_STARTER_MON, 0);
    SetMainCallback2(CB2_ChooseStarter);
    gMain.savedCallback = CB2_DebugStarterChosen;
}

static void VblankCB_StarterChoose(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

// Data for Task_StarterChoose
#define tStarterSelection   data[0]
#define tPkmnSpriteId       data[1]
#define tCircleSpriteId     data[2]
#define tRegion             data[3]
#define tRegionWindowId     data[4]
#define tHandSpriteId       data[5]
#define tBallSpriteId(i)    data[6 + (i)]

// Data for sSpriteTemplate_Pokeball
#define sTaskId data[0]
#define sBallId data[1]

void CB2_ChooseStarter(void)
{
    u8 taskId;
    u8 spriteId;

    SetVBlankCallback(NULL);

    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    SetGpuReg(REG_OFFSET_BG3CNT, 0);
    SetGpuReg(REG_OFFSET_BG2CNT, 0);
    SetGpuReg(REG_OFFSET_BG1CNT, 0);
    SetGpuReg(REG_OFFSET_BG0CNT, 0);

    ChangeBgX(0, 0, BG_COORD_SET);
    ChangeBgY(0, 0, BG_COORD_SET);
    ChangeBgX(1, 0, BG_COORD_SET);
    ChangeBgY(1, 0, BG_COORD_SET);
    ChangeBgX(2, 0, BG_COORD_SET);
    ChangeBgY(2, 0, BG_COORD_SET);
    ChangeBgX(3, 0, BG_COORD_SET);
    ChangeBgY(3, 0, BG_COORD_SET);

    DmaFill16(3, 0, VRAM, VRAM_SIZE);
    DmaFill32(3, 0, OAM, OAM_SIZE);
    DmaFill16(3, 0, PLTT, PLTT_SIZE);

    DecompressDataWithHeaderVram(gBirchBagGrass_Gfx, (void *)VRAM);
    DecompressDataWithHeaderVram(gBirchBagTilemap, (void *)(BG_SCREEN_ADDR(6)));
    DecompressDataWithHeaderVram(gBirchGrassTilemap, (void *)(BG_SCREEN_ADDR(7)));

    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
    InitWindows(sWindowTemplates);

    DeactivateAllTextPrinters();
    LoadUserWindowBorderGfx(0, 0x2A8, BG_PLTT_ID(13));
    ClearScheduledBgCopiesToVram();
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    ResetPaletteFade();
    FreeAllSpritePalettes();
    ResetAllPicSprites();

    LoadPalette(GetOverworldTextboxPalettePtr(), BG_PLTT_ID(14), PLTT_SIZE_4BPP);
    LoadPalette(gBirchBagGrass_Pal, BG_PLTT_ID(0), sizeof(gBirchBagGrass_Pal));
    LoadCompressedSpriteSheet(&sSpriteSheet_PokeballSelect[0]);
    LoadCompressedSpriteSheet(&sSpriteSheet_StarterCircle[0]);
    LoadSpritePalettes(sSpritePalettes_StarterChoose);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0x10, 0, RGB_BLACK);

    EnableInterrupts(DISPSTAT_VBLANK);
    SetVBlankCallback(VblankCB_StarterChoose);
    SetMainCallback2(CB2_StarterChoose);

    SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG_ALL | WININ_WIN0_OBJ | WININ_WIN0_CLR);
    SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG_ALL | WINOUT_WIN01_OBJ);
    SetGpuReg(REG_OFFSET_WIN0H, 0);
    SetGpuReg(REG_OFFSET_WIN0V, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BD | BLDCNT_EFFECT_DARKEN);
    SetGpuReg(REG_OFFSET_BLDALPHA, 0);
    SetGpuReg(REG_OFFSET_BLDY, 7);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);

    ShowBg(0);
    ShowBg(2);
    ShowBg(3);

    // Starts on the region menu; the balls appear once a region is chosen.
    taskId = CreateTask(Task_ShowRegionMenu, 0);
    gTasks[taskId].tStarterSelection = 1;
    gTasks[taskId].tRegion = STARTER_REGION_HOENN;
    gTasks[taskId].tRegionWindowId = WINDOW_NONE;

    // Create hand sprite
    spriteId = CreateSprite(&sSpriteTemplate_Hand, 120, 56, 2);
    gSprites[spriteId].data[0] = taskId;
    gTasks[taskId].tHandSpriteId = spriteId;

    // Create three Poké Ball sprites
    for (u32 i = 0; i < STARTER_MON_COUNT; i++)
    {
        spriteId = CreateSprite(&sSpriteTemplate_Pokeball, sPokeballCoords[i][0], sPokeballCoords[i][1], 2);
        gSprites[spriteId].sTaskId = taskId;
        gSprites[spriteId].sBallId = i;
        gTasks[taskId].tBallSpriteId(i) = spriteId;
    }
    SetBallSpritesVisible(taskId, FALSE);

    sStarterLabelWindowId = WINDOW_NONE;
}

static void CB2_StarterChoose(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void SetBallSpritesVisible(u8 taskId, bool32 visible)
{
    gSprites[gTasks[taskId].tHandSpriteId].invisible = !visible;
    for (u32 i = 0; i < STARTER_MON_COUNT; i++)
        gSprites[gTasks[taskId].tBallSpriteId(i)].invisible = !visible;
}

static void Task_ShowRegionMenu(u8 taskId)
{
    u8 windowId;

    SetBallSpritesVisible(taskId, FALSE);

    DrawStdFrameWithCustomTileAndPalette(0, FALSE, 0x2A8, 0xD);
    AddTextPrinterParameterized(0, FONT_NORMAL, COMPOUND_STRING("PROF. BIRCH is in trouble!\nPick a region's POKéMON!"), 0, 1, 0, NULL);
    PutWindowTilemap(0);

    windowId = AddWindow(&sWindowTemplate_Region);
    gTasks[taskId].tRegionWindowId = windowId;
    DrawStdFrameWithCustomTileAndPalette(windowId, FALSE, 0x2A8, 0xD);
    for (u32 i = 0; i < STARTER_REGION_COUNT; i++)
        AddTextPrinterParameterized(windowId, FONT_NORMAL, sStarterRegions[i].name, 8, i * 16 + 1, TEXT_SKIP_DRAW, NULL);
    InitMenuInUpperLeftCornerNormal(windowId, STARTER_REGION_COUNT, gTasks[taskId].tRegion);
    CopyWindowToVram(windowId, COPYWIN_FULL);
    ScheduleBgCopyTilemapToVram(0);
    gTasks[taskId].func = Task_HandleRegionMenuInput;
}

static void Task_HandleRegionMenuInput(u8 taskId)
{
    s8 input;

    if (JOY_NEW(B_BUTTON)) // B does nothing on the region menu
        return;

    input = Menu_ProcessInputNoWrap();
    if (input < 0)
        return;

    PlaySE(SE_SELECT);
    gTasks[taskId].tRegion = input;
    ClearStdWindowAndFrameToTransparent(gTasks[taskId].tRegionWindowId, TRUE);
    RemoveWindow(gTasks[taskId].tRegionWindowId);
    gTasks[taskId].tRegionWindowId = WINDOW_NONE;
    SetBallSpritesVisible(taskId, TRUE);
    gTasks[taskId].func = Task_StarterChoose;
}

static void Task_StarterChoose(u8 taskId)
{
    CreateStarterPokemonLabel(gTasks[taskId].tRegion, gTasks[taskId].tStarterSelection);
    FillWindowPixelBuffer(0, PIXEL_FILL(1));
    DrawStdFrameWithCustomTileAndPalette(0, FALSE, 0x2A8, 0xD);
    AddTextPrinterParameterized(0, FONT_NORMAL, gText_BirchInTrouble, 0, 1, 0, NULL);
    PutWindowTilemap(0);
    ScheduleBgCopyTilemapToVram(0);
    gTasks[taskId].func = Task_HandleStarterChooseInput;
}

static void Task_HandleStarterChooseInput(u8 taskId)
{
    u8 selection = gTasks[taskId].tStarterSelection;

    if (JOY_NEW(A_BUTTON))
    {
        u8 spriteId;

        ClearStarterLabel();

        // Create white circle background
        spriteId = CreateSprite(&sSpriteTemplate_StarterCircle, sPokeballCoords[selection][0], sPokeballCoords[selection][1], 1);
        gTasks[taskId].tCircleSpriteId = spriteId;

        // Create Pokémon sprite
        spriteId = CreatePokemonFrontSprite(GetRegionStarterPokemon(gTasks[taskId].tRegion, selection), sPokeballCoords[selection][0], sPokeballCoords[selection][1]);
        gSprites[spriteId].affineAnims = &sAffineAnims_StarterPokemon;
        gSprites[spriteId].callback = SpriteCB_StarterPokemon;

        gTasks[taskId].tPkmnSpriteId = spriteId;
        gTasks[taskId].func = Task_WaitForStarterSprite;
    }
    else if (JOY_NEW(DPAD_LEFT) && selection > 0)
    {
        gTasks[taskId].tStarterSelection--;
        gTasks[taskId].func = Task_MoveStarterChooseCursor;
    }
    else if (JOY_NEW(DPAD_RIGHT) && selection < STARTER_MON_COUNT - 1)
    {
        gTasks[taskId].tStarterSelection++;
        gTasks[taskId].func = Task_MoveStarterChooseCursor;
    }
    else if (JOY_NEW(B_BUTTON)) // back to the region menu, cursor on the last region
    {
        PlaySE(SE_SELECT);
        ClearStarterLabel();
        gTasks[taskId].func = Task_ShowRegionMenu;
    }
}

static void Task_WaitForStarterSprite(u8 taskId)
{
    if (gSprites[gTasks[taskId].tCircleSpriteId].affineAnimEnded &&
        gSprites[gTasks[taskId].tCircleSpriteId].x == STARTER_PKMN_POS_X &&
        gSprites[gTasks[taskId].tCircleSpriteId].y == STARTER_PKMN_POS_Y)
    {
        gTasks[taskId].func = Task_AskConfirmStarter;
    }
}

static void Task_AskConfirmStarter(u8 taskId)
{
    PlayCry_Normal(GetRegionStarterPokemon(gTasks[taskId].tRegion, gTasks[taskId].tStarterSelection), 0);
    FillWindowPixelBuffer(0, PIXEL_FILL(1));
    AddTextPrinterParameterized(0, FONT_NORMAL, gText_ConfirmStarterChoice, 0, 1, 0, NULL);
    ScheduleBgCopyTilemapToVram(0);
    CreateYesNoMenu(&sWindowTemplate_ConfirmStarter, 0x2A8, 0xD, 0);
    gTasks[taskId].func = Task_HandleConfirmStarterInput;
}

static void Task_HandleConfirmStarterInput(u8 taskId)
{
    u8 spriteId;

    switch (Menu_ProcessInputNoWrapClearOnChoose())
    {
    case 0:  // YES
        // Return the starter choice and exit.
        gSpecialVar_Result = gTasks[taskId].tStarterSelection;
        VarSet(VAR_STARTER_REGION, gTasks[taskId].tRegion);
        ResetAllPicSprites();
        SetMainCallback2(gMain.savedCallback);
        break;
    case 1:  // NO
    case MENU_B_PRESSED:
        PlaySE(SE_SELECT);
        spriteId = gTasks[taskId].tPkmnSpriteId;
        FreeOamMatrix(gSprites[spriteId].oam.matrixNum);
        FreeAndDestroyMonPicSprite(spriteId);

        spriteId = gTasks[taskId].tCircleSpriteId;
        FreeOamMatrix(gSprites[spriteId].oam.matrixNum);
        DestroySprite(&gSprites[spriteId]);
        gTasks[taskId].func = Task_DeclineStarter;
        break;
    }
}

static void Task_DeclineStarter(u8 taskId)
{
    gTasks[taskId].func = Task_StarterChoose;
}

static void CreateStarterPokemonLabel(u8 region, u8 selection)
{
    u8 categoryText[32];
    struct WindowTemplate winTemplate;
    const u8 *speciesName;
    s32 width;
    u8 labelLeft, labelRight, labelTop, labelBottom;

    enum Species species = GetRegionStarterPokemon(region, selection);
    CopyMonCategoryText(species, categoryText);
    speciesName = GetSpeciesName(species);

    winTemplate = sWindowTemplate_StarterLabel;
    winTemplate.tilemapLeft = sStarterLabelCoords[selection][0];
    winTemplate.tilemapTop = sStarterLabelCoords[selection][1];

    sStarterLabelWindowId = AddWindow(&winTemplate);
    FillWindowPixelBuffer(sStarterLabelWindowId, PIXEL_FILL(0));

    width = GetStringCenterAlignXOffset(FONT_NARROW, categoryText, 0x68);
    AddTextPrinterParameterized3(sStarterLabelWindowId, FONT_NARROW, width, 1, sTextColors, 0, categoryText);

    width = GetStringCenterAlignXOffset(FONT_NORMAL, speciesName, 0x68);
    AddTextPrinterParameterized3(sStarterLabelWindowId, FONT_NORMAL, width, 17, sTextColors, 0, speciesName);

    PutWindowTilemap(sStarterLabelWindowId);
    ScheduleBgCopyTilemapToVram(0);

    labelLeft = sStarterLabelCoords[selection][0] * 8 - 4;
    labelRight = (sStarterLabelCoords[selection][0] + 13) * 8 + 4;
    labelTop = sStarterLabelCoords[selection][1] * 8;
    labelBottom = (sStarterLabelCoords[selection][1] + 4) * 8;
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(labelLeft, labelRight));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(labelTop, labelBottom));
}

static void ClearStarterLabel(void)
{
    FillWindowPixelBuffer(sStarterLabelWindowId, PIXEL_FILL(0));
    ClearWindowTilemap(sStarterLabelWindowId);
    RemoveWindow(sStarterLabelWindowId);
    sStarterLabelWindowId = WINDOW_NONE;
    SetGpuReg(REG_OFFSET_WIN0H, 0);
    SetGpuReg(REG_OFFSET_WIN0V, 0);
    ScheduleBgCopyTilemapToVram(0);
}

static void Task_MoveStarterChooseCursor(u8 taskId)
{
    ClearStarterLabel();
    gTasks[taskId].func = Task_CreateStarterLabel;
}

static void Task_CreateStarterLabel(u8 taskId)
{
    CreateStarterPokemonLabel(gTasks[taskId].tRegion, gTasks[taskId].tStarterSelection);
    gTasks[taskId].func = Task_HandleStarterChooseInput;
}

static u8 CreatePokemonFrontSprite(enum Species species, u8 x, u8 y)
{
    u8 spriteId;

    spriteId = CreateMonPicSprite_Affine(species, FALSE, 0, MON_PIC_AFFINE_FRONT, x, y, 14, TAG_NONE);
    gSprites[spriteId].oam.priority = 0;
    return spriteId;
}

static void SpriteCB_SelectionHand(struct Sprite *sprite)
{
    // Float up and down above selected Poké Ball
    sprite->x = sCursorCoords[gTasks[sprite->data[0]].tStarterSelection][0];
    sprite->y = sCursorCoords[gTasks[sprite->data[0]].tStarterSelection][1];
    sprite->y2 = Sin(sprite->data[1], 8);
    sprite->data[1] = (u8)(sprite->data[1]) + 4;
}

static void SpriteCB_Pokeball(struct Sprite *sprite)
{
    // Animate Poké Ball if currently selected
    if (gTasks[sprite->sTaskId].tStarterSelection == sprite->sBallId)
        StartSpriteAnimIfDifferent(sprite, 1);
    else
        StartSpriteAnimIfDifferent(sprite, 0);
}

static void SpriteCB_StarterPokemon(struct Sprite *sprite)
{
    // Move sprite to upper center of screen
    if (sprite->x > STARTER_PKMN_POS_X)
        sprite->x -= 4;
    if (sprite->x < STARTER_PKMN_POS_X)
        sprite->x += 4;
    if (sprite->y > STARTER_PKMN_POS_Y)
        sprite->y -= 2;
    if (sprite->y < STARTER_PKMN_POS_Y)
        sprite->y += 2;
}
