#include "llviewerprecompiledheaders.h"
#include "genxmembers.h"


GenxMembers::GenxMembers()
{
	members.push_back("d66dab61-0b8e-427f-82d0-eb5872a73531"); //Billy Arentire
    	members.push_back("0de266c4-8bbe-4ba5-9217-20d22d04f36e"); //Torric Rodas
	members.push_back("87ae9358-8df0-48e9-a77f-194fe79c6dc1"); //Mely
	members.push_back("ecf77488-7ead-4e75-80ff-88661bce6037"); //Shep62
	members.push_back("b0fd5e6d-fb23-4608-9aee-4928225722d3"); //illianna1
	members.push_back("8f65f237-fd7e-4fb9-821c-0e2acd2b1ff3"); //XLR8RRICK
	members.push_back("82d88055-7897-4e5b-8804-e5befe86d92f"); //Coffeequeen68
	members.push_back("9fc36b86-f5dd-43ab-bfeb-f97b5701aabb"); //Wolfgang Krautrauch
	members.push_back("96f9c1a5-bad5-4e71-a491-07382979020c"); //LadyDeeFul
	members.push_back("3ac2cf73-eefa-4102-8016-7676a9a0081e"); //candicek15
	members.push_back("a524cfd5-0e03-4c2a-865c-747bb2fa0f16"); //biyayo
	
	
	
	
}
bool GenxMembers::isMember(LLUUID uuid) {
	auto it = std::find(members.begin(), members.end(), uuid.asString());
    return (it != members.end());
}
