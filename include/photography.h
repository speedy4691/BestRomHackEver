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
const u8 *GetPhotoGradeDexText(enum Species species);

// Viewfinder (prototype). Debug: any species, no shot limit. Battle: opened via the CAMERA in the Bag.
void OpenPhotoViewfinder_Debug(enum Species species, MainCallback returnCallback);
void CB2_OpenPhotoViewfinderFromBattle(void);

#endif // GUARD_PHOTOGRAPHY_H
