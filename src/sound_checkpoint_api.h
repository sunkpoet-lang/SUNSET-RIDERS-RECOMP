#pragma once
#include "dual_sound_api.h"
struct SoundCheckpoint;
SoundCheckpoint* sound_checkpoint(DualSound*);
void sound_checkpoint_delete(SoundCheckpoint*);
bool sound_checkpoints_equal(const SoundCheckpoint*,const SoundCheckpoint*);
DualSound* sound_prepare_restore(const DualSound*,const SoundCheckpoint*);
