#pragma once
#include <cstdint>
#include <limits>
namespace sunset_session {
inline constexpr uint64_t seconds=12ull*60*60,admission_seconds=seconds+1;
inline constexpr uint64_t master_hz=48000000,chip_hz=3579545,frame_ticks=811008;
inline constexpr uint64_t max_master=seconds*master_hz,max_frames=max_master/frame_ticks;
inline constexpr uint64_t max_audio=admission_seconds*48000,qa_frames=90ull*master_hz/frame_ticks;
static_assert(admission_seconds*chip_hz<(uint64_t(std::numeric_limits<int64_t>::max())-chip_hz)/master_hz);
static_assert(max_audio<std::numeric_limits<int32_t>::max());
static_assert(max_audio<std::numeric_limits<uint64_t>::max()/chip_hz);
static_assert(max_master+frame_ticks<admission_seconds*master_hz);
}
