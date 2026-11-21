#ifndef GUARD_PHOTOGRAPHY_H
#define GUARD_PHOTOGRAPHY_H

#include "main.h"

// Best photo grade per species (forms are separate species IDs). Stored in SaveBlock3.
enum PhotoGrade
{
    PHOTO_GRADE_NONE,
    PHOTO_GRADE_PHOTOGRAPHED,
    PHOTO_GRADE_GOOD,
    PHOTO_GRADE_AMAZING,
};

enum PhotoGrade GetPhotoGrade(enum Species species);
void SetPhotoGrade(enum Species species, enum PhotoGrade grade);
bool32 TryUpgradePhotoGrade(enum Species species, enum PhotoGrade grade);
void ClearAllPhotoGrades(void);
const u8 *GetPhotoGradeName(enum PhotoGrade grade);
void GetPhotoGradeDexText(u8 *dest, enum Species species);

// Rewards (applied when a wild POKéMON is created, before anything is shown; see include/config/photography.h).
u32 PhotoReward_PickNature(enum Species species, u32 nature, u32 (*pickNature)(enum Species));
void PhotoReward_BoostIvs(struct Pokemon *mon);
// L on the wild battle action menu opens the camera (src/battle_controller_player.c).
bool32 PhotoCamera_CanUseShortcut(void);

// Battle effects (see include/config/photography.h).
u32 PhotoReward_CatchRatePercent(enum Species species);
bool32 PhotoCrit_IsPending(enum BattlerId battlerAtk);
void PhotoCrit_Consume(void);
bool32 ShouldShowPhotoGradeIcon(void); // Wild battles only (health bar camera icon).

// Info panel: SELECT on the wild battle action menu (src/battle_controller_player.c).
bool32 PhotoInfo_TryOpen(void);
bool32 PhotoInfo_HandleInput(void); // TRUE once the panel has closed.

// Viewfinder (prototype). Debug: any species, no shot limit. Battle: opened via the CAMERA in the Bag.
void OpenPhotoViewfinder_Debug(enum Species species, MainCallback returnCallback);
void CB2_OpenPhotoViewfinderFromBattle(void);

#endif // GUARD_PHOTOGRAPHY_H
