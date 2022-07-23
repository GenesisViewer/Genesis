
#include <stdio.h>

#include "sqlite3.h"
#include "v4color.h"

#ifndef LLTAGGEDAVATARSMGR_H
#define LLTAGGEDAVATARSMGR_H

class LLTaggedAvatarsMgr : public LLSingleton<LLTaggedAvatarsMgr>
{
public:
	LLTaggedAvatarsMgr();
	~LLTaggedAvatarsMgr();

    // viewer auth version
	char init();
    
    
    std::map<std::string, std::string> getContactSets();
    void updateColorContactSet(std::string csId, LLColor4 color);
    void updateContactSet(std::string avatarId, std::string contactSet,std::string avatarName);
    void updateContactSetName(std::string csId, std::string csAlias);
    void deleteContactSet(std::string csId);
    void deleteAvatarContactSet(std::string avatarId);
    std::string getAvatarContactSetId(std::string avatarId);
    std::string getAvatarContactSetName(std::string avatarId);
    LLColor4 getAvatarColorContactSet(std::string avatarId);
    LLColor4 getColorContactSet(std::string csId);
    std::string getContactSetName(std::string csId);
    std::string insertContactSet(std::string csId, std::string csAlias);
private:
    std::map<std::string, LLColor4> mTaggedAvatarsColors;
    std::map<std::string, std::string> mTaggedAvatarsContactSetName;
};

#endif /* LLTAGGEDAVATARSMGR_H */
