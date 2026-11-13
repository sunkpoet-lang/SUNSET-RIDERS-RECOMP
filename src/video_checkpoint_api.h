#pragma once
#include "sunset_video_api.h"
struct VideoCheckpoint;
VideoCheckpoint* video_checkpoint(SunsetVideo*,const SunsetBus&);
void video_checkpoint_delete(VideoCheckpoint*);
bool video_checkpoints_equal(const VideoCheckpoint*,const VideoCheckpoint*);
SunsetVideo* video_prepare_restore(const SunsetVideo*,const VideoCheckpoint*,const SunsetBus&);
void video_checkpoint_test();
