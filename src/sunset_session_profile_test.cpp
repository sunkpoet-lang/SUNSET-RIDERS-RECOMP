#include <sunset_session_profile.h>
#include <streaming_audio.h>
#include <iostream>
#include <stdexcept>
static int checks=0;static void check(bool v){++checks;if(!v)throw std::runtime_error("session boundary regression");}
int main(){for(uint64_t seconds:{301ull,361ull,3601ull,43200ull}){
 auto n=seconds*48000,i=SunsetStreamingAudio::index(n);SunsetStreamingAudio mixer;
 SunsetStreamingAudio::Snapshot s{n,i,3,{},i,3,{}};mixer.restore(s);check(mixer.ready());check(mixer.pop()==SunsetStreamingAudio::Stereo{0,0});check(mixer.next==n+1);
 auto snapshot=mixer.snapshot();SunsetStreamingAudio restored;restored.restore(snapshot);check(restored.snapshot()==snapshot);
 auto clocks=seconds*sunset_session::chip_hz;check(clocks*sunset_session::master_hz/sunset_session::chip_hz==seconds*sunset_session::master_hz);
 }
 SunsetStreamingAudio mixer;auto s=mixer.snapshot();std::get<0>(s)=SunsetStreamingAudio::limit+1;bool rejected=false;try{mixer.restore(s);}catch(...){rejected=true;}check(rejected);check(mixer.next==0);
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"arithmetic_profile_hours\":12,\"full_profile_runtime_tested\":false}\n";
}
