#ifndef GUARD_CONFIG_PHOTOGRAPHY_H
#define GUARD_CONFIG_PHOTOGRAPHY_H

// BestRomHackEver Photography prototype: every tuning number for the viewfinder (src/photography.c).

// --- Shots ---
// Photos per wild battle. The debug viewfinder has no limit. Closing the camera after taking
// at least one photo uses the player's turn.
#define PHOTO_SHOTS_PER_BATTLE      3

// --- Drift speed (the "old TV screensaver" movement) ---
// Speed number per species = base Speed + rarity bonus. Rarer = lower catch rate = bigger bonus.
// Examples: Zigzagoon (Spe 60, catch 255) = 60; Ralts (40, 235) = 45; Mewtwo (130, 3) = 193.
#define PHOTO_RARITY_DIVISOR        4     // Rarity bonus = (255 - catch rate) / this.
#define PHOTO_SPEED_NUMBER(baseSpeed, catchRate) ((baseSpeed) + (255 - (catchRate)) / PHOTO_RARITY_DIVISOR)
// Pixels per frame = speed number * this / 256 (diagonal movement). 4 -> speed 60 moves ~1 px per frame.
#define PHOTO_SPEED_TO_SUBPIXELS    4
#define PHOTO_MIN_SPEED_NUMBER      20    // Floor, so very slow POKéMON still move.

// --- Viewfinder frame ---
#define PHOTO_FRAME_WIDTH           112   // Pixels.
#define PHOTO_FRAME_HEIGHT          88
#define PHOTO_FRAME_MOVE_SPEED      2     // Pixels per frame while the D-pad is held.
#define PHOTO_PLAY_AREA_BOTTOM      120   // The POKéMON and frame stay above the text box.
#define PHOTO_OUTSIDE_DARKEN        7     // How dark it is outside the frame (0-16).

// --- Zoom (hardware sprite scaling) ---
// Size of the POKéMON per zoom step, 256 = normal size. L zooms in, R zooms out.
#define PHOTO_ZOOM_STEPS            { 256, 320, 384, 448 } // 1x, 1.25x, 1.5x, 1.75x
#define PHOTO_ZOOM_STEP_COUNT       4

// --- Scoring (total out of 100) ---
#define PHOTO_CENTRE_MAX            40    // Dead centre = 40, edge of frame = 0 (straight line between).
#define PHOTO_SIZE_POINTS           { 10, 20, 30, 40 } // Per zoom step; 0 if any part is cut off by the frame.
#define PHOTO_POSE_PEAK             20    // At the peak of the front animation, or on the second front frame.
#define PHOTO_POSE_FACING_CENTRE    10    // Otherwise, if the POKéMON faces the frame's centre.
// "Peak" = the animation's movement is at least this % of its biggest movement in the last loop.
#define PHOTO_POSE_PEAK_PERCENT     75
#define PHOTO_POSE_PEAK_MIN         3     // Ignore tiny movements (pixels).

// --- Grades ---
// Any shot with the POKéMON at least partly in the frame = Photographed.
#define PHOTO_GOOD_SCORE            60
#define PHOTO_AMAZING_SCORE         85

// --- Front animation ---
#define PHOTO_ANIM_REPEAT_FRAMES    90    // Pause between loops of the front animation (60 = 1 second).
#define PHOTO_SHOT_COOLDOWN         20    // Frames before the next photo can be taken.
#define PHOTO_AUTO_CLOSE_FRAMES     120   // After the last shot, the camera closes by itself after this long.

#endif // GUARD_CONFIG_PHOTOGRAPHY_H
