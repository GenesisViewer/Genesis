#include "llviewerprecompiledheaders.h"
#include <stdio.h>
#include "llsingleton.h"


#ifndef GENXAOMGR_H
#define GENXAOMGR_H
class GenxAOSet {
public:
    
    LLUUID getID(){return id;}
    LLUUID getNoteCardID(){return notecardId;}
    std::string getName() {return name;}
    bool getRandomizeStand() {return randomizeStand;}
    bool getDisableStandMouselook() {return disableStandMouselook;}
    int  getStandTime() {return standTime;}
    void setID(LLUUID newID) {id = newID;}
    void setNotecardID(LLUUID newID) {notecardId = newID;}
    void setName(std::string newName) {name=newName;}
    void setRandomizeStand(bool newValue) {randomizeStand=newValue;}
    void setDisableStandMouselook(bool newValue) {disableStandMouselook=newValue;}
    void setStandTime(int newValue) {standTime=newValue;}
    std::string getDefaultAnim(std::string anim) {return defaultAnims[anim];}
    void setDefaultAnim(std::string anim, std::string aoID) {LL_INFOS() << "in setdefaultanim setting default anim" << anim <<LL_ENDL;defaultAnims[anim] = aoID;}
    std::map<std::string, std::string> getDefaultAnims(){return defaultAnims;} 
     
private:
    LLUUID id;
    std::string name;
    LLUUID notecardId;
    bool randomizeStand;
    bool disableStandMouselook;
    int  standTime;
    std::map<std::string, std::string> defaultAnims={};

};
class GenxAOMgr : public LLSingleton<GenxAOMgr>
{
    public:
        GenxAOMgr();
        ~GenxAOMgr();
        std::vector<GenxAOSet> getAoSets();
        LLUUID newAOSet();
        GenxAOSet getAOSet(LLUUID id);
        void updateAOSet(GenxAOSet genxAO);
        void deleteAOSet(GenxAOSet genxAO);
        
};
#endif //GENXAOMGR_H