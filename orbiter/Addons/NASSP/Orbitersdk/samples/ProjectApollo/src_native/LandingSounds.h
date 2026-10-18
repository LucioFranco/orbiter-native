/* Modified for native portability. Publication notice added 2026-10-18.
 * Original copyright, license grants and linking exceptions remain unchanged.
 * Behavioral changes are inventoried in upstream/nassp-v9/manifest.json.
 */
#pragma once
#include <algorithm>
#include <cmath>
#include <istream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace nassp::native {
struct LandingSound {
    std::string file;
    double met=0,altitude=-1,toIgnition=0,afterIgnition=0,toApproach=0,offset=-1;
    int mode=-1;
    bool mandatory=false;
};
inline double SoundTime(const std::string &text) {
    std::istringstream in(text);int h,m;double s;char a,b;
    if(!(in>>h>>a>>m>>b>>s) || a!=':' || b!=':' || h<0 || m<0 || m>=60 ||
       !std::isfinite(s) || s<0 || s>=60 || (in>>std::ws,!in.eof()))
        throw std::runtime_error("Invalid mission sound time: "+text);
    return h*3600.0+m*60+s;
}
inline std::vector<std::string> SoundFields(std::string line) {
    if(!line.empty() && line.back()=='\r')line.pop_back();
    std::vector<std::string> fields;size_t start=0;
    for(;;) {auto end=line.find(';',start);fields.push_back(line.substr(start,end-start));
        if(end==std::string::npos)break;start=end+1;}
    return fields;
}
inline std::vector<LandingSound> ReadLandingSounds(std::istream &input,const std::string &directory) {
    std::string line;
    if(!std::getline(input,line))throw std::runtime_error("Empty landing sound table");
    auto fields=SoundFields(line);const bool grouped=fields[0]=="type2";
    if(grouped) {if(!std::getline(input,line))throw std::runtime_error("Missing sound prefix");fields=SoundFields(line);}
    const auto prefix=fields[0];if(prefix.empty())throw std::runtime_error("Empty sound prefix");
    if(!std::getline(input,line))throw std::runtime_error("Missing landing sound columns");
    std::vector<LandingSound> events;int previous=-1;double groupStart=0;
    while(std::getline(input,line)) {
        if(line.empty() || line=="\r" || line[0]=='#')continue;
        fields=SoundFields(line);if(fields.size()<7)throw std::runtime_error("Truncated landing sound row");
        LandingSound event;event.met=SoundTime(fields[0]);
        if(!events.empty() && event.met<events.back().met)throw std::runtime_error("Unordered landing sound time");
        size_t used=0;const int number=std::stoi(fields[1],&used);
        if(used!=fields[1].size() || number<0)throw std::runtime_error("Invalid recording number");
        event.file=directory+"/"+prefix+(number<10?"0":"")+std::to_string(number)+".wav";
        if(grouped) {if(number!=previous)groupStart=event.met;event.offset=event.met-groupStart;}
        previous=number;
        if(!fields[2].empty()) {
            event.altitude=std::stod(fields[2],&used)*0.3048;
            if(used!=fields[2].size() || !std::isfinite(event.altitude) || event.altitude<0)
                throw std::runtime_error("Invalid sound altitude");
        }
        event.mode=fields[3]=="o"?0:fields[3]=="b"?1:fields[3]=="a"?2:-1;
        if(event.mode<0)throw std::runtime_error("Invalid landing phase");
        event.toIgnition=SoundTime(fields[4])-event.met;event.afterIgnition=std::max(0.0,-event.toIgnition);
        event.mandatory=fields[5]=="m";event.toApproach=SoundTime(fields[6])-event.met;
        events.push_back(event);
    }
    // Interpolate only between known anchors; never read a sentinel past the end.
    size_t anchor=0;
    while(anchor<events.size() && events[anchor].altitude<0)++anchor;
    for(size_t next=anchor+1;next<events.size();++next)if(events[next].altitude>=0) {
        const double span=events[next].met-events[anchor].met;
        for(size_t i=anchor+1;i<next;++i) {
            const double fraction=span>0?(events[i].met-events[anchor].met)/span:0;
            events[i].altitude=events[anchor].altitude+fraction*(events[next].altitude-events[anchor].altitude);
        }
        anchor=next;
    }
    if(events.empty())throw std::runtime_error("No landing sound events");
    return events;
}
struct LandingClock {
    int mode;
    double met,toIgnition,afterIgnition,toApproach,altitude;
};
struct LandingSelection {size_t index;double offset;};
inline std::optional<LandingSelection> SelectLandingSound(const std::vector<LandingSound> &events,
                                                         size_t next,const LandingClock &clock) {
    if(clock.mode<0 || clock.mode>3)return std::nullopt;
    auto due=[&](const LandingSound &event) {
        if(clock.mode!=3 && event.mode!=clock.mode)return false;
        switch(clock.mode) {
        case 0:return std::isfinite(clock.toIgnition) && clock.toIgnition<=event.toIgnition;
        case 1:return clock.afterIgnition<160?
            std::isfinite(clock.afterIgnition) && clock.afterIgnition>=event.afterIgnition:
            std::isfinite(clock.toApproach) && clock.toApproach<=event.toApproach;
        case 2:return event.altitude>=0 && std::isfinite(clock.altitude) && clock.altitude<=event.altitude;
        case 3:return std::isfinite(clock.met) && clock.met>=event.met;
        }
        return false;
    };
    while(next<events.size() && clock.mode!=3 && events[next].mode<clock.mode)++next;
    if(next>=events.size() || !due(events[next]))return std::nullopt;
    while(next+1<events.size() && !events[next].mandatory && due(events[next+1]))++next;
    const auto &event=events[next];
    const double late=clock.mode==0?std::max(0.0,event.toIgnition-clock.toIgnition):0;
    return LandingSelection{next,event.offset<0?-1:event.offset+late};
}

}
