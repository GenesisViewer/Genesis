#include "llviewerprecompiledheaders.h"
#include "genxmembers.h"


GenxMembers::GenxMembers()
{
	members.push_back("d66dab61-0b8e-427f-82d0-eb5872a73531"); //Billy Arentire
    	members.push_back("0de266c4-8bbe-4ba5-9217-20d22d04f36e"); //Torric Rodas
	members.push_back("87ae9358-8df0-48e9-a77f-194fe79c6dc1"); //Mely
	members.push_back("ecf77488-7ead-4e75-80ff-88661bce6037"); //Shep62
	members.push_back("b0fd5e6d-fb23-4608-9aee-4928225722d3"); //illianna1
}
bool GenxMembers::isMember(LLUUID uuid) {
	auto it = std::find(members.begin(), members.end(), uuid.asString());
    return (it != members.end());
}
