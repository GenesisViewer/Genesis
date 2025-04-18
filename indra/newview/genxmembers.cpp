#include "llviewerprecompiledheaders.h"
#include "genxmembers.h"


GenxMembers::GenxMembers()
{
	members.push_back("d66dab61-0b8e-427f-82d0-eb5872a73531"); //Billy Arentire
    	members.push_back("0de266c4-8bbe-4ba5-9217-20d22d04f36e"); //Torric Rodas
	members.push_back("87ae9358-8df0-48e9-a77f-194fe79c6dc1"); //Mely
	members.push_back("ecf77488-7ead-4e75-80ff-88661bce6037"); //Shep62
	// members.push_back("b0fd5e6d-fb23-4608-9aee-4928225722d3"); //illianna1  - "sabbatical"
	members.push_back("82d88055-7897-4e5b-8804-e5befe86d92f"); //Coffeequeen68
	members.push_back("a524cfd5-0e03-4c2a-865c-747bb2fa0f16"); //biyayo
	members.push_back("64bcffb1-6466-40fe-affd-b97319692c55"); //Jazzi Slade
	members.push_back("9f9011e4-de9d-4725-abb2-bfa1a21d5918"); //Evie Falconer
	members.push_back("76234dcc-07f0-472a-bf46-230df2037997"); //DeadlyBellaDonna Resident
	members.push_back("86d8bab7-b82e-4c34-b991-ea19f67c8226"); //Roni Nightingale
	members.push_back("3ac2cf73-eefa-4102-8016-7676a9a0081e"); //candicek15 Resident
	members.push_back("dc5f4e67-9c08-49a9-977e-4de91a4fed74"); //meka Gearbox
	members.push_back("5573bf71-d46e-444b-af40-4f3127af6da2"); //Fidget Nebula
	members.push_back("7ecdc21b-d83d-415a-bda0-ce2eac92550d"); // PuddingPop Jillybean
	
	
	
}
bool GenxMembers::isMember(LLUUID uuid) {
	auto it = std::find(members.begin(), members.end(), uuid.asString());
    return (it != members.end());
}
