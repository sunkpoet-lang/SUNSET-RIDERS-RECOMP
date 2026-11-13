#include "sunset_assists.h"
#include <vector>
#include <iostream>
static unsigned checks=0;static void check(bool ok){++checks;if(!ok)throw std::runtime_error("assist policy regression");}
struct Board {
 std::vector<uint8_t>ram=std::vector<uint8_t>(0x4000),objects=std::vector<uint8_t>(0x4000);unsigned writes=0;
 uint8_t& byte(unsigned a){if(a>=0x104000&&a<0x108000)return ram.at(a-0x104000);return objects.at(a-0x180000);}
 uint16_t read(unsigned a,unsigned,bool){return uint16_t(byte(a))<<8|byte(a+1);}
 void write(unsigned a,uint16_t d,uint16_t mask){if(mask&0xff00)byte(a)=d>>8;if(mask&0xff)byte(a+1)=d;++writes;}
 Board(){ram[5]=4;ram[0x170]=15;for(unsigned p=0;p<4;++p){ram[0x200+p*0x40]=3;byte(0x181c80+p*0x300+0x20)=p;byte(0x181c80+p*0x300+0x31)=3;}}
};
int main(){using namespace sunset_menu;
 for(unsigned p=0;p<4;++p){Board b;Policy policy;Assists a;check(!a.set_lives(p,true,policy));check(a.bind(a.expected_digest()));check(a.set_lives(p,true,policy));b.ram[0x200+p*0x40]=1;a.tick(b,policy);check(b.ram[0x200+p*0x40]==3);check(policy.tainted());for(unsigned q=0;q<4;++q)if(q!=p)check(b.ram[0x200+q*0x40]==3);
  unsigned base=0x181c80+p*0x300;check(a.request(p,3,policy));a.tick(b,policy);check(b.byte(base+0x6f)==14);check(b.byte(base+0x60)&1);
  b.byte(base+0x27)=1;b.byte(base+0x6f)=0;b.byte(base+0x60)=0;a.tick(b,policy);check(b.byte(base+0x6f)==15);check(a.pickups==2);
  b.byte(base+0x27)=3;b.byte(base+0x6f)=0;b.byte(base+0x60)=0;auto w=b.writes;check(a.request(p,3,policy));a.tick(b,policy);check(b.writes==w);check(a.last[p]==Assists::AlreadyOwned);
  check(a.set_lives(p,false,policy));b.ram[0x200+p*0x40]=2;a.tick(b,policy);check(b.ram[0x200+p*0x40]==2);check(policy.tainted());
 }
 for(auto mode:{Mode::Preparing,Mode::Online,Mode::Reconnecting,Mode::Spectating,Mode::Replay,Mode::Unknown}){Board b;Policy p;Assists a;a.bind(a.expected_digest());a.set_lives(0,true,p);a.request(0,3,p);p.enter(mode);b.ram[0x200]=1;auto ram=b.ram,objects=b.objects;a.tick(b,p);check(b.ram==ram&&b.objects==objects&&b.writes==0);check(!a.infinite(0));check(!a.request(1,3,p));}
 for(unsigned mode:{0,85,170}){Board b;Policy p;Assists a;a.bind(a.expected_digest());a.set_lives(0,true,p);a.request(0,3,p);b.ram[5]=mode;b.ram[0x200]=1;a.tick(b,p);check(b.writes==0);}
 Board b;Policy p;Assists a;a.bind(a.expected_digest());check(!a.request(4,1,p));check(!a.request(0,4,p));auto s=a.snapshot();s.pending[0]=4;bool bad=false;try{a.restore(s);}catch(...){bad=true;}check(bad);check(a.snapshot().pending[0]==0);
 a.request(0,1,p);b.byte(0x181cef)=9;for(unsigned f=0;f<60;++f)a.tick(b,p);check(a.last[0]==Assists::Busy);check(b.writes==0);
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";
}
