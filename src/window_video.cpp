// Diagnostic MAME-chip renderer, not independent full-MAME verification.
// Host design adapted from Sebastian Toyos kvideo.cpp, BSD-3-Clause.
// Sunset callbacks/palette/priority adapted from Nicola Salmoria tmnt2.cpp, BSD-3-Clause.
#include "video_checkpoint_api.h"
#include "board_state.h"
#include <optional>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <list>
#include <memory>
#include <new>
#include <sstream>
#include <set>
#include <type_traits>
#include <utility>
namespace sv {
#include "emu.h"
#include "util/palette.cpp"
#include "util/bitmap.cpp"
#include "emu/drawgfx.cpp"
#include "emu/tilemap.cpp"
#include "konami/k052109.cpp"
#include "k053244_k053245.cpp"
}
struct SunsetVideo {
    std::unique_ptr<sv::host_context> context=std::make_unique<sv::host_context>();
    std::optional<SunsetBoardSnapshot> entry_scroll,last_entry,last_exit;
    std::array<uint64_t,2> board_roms{};
    bool roms_bound=false,last_entering=false;
    std::vector<uint8_t> tile_rom,sprite_rom;
    sv::device_t owner{0,"sunset","sunset",nullptr,24000000};
    sv::device_palette_interface palette{owner,2048};
    std::unique_ptr<sv::k052109_device> tiles;
    std::unique_ptr<sv::k053244_device> sprites;
    sv::bitmap_ind16 bitmap{384,264};
    int layer_colors[3]{},priorities[3]{},sprite_color=0;
    std::vector<uint8_t> rgb=std::vector<uint8_t>(288*224*3);
    uint64_t frames=0,hash=0,nonblack=0,distinct=0;
    explicit SunsetVideo(std::vector<uint8_t> t,std::vector<uint8_t> c):tile_rom(std::move(t)),sprite_rom(std::move(c)){
        sv::context_scope selected(*context);
        if(tile_rom.size()!=0x100000||sprite_rom.size()!=0x200000)throw std::runtime_error("invalid video ROM sizes");
        sv::host_region_base=tile_rom.data();sv::host_region_length=tile_rom.size();
        tiles=std::make_unique<sv::k052109_device>(0,"k052109",&owner,24000000);
        sv::host_region_base=sprite_rom.data();sv::host_region_length=sprite_rom.size();
        sprites=std::make_unique<sv::k053244_device>(0,"k053245",&owner,24000000);
        sv::host_region_base=nullptr;sv::host_region_length=0;
        auto& screen=sv::the_screen();screen.set_visible_area(16,303,16,239);screen.priority().allocate(384,264);
        tiles->set_palette_interface(palette);tiles->set_gfx_region(tile_rom.data(),tile_rom.size());
        tiles->set_tile_callback([this](int layer,int bank,int& code,int& color,int&,int&){
            code|=((color&3)<<8)|((color&0x10)<<6)|((color&0x0c)<<9)|(bank<<13);
            color=layer_colors[layer]+((color&0xe0)>>5);
        });
        sprites->set_palette_interface(palette);sprites->set_gfx_region(sprite_rom.data(),sprite_rom.size());
        sprites->set_sprite_callback([this](int&,int& color,int& priority){
            int pri=0x20|((color&0x60)>>2);
            priority=pri<=priorities[2]?0:(pri<=priorities[1]?0xf0:(pri<=priorities[0]?0xfc:0xfe));
            color=sprite_color+(color&0x1f);
        });
        tiles->start();sprites->start();tiles->reset();sprites->reset();sprites->set_z_rejection(0);
    }
    ~SunsetVideo(){sv::context_scope selected(*context);tiles.reset();sprites.reset();}
    void load(const SunsetBus& b){
        tiles->set_rmrd_line(sv::CLEAR_LINE);
        for(unsigned i=0;i<0x4000;++i)if(tiles->read(i)!=b.tiles.ram[i])tiles->write(i,b.tiles.ram[i]);
        for(unsigned i=0;i<0x400;++i)sprites->k053245_word_w(i,b.sprites.buffer[i]);
        sprites->k053244_w(6,0); // Load the actual latched buffer, not live sprite RAM.
        for(unsigned i=0;i<0x400;++i)sprites->k053245_word_w(i,b.sprites.ram[i]);
        for(unsigned i=0;i<16;++i)if(i!=6&&i!=7)sprites->k053244_w(i,b.sprites.regs[i]);
        sprites->bankselect(b.sprite_rom_bank);
    }
    void draw(const SunsetBus& b){
        const unsigned ci[3]={2,4,3};
        for(unsigned i=0;i<3;++i){layer_colors[i]=b.priority.palette(ci[i]);tiles->mark_tilemap_dirty(i);priorities[i]=b.priority.priority(ci[i]);}
        sprite_color=b.priority.palette(1);
        int layer[3]={0,1,2};sv::konami_sortlayers3(layer,priorities);
        for(unsigned i=0;i<2048;++i){unsigned raw=(b.palette[2*i]<<8)|b.palette[2*i+1];
            palette.set_pen_color(i,sv::rgb_t(sv::pal5bit(raw),sv::pal5bit(raw>>5),sv::pal5bit(raw>>10)));}
        palette.set_shadow_mode((~b.dim_control>>4)&1);
        auto& screen=sv::the_screen();auto clip=screen.visible_area();
        screen.priority().fill(0,clip);bitmap.fill(16*b.priority.palette(0),clip);
        for(unsigned i=0;i<3;++i)tiles->tilemap_draw(screen,bitmap,clip,layer[i],0,1<<i);
        sprites->sprites_draw(bitmap,clip,screen.priority());
        unsigned enable=b.priority.priority(5),dimpal=0;
        if(enable!=0x3e){dimpal|=1<<((layer_colors[1]>>4)&7);
            if(!(enable&0x10)){for(unsigned i=0;i<2;++i)dimpal|=1<<(((sprite_color>>4)|i)&7);dimpal|=1<<((layer_colors[2]>>4)&7);}}
        unsigned factor=b.dim_video|((~b.dim_control&0x10)>>1);
        double brightness=1.0-(1.0-0.6)*factor/8;
        for(unsigned i=0;i<2048;++i)palette.set_pen_contrast(i,(dimpal&(1<<(i/256)))?brightness:1.0);
        const auto* pens=palette.pens();hash=14695981039346656037ull;nonblack=0;std::set<uint32_t> colours;
        for(unsigned y=0;y<224;++y)for(unsigned x=0;x<288;++x){
            auto index=bitmap.pix(y+16,x+16);if(index>=6144)throw std::runtime_error("pixel palette index out of range");
            uint32_t p=pens[index]&0xffffff;colours.insert(p);if(p)++nonblack;
            for(unsigned c=0;c<3;++c){uint8_t v=(p>>(16-8*c))&255;rgb[(y*288+x)*3+c]=v;hash=(hash^v)*1099511628211ull;}}
        distinct=colours.size();++frames;
    }
};
SunsetVideo* video_create(std::vector<uint8_t> t,std::vector<uint8_t> c){return new SunsetVideo(std::move(t),std::move(c));}
bool video_bus_size_matches(size_t n){return n==sizeof(SunsetBus);}
void video_destroy(SunsetVideo* v){delete v;}
void video_edge(SunsetVideo* v,const SunsetBus& b,bool entering){
    sv::context_scope selected(*v->context);
    if(!v->roms_bound){v->board_roms=board_rom_identity(b);v->roms_bound=true;}
    SunsetBoardSnapshot latch{board_registry(const_cast<SunsetBus&>(b))->capture(),b.unmapped_writes,v->board_roms,0,1};
    latch.checksum=board_state_checksum(latch);
    if(entering){v->entry_scroll=v->last_exit;v->last_entry=std::move(latch);}else v->last_exit=std::move(latch);
    v->last_entering=entering;
    v->load(b);if(entering)v->draw(b);sv::the_screen().vblank(entering);
}
void video_write_ppm(SunsetVideo* v,const std::string& path){if(!v->frames)throw std::runtime_error("no rendered frame");std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("cannot write private frame");f<<"P6\n288 224\n255\n";f.write(reinterpret_cast<const char*>(v->rgb.data()),v->rgb.size());if(!f)throw std::runtime_error("frame write failed");}
std::string video_report(SunsetVideo* v){std::ostringstream o;o<<"\"rendered_frames\":"<<v->frames<<",\"frame_hash\":"<<v->hash<<",\"nonblack_pixels\":"<<v->nonblack<<",\"distinct_colours\":"<<v->distinct;return o.str();}
void video_test(){
    SunsetVideo v(std::vector<uint8_t>(0x100000),std::vector<uint8_t>(0x200000));SunsetBus b(std::vector<uint8_t>(0xc0000));
    // Pen zero red, transparent/zero graphics: background must be exactly red.
    b.palette[1]=31;b.priority.write(5,0x3e);video_edge(&v,b,false);video_edge(&v,b,true);
    if(v.frames!=1||v.distinct!=1||v.rgb[0]!=255||v.rgb[1]!=0||v.rgb[2]!=0)throw std::runtime_error("synthetic video palette/crop mismatch");
    auto h=v.hash;video_edge(&v,b,false);video_edge(&v,b,true);if(v.hash!=h)throw std::runtime_error("repeat video frame differs");
}

struct VideoCheckpoint {
    std::optional<SunsetBoardSnapshot> entry_scroll,last_entry,last_exit;
    std::array<uint64_t,2> board_roms,video_roms;
    std::vector<uint8_t> rgb;
    uint64_t frames,hash,nonblack,distinct;
    bool last_entering;
    unsigned schema=1;
    bool operator==(const VideoCheckpoint&)const=default;
};
static uint64_t video_rom_hash(const std::vector<uint8_t>& bytes){uint64_t h=14695981039346656037ull;for(auto b:bytes)h=(h^b)*1099511628211ull;return h;}
VideoCheckpoint* video_checkpoint(SunsetVideo* v,const SunsetBus& b){
    return new VideoCheckpoint{v->entry_scroll,v->last_entry,v->last_exit,board_rom_identity(b),{video_rom_hash(v->tile_rom),video_rom_hash(v->sprite_rom)},v->rgb,v->frames,v->hash,v->nonblack,v->distinct,v->last_entering,1};
}
void video_checkpoint_delete(VideoCheckpoint* s){delete s;}
bool video_checkpoints_equal(const VideoCheckpoint* a,const VideoCheckpoint* b){return *a==*b;}
SunsetVideo* video_prepare_restore(const SunsetVideo* live,const VideoCheckpoint* s,const SunsetBus& b){
    if(s->schema!=1||s->board_roms!=board_rom_identity(b)||s->video_roms!=std::array<uint64_t,2>{video_rom_hash(live->tile_rom),video_rom_hash(live->sprite_rom)}||s->rgb.size()!=288*224*3)throw std::runtime_error("video checkpoint identity/shape mismatch");
    auto candidate=std::make_unique<SunsetVideo>(live->tile_rom,live->sprite_rom);
    SunsetBus scratch(b.rom);scratch.sprites.rom=b.sprites.rom;
    auto replay=[&](const SunsetBoardSnapshot& latch,bool enter){board_restore(scratch,latch);video_edge(candidate.get(),scratch,enter);};
    if(s->last_entry){if(s->entry_scroll)replay(*s->entry_scroll,false);replay(*s->last_entry,true);}
    if(!s->last_entering&&s->last_exit)replay(*s->last_exit,false);
    if(candidate->rgb!=s->rgb||candidate->hash!=s->hash||candidate->nonblack!=s->nonblack||candidate->distinct!=s->distinct)throw std::runtime_error("video latches did not reconstruct saved RGB");
    candidate->frames=s->frames;candidate->entry_scroll=s->entry_scroll;candidate->last_entry=s->last_entry;candidate->last_exit=s->last_exit;candidate->last_entering=s->last_entering;candidate->board_roms=s->board_roms;candidate->roms_bound=true;
    return candidate.release();
}
void video_checkpoint_test(){
    video_test();
    for(unsigned phase=0;phase<3;++phase){
        SunsetBus b(std::vector<uint8_t>(0xc0000));b.sprites.rom.resize(0x200000);
        auto v=std::unique_ptr<SunsetVideo,decltype(&video_destroy)>(video_create(std::vector<uint8_t>(0x100000),b.sprites.rom),video_destroy);
        b.palette[1]=31;b.priority.write(5,0x3e);
        if(phase){video_edge(v.get(),b,false);video_edge(v.get(),b,true);}
        if(phase==2){b.tiles.write(0x1a00,7);video_edge(v.get(),b,false);}
        auto saved=std::unique_ptr<VideoCheckpoint,decltype(&video_checkpoint_delete)>(video_checkpoint(v.get(),b),video_checkpoint_delete);
        auto candidate=std::unique_ptr<SunsetVideo,decltype(&video_destroy)>(video_prepare_restore(v.get(),saved.get(),b),video_destroy);
        auto untouched=std::unique_ptr<VideoCheckpoint,decltype(&video_checkpoint_delete)>(video_checkpoint(v.get(),b),video_checkpoint_delete);
        if(!video_checkpoints_equal(saved.get(),untouched.get()))throw std::runtime_error("candidate video touched live context");
        video_edge(v.get(),b,true);video_edge(candidate.get(),b,true);
        if(v->rgb!=candidate->rgb)throw std::runtime_error("video context continuation diverged");
    }
}

#include "persistent_checkpoint_api.h"
#include "state_wire.h"

std::vector<uint8_t> video_checkpoint_pack(const VideoCheckpoint* s){
    state_wire::Writer w;state_wire::put(w,s->schema);state_wire::put(w,s->board_roms);state_wire::put(w,s->video_roms);
    state_wire::put(w,s->entry_scroll);state_wire::put(w,s->last_entry);state_wire::put(w,s->last_exit);state_wire::put(w,s->rgb);
    state_wire::put(w,s->frames);state_wire::put(w,s->hash);state_wire::put(w,s->nonblack);state_wire::put(w,s->distinct);state_wire::put(w,s->last_entering);return std::move(w.bytes);
}
VideoCheckpoint* video_checkpoint_unpack(std::span<const uint8_t> bytes){
    auto s=std::make_unique<VideoCheckpoint>();state_wire::Reader r(bytes);state_wire::get(r,s->schema);state_wire::get(r,s->board_roms);state_wire::get(r,s->video_roms);
    state_wire::get(r,s->entry_scroll);state_wire::get(r,s->last_entry);state_wire::get(r,s->last_exit);state_wire::get(r,s->rgb);
    state_wire::get(r,s->frames);state_wire::get(r,s->hash);state_wire::get(r,s->nonblack);state_wire::get(r,s->distinct);state_wire::get(r,s->last_entering);r.finish();
    if(s->schema!=1||s->rgb.size()!=288*224*3)throw std::runtime_error("unsupported video state schema/shape");return s.release();
}

const uint8_t* window_video_rgb(SunsetVideo* v){return v->rgb.data();}
