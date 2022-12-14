#include "llpaneleditcontactset.h"
#include "lluictrlfactory.h"

#include "genxcontactset.h"
#include "llcolorswatch.h"
#include "llfloatercontacts.h"
#include "llvoavatar.h"
LLPanelEditContactSet::LLPanelEditContactSet() {
	LLCallbackMap::map_t factory_map;
	LLUICtrlFactory::getInstance()->buildFloater(this, "floatereditcontactset.xml", &factory_map);
}
void LLPanelEditContactSet::setContactSetID(const std::string& contact_set_id) {
	mContactSet = contact_set_id;
	ContactSet contactSet = GenxContactSetMgr::instance().getContactSet(contact_set_id);
	std::string name = contactSet.getName();
	LLColor4 color = contactSet.getColor();
	getChild<LLLineEditor>("contactset_name")->setValue(name);
	getChild<LLColorSwatchCtrl>("contactset_color")->set(color);
	childSetAction("Apply", saveContactSet, this);

}
void LLPanelEditContactSet::setContactSetPanel(LLPanelContactSets* contact_set_panel) {
	mContactSetPanel = contact_set_panel;
}
void LLPanelEditContactSet::saveContactSet(void* userdata) {
	LLPanelEditContactSet* self = (LLPanelEditContactSet*)userdata;
	std::string name = self->getChild<LLLineEditor>("contactset_name")->getValue();
	LLColor4 color = self->getChild<LLColorSwatchCtrl>("contactset_color")->get();

	if (name.length()>0) {
		ContactSet cs = ContactSet();
		cs.setName(name);
		cs.setColor(color);
		cs.setId(self->mContactSet);
		GenxContactSetMgr::instance().updateContactSet(cs);

	}
	self->mContactSetPanel->reset();
	self->close();


	//reset the client tag manager to update all avatars tag
	SHClientTagMgr::getInstance()->resetAvatarTags();
}