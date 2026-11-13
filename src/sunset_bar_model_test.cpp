#include <sunset_bar_model.h>
#include <iostream>
#include <stdexcept>
static unsigned checks=0;static void check(bool b){++checks;if(!b)throw std::runtime_error("bar model check "+std::to_string(checks));}
static const recomp::BarEntry& find(const std::vector<recomp::BarEntry>&entries,const std::string&name){for(auto&e:entries)if(e.label==name)return e;throw std::runtime_error("missing bar entry: "+name);}
int main(){using namespace sunset_menu;Adapter a;a.assists.bind(a.assists.expected_digest());a.open();a.menu.Close();BarModel model(a);auto menus=model.entries(false,true);
 check(menus.size()==5);for(unsigned i=0;i<5;++i)check(menus[i].title==std::array<std::string,5>{"Game","Video","Input","Features","Help"}[i]);
 auto&save=find(menus[0].entries,"Save temporary state");auto&load=find(menus[0].entries,"Load temporary state");check(save.children.size()==1&&save.enabled);check(load.children.size()==1&&!load.enabled&&!load.children[0].enabled);
 check(model.choose(save.children[0].id).kind==recomp::Menu::Chosen::Save);check(a.menu.Slot()==1);
 auto&cheats=find(menus[3].entries,"Cheats");auto&player=find(cheats.children,"Player 1 cheats");auto&lives=find(player.children,"Infinite lives");int life_on=find(lives.children,"On").id;model.choose(life_on);check(a.assists.infinite(0));check(a.config.value(4)==1);
 model.choose(find(player.children,"Give both powerups").id);check(a.assists.snapshot().pending[0]==3);
 auto&turbo=find(menus[3].entries,"Turbo fire");model.choose(find(find(turbo.children,"Player 4").children,"15 / sec").id);check(a.turbo.rate(3)==15);
 a.config.setValue(4,0);check(!a.assists.infinite(0));menus=model.entries(true,true);check(find(menus[0].entries,"Load temporary state").enabled);
 a.policy.enter(Mode::Online);menus=model.entries(true,true);check(!find(menus[0].entries,"Save temporary state").enabled);check(!find(menus[3].entries,"Cheats").enabled);
 model.choose(life_on);check(!a.assists.infinite(0));
 recomp::MenuBar bar;bar.Configure(model.entries(true,true));bar.SetArea(576,536,2);auto box=bar.TitleBox(0);bar.Click(box.x+box.w/2,box.y+box.h/2);check(bar.IsOpen());bar.Buttons(recomp::kMenuBack);check(!bar.IsOpen());
 std::vector<uint32_t> pixels(576*536,0);bar.Draw(pixels.data(),576,536);check(pixels[0]!=0);
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"headings\":5}\n";
}
