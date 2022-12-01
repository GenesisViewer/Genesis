#include "llviewerprecompiledheaders.h"
#include "genxmembers.h"


GenxMembers::GenxMembers()
{
	members.push_back("d66dab61-0b8e-427f-82d0-eb5872a73531"); //Billy Arentire
    members.push_back("0de266c4-8bbe-4ba5-9217-20d22d04f36e"); //Torric Rodas
}
bool GenxMembers::isMember(LLUUID uuid) {
	auto it = std::find(members.begin(), members.end(), uuid.asString());
    return (it != members.end());
}