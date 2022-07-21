#include "llfloater.h"
#include "llfloatercontacts.h"
class LLPanelEditContactSet : public LLFloater
{
public:
	LLPanelEditContactSet();
    void setContactSetID(const std::string& contact_set_id);
    void setContactSetPanel(LLPanelContactSets* contact_set_panel);
protected:
	std::string mContactSet;    
    LLPanelContactSets* mContactSetPanel;
    static void saveContactSet(void* userdata);
};