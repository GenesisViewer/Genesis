#include "genxaomgr.h"
#include "llviewercontrol.h"
GenxAOMgr::GenxAOMgr() {

}
GenxAOMgr::~GenxAOMgr() {
    
}
std::vector<GenxAOSet> GenxAOMgr::getAoSets() {
    std::vector<GenxAOSet> sets;
    LLSD aoSets = gSavedPerAccountSettings.getLLSD("GenxAOSets");
    LLSD::array_const_iterator ao_it = aoSets.beginArray();
	LLSD::array_const_iterator ao_end = aoSets.endArray();
	for ( ; ao_it != ao_end; ++ao_it)
	{
		
        GenxAOSet aoSet= GenxAOSet();
        aoSet.setID(LLUUID((*ao_it).get("id")));
        if ((*ao_it).has("name"))
            aoSet.setName((*ao_it).get("name").asString());
        if ((*ao_it).has("notecardID"))
            aoSet.setNotecardID((*ao_it).get("notecardID").asUUID());

        sets.push_back(aoSet);        
    }  
    return sets;
}
LLUUID GenxAOMgr::newAOSet() {
    LLUUID newAOID=LLUUID::generateNewID();
    LLSD aoSets = gSavedPerAccountSettings.getLLSD("GenxAOSets");
    LLSD newAO = LLSD(newAOID);
    newAO.insert("id",newAOID);
    newAO.insert("name","New AO Set");
    newAO.insert("standTime",20);
    newAO.insert("defaultAnims",LLSD::emptyMap());
    aoSets.append(newAO);
   
    gSavedPerAccountSettings.setLLSD("GenxAOSets",aoSets);

    
    return newAOID;
}
GenxAOSet GenxAOMgr::getAOSet(LLUUID id) {
   
    GenxAOSet genxAO = GenxAOSet();
    LLSD aoSets = gSavedPerAccountSettings.getLLSD("GenxAOSets");
    LLSD::array_const_iterator ao_it = aoSets.beginArray();
	LLSD::array_const_iterator ao_end = aoSets.endArray();
	for ( ; ao_it != ao_end; ++ao_it)
	{
        LLUUID aoId = LLUUID((*ao_it).get("id"));
        if (aoId == id) {
            genxAO.setID(aoId);
            if ((*ao_it).has("name"))
                genxAO.setName((*ao_it).get("name").asString());
            if ((*ao_it).has("notecardID"))
                genxAO.setNotecardID((*ao_it).get("notecardID").asUUID());
            if ((*ao_it).has("randomizeStand"))
                genxAO.setRandomizeStand((*ao_it).get("randomizeStand").asBoolean());
            if ((*ao_it).has("disableStandMouselook"))
                genxAO.setDisableStandMouselook((*ao_it).get("disableStandMouselook").asBoolean());
            if ((*ao_it).has("standTime"))
                genxAO.setStandTime((*ao_it).get("standTime").asInteger());      
            if ((*ao_it).has("defaultAnims")) {
              
                LLSD defaultAnims  =  (*ao_it).get("defaultAnims");
                
                LLSD::map_const_iterator default_it = defaultAnims.beginMap();
                LLSD::map_const_iterator default_end = defaultAnims.endMap();
                while(default_it != default_end)
	            {
                    std::string anim = (*default_it).first;
                    std::string name = (*default_it).second;
                    genxAO.setDefaultAnim(anim,name);
                    default_it++;
                }
            }
                
            break;
        }
    }
   
    return genxAO;
}
void GenxAOMgr::updateAOSet(GenxAOSet genxAO) {
   
    LLSD aoSets = gSavedPerAccountSettings.getLLSD("GenxAOSets");
    LLSD::array_const_iterator ao_it = aoSets.beginArray();
	LLSD::array_const_iterator ao_end = aoSets.endArray();
    int index = 0;
    bool found=false;
	for ( ; ao_it != ao_end; ++ao_it)
	{
        LLUUID aoId = LLUUID((*ao_it).get("id"));
        if (aoId == genxAO.getID()) {
            found=true;
            break;

        }
        index++;
    }
    if (found) {
        aoSets[index]["name"]=genxAO.getName();
        aoSets[index]["notecardID"]=genxAO.getNoteCardID();
        aoSets[index]["randomizeStand"]=genxAO.getRandomizeStand();
        aoSets[index]["disableStandMouselook"]=genxAO.getDisableStandMouselook();
        aoSets[index]["standTime"]=genxAO.getStandTime();
       
        for (auto const& defaultanim_iter : genxAO.getDefaultAnims() )
        {
        
               
            aoSets[index]["defaultAnims"][defaultanim_iter.first] = defaultanim_iter.second;
            
        }  
        
       
        gSavedPerAccountSettings.setLLSD("GenxAOSets",aoSets);
        
    }
    

}
void GenxAOMgr::deleteAOSet(GenxAOSet genxAO) {
    LLSD aoSets = gSavedPerAccountSettings.getLLSD("GenxAOSets");
    LLSD::array_const_iterator ao_it = aoSets.beginArray();
	LLSD::array_const_iterator ao_end = aoSets.endArray();
    int index = 0;
    bool found=false;
	for ( ; ao_it != ao_end; ++ao_it)
	{
        LLUUID aoId = LLUUID((*ao_it).get("id"));
        if (aoId == genxAO.getID()) {
            found=true;
            break;

        }
        index++;
    }
    if (found) {
        aoSets.erase(index);
        gSavedPerAccountSettings.setLLSD("GenxAOSets",aoSets);
        
    }

}
