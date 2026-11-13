#pragma once
#include "sound_checkpoint_api.h"
#include "video_checkpoint_api.h"
#include <span>
std::vector<uint8_t> sound_checkpoint_pack(const SoundCheckpoint*);
SoundCheckpoint* sound_checkpoint_unpack(std::span<const uint8_t>);
std::vector<uint8_t> video_checkpoint_pack(const VideoCheckpoint*);
VideoCheckpoint* video_checkpoint_unpack(std::span<const uint8_t>);
