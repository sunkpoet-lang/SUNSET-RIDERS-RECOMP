#include "sunset_turbo.h"
#include <iostream>
#include <vector>
static unsigned checks=0;static void check(bool ok){++checks;if(!ok)throw std::runtime_error("turbo regression");}
int main(){using namespace sunset_menu;
 for(unsigned rate:{5,10,15}){
   Turbo t;Policy p;check(t.set(0,rate,p));unsigned rises=0;bool prior=false;
   const unsigned frames=480000000ull/Turbo::frame_ticks;
   for(unsigned f=0;f<frames;++f){auto input=t.apply(0,63,p);bool fire=(input&16)!=0;check((input&~16)==(63&~16));if(fire&&!prior)++rises;prior=fire;}
   check(rises>=rate*10-1&&rises<=rate*10+1);check(p.tainted());
   auto saved=t.snapshot();std::vector<uint8_t> expected;for(int i=0;i<197;++i)expected.push_back(t.apply(0,63,p));t.restore(saved);
   for(auto e:expected)check(t.apply(0,63,p)==e);
   check(t.apply(0,0,p)==0);check(t.snapshot().phase[0]==0);check(t.apply(0,16,p)==16);
 }
 Turbo t;Policy p;for(unsigned i=0;i<4;++i)check(t.set(i,i?i*5:0,p));
 check(!t.set(4,5,p));check(!t.set(0,7,p));
 for(unsigned f=0;f<257;++f){check(t.apply(0,63,p)==63);for(unsigned i=1;i<4;++i)t.apply(i,63,p);}
 auto saved=t.snapshot();auto bad=saved;bad.phase[2]=Turbo::clock;bool rejected=false;try{t.restore(bad);}catch(...){rejected=true;}check(rejected);check(t.snapshot().phase==saved.phase);
 for(auto m:{Mode::Preparing,Mode::Online,Mode::Reconnecting,Mode::Spectating,Mode::Replay,Mode::Unknown}){
   Policy clean;t.restore(saved);clean.enter(m);check(!t.set(0,15,clean));check(t.apply(2,63,clean)==63);check(t.snapshot().rate==std::array<unsigned,4>{});check(!clean.tainted());
 }
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"rates\":[0,5,10,15]}\n";
}
