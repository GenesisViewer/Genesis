#include "llsingleton.h"    
#ifndef GENX_MEMBERS_H
#define GENX_MEMBERS_H



class GenxMembers: public LLSingleton<GenxMembers>
{
 public:
	GenxMembers();
	bool isMember(LLUUID uuid);
 private:
    std::list<std::string> members;    
};

#endif // GENX_MEMBERS_H