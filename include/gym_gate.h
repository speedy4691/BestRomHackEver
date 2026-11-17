#ifndef GUARD_GYM_GATE_H
#define GUARD_GYM_GATE_H

// G2 Pokédex gate at gym doors. Thresholds live in sGymGates (src/gym_gate.c).
bool32 GymGate_TryBlockWarp(u8 destMapGroup, u8 destMapNum);
void BufferNextGymGateSpecies(void);

#endif // GUARD_GYM_GATE_H
