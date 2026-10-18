#include "LandingSounds.h"
#include <fstream>
#include <iostream>
static void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv){try {
    require(argc==2,"NASSP sound root required");
    const std::string root=argv[1];
    std::ifstream ap11(root+"/Apollo11/sound.csv"),ap12(root+"/Apollo12/sound.csv");
    auto first=nassp::native::ReadLandingSounds(ap11,"Apollo11");
    auto second=nassp::native::ReadLandingSounds(ap12,"Apollo12");
    require(first.size()>50 && second.size()>30,"original mission event tables parse");
    require(first.front().file=="Apollo11/ap11landingmono81.wav" && first.front().offset==-1,"individual recording path");
    require(std::abs(first.front().altitude-53000*0.3048)<1e-9 && first[1].altitude>0,"feet conversion and missing altitude interpolation");
    require(second[0].offset==0 && second[1].offset==25 && second[0].file==second[1].file,"grouped recording offsets");
    require(first.back().mandatory && first.back().mode==2,"mandatory final landing event");
    std::istringstream bad("prefix;\nMET;\ninvalid;1;0;o;1:00:00;;1:01:00\n");
    bool rejected=false;try {nassp::native::ReadLandingSounds(bad,"");}catch(const std::exception &){rejected=true;}
    require(rejected,"malformed event rejected without partial result");
    nassp::native::LandingClock clock{0,0,first.front().toIgnition+1,0,0,0};
    require(!nassp::native::SelectLandingSound(first,0,clock),"pre-ignition call waits for threshold");
    clock.toIgnition=first.front().toIgnition;
    auto selection=nassp::native::SelectLandingSound(first,0,clock);
    require(selection && selection->index==0 && selection->offset==-1,"first individual call becomes due");
    require(!nassp::native::SelectLandingSound(first,1,clock),"consumed call does not repeat");
    clock={2,0,0,0,0,0};
    selection=nassp::native::SelectLandingSound(first,0,clock);
    require(selection && first[selection->index].mandatory,"mandatory call survives large altitude jump");
    std::vector<nassp::native::LandingSound> shortList(3);
    for(size_t i=0;i<3;++i){shortList[i].mode=2;shortList[i].altitude=100-10*i;}
    selection=nassp::native::SelectLandingSound(shortList,0,clock);
    require(selection && selection->index==2,"stale optional calls coalesce to latest due event");
    shortList[1].mandatory=true;selection=nassp::native::SelectLandingSound(shortList,0,clock);
    require(selection && selection->index==1,"mandatory intermediate event is retained");
    require(!nassp::native::SelectLandingSound(shortList,3,clock),"end of table is bounded");
    std::cout<<"Apollo 11 events="<<first.size()<<" Apollo 12 events="<<second.size()<<'\n';
    return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
