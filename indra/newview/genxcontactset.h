#include <stdio.h>

#include "sqlite3.h"
#include "v4color.h"

#ifndef GENXCONTACTSET_H
#define GENXCONTACTSET_H

class ContactSet {
    public:
        std::string getId(){return id;}
        std::string getName(){return name;}
        LLColor4 getColor(){return color;}
        void setName(std::string newName) {name=newName;}
        void setColor(LLColor4 newColor) {color=newColor;}
        void setId(std::string newId) {id=newId;}
    private:
        std::string id;
        LLColor4 color;
        std::string name;    
};

class GenxContactSetMgr : public LLSingleton<GenxContactSetMgr>
{
    public:
        GenxContactSetMgr();
        ~GenxContactSetMgr();
        void init();
        ContactSet getContactSet(std::string csId);
        std::vector<ContactSet> getContactSets();
        ContactSet getAvatarContactSet(std::string avatarId);
        void resetAvatarContactSet(std::string avatarId);
        void updateAvatarContactSet(std::string avatarId, std::string contactSetId);
        void updateContactSet(ContactSet contactSet);
        std::string insertContactSet(std::string csId, std::string csAlias);
        void deleteContactSet(std::string csId);
    private:
        std::map<std::string, ContactSet> cache;

};
#endif /* GENXCONTACTSET_H */