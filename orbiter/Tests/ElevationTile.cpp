#include "Terrain/ElevationTile.h"
#include "Terrain/TileGeometry.h"
#include "Terrain/TileSelection.h"
#include "Orbiter.h"
#include "Celbody.h"
#include "elevmgr.h"
#include <zlib.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <limits>
#include <memory>
using Bytes=std::vector<std::uint8_t>;
using Tile=orbiter::terrain::ElevationTile;
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
static void u32(Bytes &bytes,std::size_t at,std::uint32_t value) {for(unsigned i=0;i<4;++i)bytes.at(at+i)=value>>(8*i);}
static void f64(Bytes &bytes,std::size_t at,double value) {std::uint64_t bits;std::memcpy(&bits,&value,8);u32(bytes,at,bits);u32(bytes,at+4,bits>>32);}
static Bytes tile(int format,int raw=0,double scale=1,double offset=0,bool ramp=false) {
    const unsigned width=std::abs(format)/8;Bytes bytes(100+259*259*width,0);
    std::memcpy(bytes.data(),"ELE\1",4);u32(bytes,4,100);u32(bytes,8,format);
    u32(bytes,12,259);u32(bytes,16,259);u32(bytes,20,1);u32(bytes,24,1);
    f64(bytes,28,scale);f64(bytes,36,offset);
    for(unsigned y=0;y<259;++y)for(unsigned x=0;x<259;++x) {
        const auto value=static_cast<std::uint32_t>(ramp?int(x+10*y):raw);
        for(unsigned b=0;b<width;++b)bytes[100+(y*259+x)*width+b]=value>>(8*b);
    }
    return bytes;
}
static void write(const std::filesystem::path &path,const Bytes &bytes) {
    std::filesystem::create_directories(path.parent_path());std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());require(bool(file),"write elevation fixture");
}
struct Scratch {
    std::filesystem::path old=std::filesystem::current_path();
    std::filesystem::path path=std::filesystem::temp_directory_path()/("orbiter-elevation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch(const char *fixture) {std::filesystem::copy(fixture,path,std::filesystem::copy_options::recursive);std::filesystem::current_path(path);}
    ~Scratch() {std::filesystem::current_path(old);std::filesystem::remove_all(path);}
};
struct Manager:ElevationManager {using ElevationManager::ElevationManager;using ElevationManager::LoadElevationTile;using ElevationManager::HasElevationTile;using ElevationManager::LoadElevationTile_mod;};
int main(int argc,char **argv) {
    try {
        require(argc==2,"native world fixture required");
        for(const auto item:std::vector<std::pair<int,int>>{{0,0},{8,255},{-8,-128},{16,65535},{-16,-32768}}) {
            const auto bytes=tile(item.first,item.second,2,3);auto decoded=Tile::Decode(bytes.data(),bytes.size());
            require(decoded && decoded->Sample(0.5,0.5)==item.second*2+3,"integer format and scale/offset decoded");
        }
        auto bytes=tile(-16,0,0.5,-50,true);auto decoded=Tile::Decode(bytes.data(),bytes.size());require(bool(decoded),"decode analytic ramp");
        for(double u:{0.0,0.123,0.5,1.0})for(double v:{0.0,0.377,0.5,1.0})
            require(std::abs(decoded->Sample(u,v)-(0.5*((1+256*u)+10*(1+256*v))-50))<1e-9,"bilinear sampling preserves south/north orientation");
        require(decoded->Sample(-1,-1)==-50 && decoded->Sample(2,2)==1369,"padded edges clamp without extrapolation");
        bool threw=false;try{decoded->Sample(std::numeric_limits<double>::quiet_NaN(),0);}catch(const std::invalid_argument &){threw=true;}
        require(threw,"nonfinite sample coordinates rejected");
        auto reject=[](const Bytes &bad){require(!Tile::Decode(bad.data(),bad.size()),"malformed elevation rejected");};
        reject(Bytes(bytes.begin(),bytes.end()-1));
        for(auto field:std::vector<std::pair<unsigned,unsigned>>{{4,99},{4,0xffffffff},{8,32},{12,258},{20,2}}) {auto bad=bytes;u32(bad,field.first,field.second);reject(bad);}
        auto bad=bytes;f64(bad,28,std::numeric_limits<double>::infinity());reject(bad);
        reject(tile(-16,32767,std::numeric_limits<double>::max()));
        auto extended=bytes;extended.insert(extended.begin()+100,16,0);u32(extended,4,116);
        auto extension=Tile::Decode(extended.data(),extended.size());
        require(extension && extension->Sample(0.5,0.5)==decoded->Sample(0.5,0.5),"extended header respected");
        std::vector<std::int16_t> base(259*259,42);
        auto patch=tile(8,255,2,3);patch[100]=7;
        require(Tile::ApplyModification(patch.data(),patch.size(),0.5,base.data()) && base[0]==34 && base[1]==42,"byte overlay mask preserves base terrain");
        patch=tile(-16,32767,2,-3);patch[100]=0xfe;patch[101]=0xff;
        require(Tile::ApplyModification(patch.data(),patch.size(),0.5,base.data()) && base[0]==-14 && base[1]==42,"signed overlay mask and negative height");
        const auto previous=base;
        patch=tile(-16,1,2);patch[patch.size()-2]=0xfe;patch.back()=0x7f;
        require(!Tile::ApplyModification(patch.data(),patch.size(),1,base.data()) && base==previous,"late overflow leaves entire base unchanged");
        require(!Tile::ApplyModification(patch.data(),patch.size()-1,1,base.data()) && base==previous,"truncated overlay leaves base unchanged");
        patch=tile(0,0,1,12.5);
        require(Tile::ApplyModification(patch.data(),patch.size(),0.5,base.data()) && base.front()==25 && base.back()==25,"flat overlay replaces entire tile");
        auto mountainBytes=tile(0,0,1,123);
        auto mountain=Tile::Decode(mountainBytes.data(),mountainBytes.size());
        const auto geometry=orbiter::terrain::BuildTileGeometry(14,512,1024,1737400,&*mountain);
        require(geometry.vertices.size()==1089 && geometry.indices.size()==6144,"terrain grid topology");
        for(const auto &vertex:geometry.vertices) {
            double p[3];for(unsigned k=0;k<3;++k)p[k]=geometry.origin[k]+vertex.position[k];
            require(std::abs(std::hypot(p[0],p[1],p[2])-1737523)<0.002,"tile geometry preserves elevation and local float precision");
            require(p[0]*vertex.normal[0]+p[1]*vertex.normal[1]+p[2]*vertex.normal[2]>1737500,"terrain normals face outward");
        }
        for(std::size_t i=0;i<geometry.indices.size();i+=3) {
            const auto &a=geometry.vertices[geometry.indices[i]],&b=geometry.vertices[geometry.indices[i+1]],&c=geometry.vertices[geometry.indices[i+2]];
            double ab[3],ac[3];for(unsigned k=0;k<3;++k){ab[k]=b.position[k]-a.position[k];ac[k]=c.position[k]-a.position[k];}
            require((ab[1]*ac[2]-ab[2]*ac[1])*a.normal[0]+(ab[2]*ac[0]-ab[0]*ac[2])*a.normal[1]+(ab[0]*ac[1]-ab[1]*ac[0])*a.normal[2]>0,"terrain triangles face outward");
        }
        const auto adjacent=orbiter::terrain::BuildTileGeometry(14,512,1025,1737400,&*mountain);
        for(unsigned y=0;y<=32;++y)for(unsigned k=0;k<3;++k)
            require(std::abs(geometry.origin[k]+geometry.vertices[y*33+32].position[k]-adjacent.origin[k]-adjacent.vertices[y*33].position[k])<0.002,"adjacent terrain tiles meet");
        const auto skirt=orbiter::terrain::BuildTileGeometry(14,512,1024,1737400,&*mountain,32,100);
        require(skirt.vertices.size()==1089+128 && skirt.indices.size()==6144+128*6,"closed terrain skirt topology");
        for(std::size_t i=1089;i<skirt.vertices.size();++i) {
            const auto &vertex=skirt.vertices[i];double p[3];
            for(unsigned k=0;k<3;++k)p[k]=skirt.origin[k]+vertex.position[k];
            require(std::abs(std::hypot(p[0],p[1],p[2])-1737423)<0.002,"skirt extends radially downward by requested depth");
        }
        for(auto index:skirt.indices)require(index<skirt.vertices.size(),"skirt indices remain valid");
        for(unsigned i=0;i<1089;++i)require(skirt.vertices[i].position==geometry.vertices[i].position,"skirt preserves surface geometry");
        const double moonRadius=1737400;
        using orbiter::terrain::TerrainTileAboveHorizon;
        require(!TerrainTileAboveHorizon({10,32,0},{moonRadius+2,0,0},moonRadius,32000),"far side terrain is rejected");
        require(TerrainTileAboveHorizon({10,32,64},{moonRadius+2,0,0},moonRadius,32000),"near side terrain retained");
        require(TerrainTileAboveHorizon({10,32,0},{0,0,0},moonRadius,32000),"underground camera conservatively retains terrain");
        // Any point on the outer relief sphere that can see the eye around the
        // inner sphere must belong to a retained tile, including polar views.
        for(const auto eye:std::vector<std::array<double,3>>{{moonRadius+2,0,0},{0,moonRadius+200000,0},{-moonRadius*3,0,0}}) {
            const double distance=std::hypot(eye[0],eye[1],eye[2]);
            const double horizon=std::acos((moonRadius-32000)/distance)+std::acos((moonRadius-32000)/(moonRadius+32000));
            for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)
            for(double v:{0.0,0.5,1.0})for(double u:{0.0,0.5,1.0}) {
                const double lat=PI/2-(y+v)*PI/32,lon=-PI+(x+u)*PI/32;
                const double cosine=(eye[0]*std::cos(lat)*std::cos(lon)+eye[1]*std::sin(lat)+eye[2]*std::cos(lat)*std::sin(lon))/distance;
                if(std::acos(std::clamp(cosine,-1.0,1.0))<=horizon)
                    require(TerrainTileAboveHorizon({9,y,x},eye,moonRadius,32000),"visible elevated terrain never culled");
            }
        }
        const auto farTiles=orbiter::terrain::SelectTerrainTiles({moonRadius*100,0,0},moonRadius,1000);
        require(farTiles.size()==2,"distant planet keeps root tiles");
        for(const auto eye:std::vector<std::array<double,3>>{{moonRadius+2,0,0},{0,moonRadius+2,0},{-moonRadius-2,0,0},{0,0,0}}) {
            const auto selected=orbiter::terrain::SelectTerrainTiles(eye,moonRadius,1000);
            require(selected.size()<=512 && selected.size()>2,"near terrain selection stays within budget");
            double coverage=0;unsigned deepest=4;
            for(const auto &tile:selected) {
                coverage+=std::ldexp(0.5,-2*int(tile.level-4));deepest=std::max(deepest,tile.level);
                for(const auto &other:selected)if(other.level<tile.level) {
                    const unsigned shift=tile.level-other.level;
                    require(other.latitude!=(tile.latitude>>shift) || other.longitude!=(tile.longitude>>shift),"selected tiles never overlap ancestors");
                }
            }
            require(coverage==1 && deepest<=18,"selection covers whole planet without holes");
            if(eye[0]>moonRadius)require(deepest>=16,"landing camera receives fine terrain");
        }
        const double edgeSpan=std::acos(-1.0)/8192,edgeLongitude=-std::acos(-1.0)+4521*edgeSpan;
        auto boundaryHeight=[&](double lon,double){return lon<edgeLongitude?0.0:3.0;};
        const auto westEdge=orbiter::terrain::BuildTileGeometry(17,2792,4520,6371000,nullptr,32,0,boundaryHeight);
        const auto eastEdge=orbiter::terrain::BuildTileGeometry(17,2792,4521,6371000,nullptr,32,0,boundaryHeight);
        for(unsigned y=0;y<=32;++y) {
            double difference[3];for(unsigned k=0;k<3;++k)
                difference[k]=(westEdge.origin[k]+westEdge.vertices[y*33+32].position[k])-(eastEdge.origin[k]+eastEdge.vertices[y*33].position[k]);
            require(std::hypot(difference[0],difference[1],difference[2])<0.001,"adjacent terrain edges use identical geographic samples at discontinuous elevation boundaries");
        }
        Scratch scratch(argv[1]);
        const auto loose=std::filesystem::path("Textures/eArTh/eLeV/04/000000/000000.ELV");write(loose,bytes);
        const auto modification=std::filesystem::path("Textures/eArTh/eLeV_mOd/04/000000/000000.ELV");
        write(modification,tile(8,255));
        {std::ofstream config("Config/Earth.cfg",std::ios::app);config<<"\nElevationResolution = 0.5\n";}
        Orbiter host;host.Launch("orbit");host.Cfg()->CfgVisualPrm.ElevMode=1;host.Cfg()->CfgPRenderPrm.TileLoadFlags=3;
        char earthName[]="Earth";
        auto earth=reinterpret_cast<CelestialBody *>(oapiGetGbodyByName(earthName));
        {
            Manager manager(earth);require(manager.HasElevationTile(4,0,0),"loose tile existence uses VFS");
            std::unique_ptr<INT16[]> data(manager.LoadElevationTile(4,0,0,0.5));
            require(data && data[129*259+129]==1319,"simulation loader preserves target resolution");
            auto overlay=tile(8,255,2,3);overlay[100]=7;write(modification,overlay);
            require(manager.LoadElevationTile_mod(4,0,0,0.5,data.get()) && data[0]==34 && data[129*259+129]==1319,"simulation applies mixed-case masked modification");
            require(!manager.LoadElevationTile(4,0,0,0),"invalid target resolution rejected");
            write(loose,tile(16,65535));
            require(!manager.LoadElevationTile(4,0,0,1),"legacy INT16 conversion rejects overflow instead of wrapping");
        }
        write(loose,tile(-16,0,1,0,true));
        for(int interpolation:{1,2}) {
            host.Cfg()->CfgVisualPrm.ElevMode=interpolation;
            Manager slope(earth);Vector normal;
            const double lat=std::acos(-1.0)/3,spacing=std::acos(-1.0)*earth->Size()/256;
            slope.Elevation(lat,-std::acos(-1.0)/2,7,nullptr,&normal);
            require(std::abs(normal.x/normal.y+1/(spacing*std::cos(lat)))<1e-10 &&
                    std::abs(normal.z/normal.y+10/spacing)<1e-10,"terrain normal respects latitude spacing and half-metre elevation units");
        }
        auto curved=tile(-16);
        for(int y=0;y<259;++y)for(int x=0;x<259;++x) {
            const auto raw=static_cast<std::uint16_t>((x-129)*(y-129)/4);
            curved[100+2*(y*259+x)]=raw;curved[101+2*(y*259+x)]=raw>>8;
        }
        write(loose,curved);host.Cfg()->CfgVisualPrm.ElevMode=2;
        {
            Manager slope(earth);Vector normal;const double lat=0.3,lon=-2.0,h=1e-7;
            slope.Elevation(lat,lon,7,nullptr,&normal);
            const double east=(slope.Elevation(lat,lon+h,7)-slope.Elevation(lat,lon-h,7))/(2*h*earth->Size()*std::cos(lat));
            const double north=(slope.Elevation(lat+h,lon,7)-slope.Elevation(lat-h,lon,7))/(2*h*earth->Size());
            require(std::abs(normal.x/normal.y+east)<1e-8 && std::abs(normal.z/normal.y+north)<1e-8,"cubic normal matches numerical derivatives of sampled curved height");
        }
        write(loose,tile(0,0,1,42));
        const auto eastLoose=loose.parent_path()/"000001.ELV";write(eastLoose,tile(0,0,1,42));
        host.Cfg()->CfgVisualPrm.ElevMode=2;
        {
            Manager cubic(earth);std::vector<::ElevationTile> cache(2);
            const double pi=std::acos(-1.0);
            require(cubic.Elevation(0,-pi/2,7,&cache)==42,"warm cubic elevation cache");
            for(double lat:{-pi/2,0.0,pi/2})for(double lon:{-pi,-pi/2,0.0})
                require(std::abs(cubic.Elevation(lat,lon,7,&cache)-42)<1e-9,"cubic sampling remains valid at inclusive tile edges and corners");
        }
        for(double lat:{-std::acos(-1.0)/2,std::acos(-1.0)/2}) {
            Manager cold(earth);std::vector<::ElevationTile> cache(2);
            require(cold.Elevation(lat,-std::acos(-1.0),7,&cache)==42,"cold polar query resolves an in-range tile");
        }
        for(double longitude:{-std::acos(-1.0),std::acos(-1.0),3*std::acos(-1.0),-3*std::acos(-1.0)}) {
            Manager cold(earth);std::vector<::ElevationTile> cache(2);
            require(cold.Elevation(0,longitude,7,&cache)==42,"equivalent seam longitudes resolve the same tile from a cold cache");
        }
        write(eastLoose,tile(0,0,1,84));
        for(int interpolation:{1,2})for(bool eastFirst:{false,true}) {
            host.Cfg()->CfgVisualPrm.ElevMode=interpolation;
            Manager border(earth);std::vector<::ElevationTile> cache(4);
            border.Elevation(0,eastFirst?0.1:-0.1,7,&cache);
            border.Elevation(0,eastFirst?-0.1:0.1,7,&cache);
            require(border.Elevation(0,0,7,&cache)==84,"shared longitude edge has deterministic east-tile ownership regardless of cache order");
        }
        std::filesystem::remove(eastLoose);
        // A cached parent may cover a missing child, while its adjacent child
        // has detailed data. Exact latitude boundaries must not reuse the
        // parent's wrong quadrant or the neighbour's inclusive edge.
        for(bool detailedSouth:{false,true}) {
            const auto child=std::filesystem::path("Textures/eArTh/eLeV/05")/(detailedSouth?"000001":"000000")/"000000.ELV";
            write(child,tile(0,0,1,84));
            for(int interpolation:{1,2})for(bool southFirst:{false,true}) {
                host.Cfg()->CfgVisualPrm.ElevMode=interpolation;
                Manager border(earth);std::vector<::ElevationTile> cache(4);
                border.Elevation(southFirst?-0.1:0.1,-2.0,8,&cache);
                border.Elevation(southFirst?0.1:-0.1,-2.0,8,&cache);
                const auto expected=detailedSouth?84:42;
                require(border.Elevation(0,-2.0,8,&cache)==expected,"latitude boundary selects south child or parent fallback independent of cache order");
                Manager cold(earth);
                require(cold.Elevation(0,-2.0,8)==expected,"cached latitude boundary agrees with a cold lookup");
            }
            std::filesystem::remove(child);
        }
        // A corrupt loose tile must fall back to a valid archived tile.
        const auto flat=tile(0,0,1,123);
        uLongf size=compressBound(flat.size());Bytes compressed(size);
        require(compress2(compressed.data(),&size,flat.data(),flat.size(),Z_BEST_SPEED)==Z_OK,"compress elevation");compressed.resize(size);
        Bytes archive(80,0);archive[0]='T';archive[1]='X';archive[2]=1;u32(archive,4,48);u32(archive,8,1);u32(archive,12,80);u32(archive,16,size);u32(archive,24,1);
        for(unsigned at=28;at<48;at+=4)u32(archive,at,0xffffffff);u32(archive,40,0);u32(archive,56,flat.size());
        for(unsigned at=60;at<76;at+=4)u32(archive,at,0xffffffff);
        archive.insert(archive.end(),compressed.begin(),compressed.end());write("Textures/eArTh/aRcHiVe/eLeV.TrEe",archive);
        write(loose,Bytes{1,2,3});
        Manager archived(earth);std::unique_ptr<INT16[]> data(archived.LoadElevationTile(4,0,0,1));
        require(data && data[0]==123 && data[259*259-1]==123,"validated archive fallback reaches original elevation manager");
        std::cout<<"Elevation decoding, padded bilinear sampling and simulation file/archive loading verified\n";
        return 0;
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
