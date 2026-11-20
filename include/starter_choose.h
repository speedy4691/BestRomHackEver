#ifndef GUARD_STARTER_CHOOSE_H
#define GUARD_STARTER_CHOOSE_H

struct ScriptContext;

#define STARTER_MON_COUNT       3   // starters per region, slot order Grass, Fire, Water
#define STARTER_LINE_STAGES     3   // evolution stages per starter line

// Saved in VAR_STARTER_REGION. Add new regions before STARTER_REGION_COUNT and give them a row
// in sStarterRegions (src/starter_choose.c).
enum StarterRegionId
{
    STARTER_REGION_HOENN,
    STARTER_REGION_KANTO,
    STARTER_REGION_JOHTO,
    STARTER_REGION_COUNT,
};

extern const u16 gBirchBagGrass_Pal[];
extern const u32 gBirchBagTilemap[];
extern const u32 gBirchGrassTilemap[];
extern const u32 gBirchBagGrass_Gfx[];
extern const u32 gPokeballSelection_Gfx[];

u16 GetStarterPokemon(u16 chosenStarterId);
u16 GetRegionStarterPokemon(u32 region, u32 slot);
u32 GetStarterRegion(void);
const u8 *GetStarterRegionName(u32 region);
u32 GetRivalStarterSlot(u32 playerSlot);
u16 GetRivalStarterPokemon(void);
void SwapRivalStarterLineForRegion(struct Pokemon *party, u32 monsCount);
void SetMonToRivalStarter(struct Pokemon *mon);
void CB2_ChooseStarter(void);
void Debug_ChooseStarterAgain(struct ScriptContext *ctx);

#endif // GUARD_STARTER_CHOOSE_H
