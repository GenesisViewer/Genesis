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
		// const std::string& ao = (*ao_it).first;
		
		// const LLSD& ao_map = (*ao_it).second;
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
    LL_INFOS()<< "New AO created " << newAO << LL_ENDL;
    gSavedPerAccountSettings.setLLSD("GenxAOSets",aoSets);

    
    return newAOID;
}
GenxAOSet GenxAOMgr::getAOSet(LLUUID id) {
    LL_INFOS() << "in get AO Set " << id << LL_ENDL;
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
                LL_INFOS() << "in get AO Set Default Anims" << id << LL_ENDL;
                LLSD defaultAnims  =  (*ao_it).get("defaultAnims");
                // LLSD::array_const_iterator default_it = defaultAnims.beginArray();
	            // LLSD::array_const_iterator default_end = defaultAnims.endArray();
                // for ( ; default_it != default_end; ++default_it)
	            // {
                //     std::string anim = (*default_it).get("anim");
                //     std::string name = (*default_it).get("name");
                //     genxAO.setDefaultAnim(name,anim);
                // }
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
    LL_INFOS() << "Finished get AO Set " << id << LL_ENDL;
    return genxAO;
}
void GenxAOMgr::updateAOSet(GenxAOSet genxAO) {
    LL_INFOS() << "In update AO Set " << genxAO.getID() << LL_ENDL;
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
        //aoSets[index]["defaultAnims"]=LLSD::emptyArray();
        //LLSD defaultAnims = LLSD::emptyMap();
        LL_INFOS() << "In update AO Set default anims" << genxAO.getID() << LL_ENDL;
        // std::map<std::string, std::string>::iterator defaultanim_iter = genxAO.getDefaultAnims().begin();
        // std::map<std::string, std::string>::iterator defaultanim_end  = genxAO.getDefaultAnims().end();

        for (auto const& defaultanim_iter : genxAO.getDefaultAnims() )
        {
        
            // LLSD newDefaultAnim = LLSD::emptyMap();
            // newDefaultAnim["name"] = it->first;
            // newDefaultAnim["anim"] = it->second;
            
            //defaultAnims.insert(defaultAnims.size(), newDefaultAnim);
            LL_INFOS() << "setting default admin" << LL_ENDL;
            LL_INFOS() << "name " << defaultanim_iter.first << LL_ENDL;
            LL_INFOS() << "value " << defaultanim_iter.second << LL_ENDL;
            aoSets[index]["defaultAnims"][defaultanim_iter.first] = defaultanim_iter.second;
            
        }  
        LL_INFOS() << "in update AO " << aoSets[index]["defaultAnims"] << LL_ENDL;   
        //aoSets[index]["defaultAnims"]=defaultAnims;
        LL_INFOS() << "in update AO before save" << aoSets << LL_ENDL;
        gSavedPerAccountSettings.setLLSD("GenxAOSets",aoSets);
        
    }
    LL_INFOS() << "Finished update AO Set " << genxAO.getID() << LL_ENDL;

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
