#include "llviewerprecompiledheaders.h"
#include "aosystem.h"
#include "genxfloaterao.h"
#include "genxaomgr.h"
#include "lluictrlfactory.h"
#include "llcombobox.h"
#include "llinventorypanel.h"
#include "llagent.h"
#include "llpreviewnotecard.h"
#include "lltexteditor.h"
#include "genxdroptarget.h"
#include "aosystem.h"
#include "roles_constants.h"
#include "llcheckboxctrl.h"
#include "llspinctrl.h"
class GenxAONotecardCallback final : public LLInventoryCallback
{
public:
	GenxAONotecardCallback(const std::string& filename) : mFileName(filename) {}

	void fire(const LLUUID &inv_item) override
	{
		if (!mFileName.empty())
		{ 
			auto nc = (LLPreviewNotecard*)LLPreview::show(inv_item);
			if (!nc)
			{
				auto item = gInventory.getItem(inv_item);
				open_notecard(item, "Note: " + item->getName(), LLUUID::null, false);
				nc = (LLPreviewNotecard*)LLPreview::find(inv_item);
			}

			if (nc)
			{
				if (LLTextEditor *text = nc->getEditor())
				{
					text->clear();
					text->makePristine();

					std::ifstream file(mFileName.c_str());

					std::string line;
					while (!file.eof())
					{
						getline(file, line);
						text->insertText(line + '\n');
					}
					file.close();

					nc->saveIfNeeded();
				}
			}
		}
	}

private:
	std::string mFileName;
};


GenxFloaterAO::GenxFloaterAO(const LLSD&) : LLFloater("floater_ao"), mCombos({})
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "genx_floater_ao.xml", nullptr, false);
}

void GenxFloaterAO::onOpen()
{
	
}

GenxFloaterAO::~GenxFloaterAO()
{
//	LL_INFOS() << "floater destroyed" << LL_ENDL;
}

BOOL GenxFloaterAO::postBuild()
{
    LLButton *new_ao_button = getChild<LLButton>("new_ao_set");
    new_ao_button->setCommitCallback(boost::bind(&GenxFloaterAO::onClickNewAOSet, this));
    LLButton *del_ao_button = getChild<LLButton>("del_ao_set");
    del_ao_button->setCommitCallback(boost::bind(&GenxFloaterAO::onDeleteAOSet, this));
    LLButton *rename_ao_button = getChild<LLButton>("rename_ao_set");
    rename_ao_button->setCommitCallback(boost::bind(&GenxFloaterAO::onRenameAOSet, this));
    LLButton *new_ao_set_name_ok = getChild<LLButton>("new_ao_set_name_ok");
    new_ao_set_name_ok->setCommitCallback(boost::bind(&GenxFloaterAO::onRenameAOSetOK, this));
    LLButton *new_ao_set_name_cancel = getChild<LLButton>("new_ao_set_name_cancel");
    new_ao_set_name_cancel->setCommitCallback(boost::bind(&GenxFloaterAO::onRenameAOSetCancel, this));
    getChild<LLUICtrl>("newcard")->setCommitCallback(boost::bind(&GenxFloaterAO::onClickNewCard, this));
    getChild<LLUICtrl>("reload")->setCommitCallback(boost::bind(&GenxFloaterAO::onClickReloadCard, this));
    getChild<LLUICtrl>("viewcard")->setCommitCallback(boost::bind(&GenxFloaterAO::onClickOpenCard, this));
    getChild<LLUICtrl>("prevstand")->setCommitCallback(boost::bind(&GenxFloaterAO::onClickCycleStand, this, false));
	getChild<LLUICtrl>("nextstand")->setCommitCallback(boost::bind(&GenxFloaterAO::onClickCycleStand, this, true));
    getChild<LLCheckBoxCtrl>("AOStandRandomize")->setCommitCallback(boost::bind(&GenxFloaterAO::onClickStandRandomize, this));
    getChild<LLCheckBoxCtrl>("AONoStandsInMouselook")->setCommitCallback(boost::bind(&GenxFloaterAO::onClickNoStandInMouselook, this));
    getChild<LLSpinCtrl>("standtime")->setCommitCallback(boost::bind(&GenxFloaterAO::onChangeStandTime, this));
    LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    aosets_combo->setCommitCallback(boost::bind(&GenxFloaterAO::onSelectAOSet,this));
    getChild<GenxDropTarget>("ao_notecard")->setCommitCallback(boost::bind(&GenxFloaterAO::onInventoryItemDropped, this));
    refreshAOSets();
    const auto& cb = boost::bind(&GenxFloaterAO::onComboBoxCommit, this, _1);
    auto ao = AOSystem::getIfExists();
    const auto& setup_combo = [&](const std::string& name, const AOState& state) {
		if (auto combo = findChild<LLComboBox>(name))
		{
			combo->setCommitCallback(cb);
			LLControlVariablePtr setting = nullptr;
			if (auto aop = ao ? ao->mAOOverrides[state] : nullptr)
				setting = aop->setting;
			else // AO is down or missing override struct(why?), try getting from UI
				setting = combo->getControl(combo->getControlName());
			if (setting) combo->add(setting->get().asStringRef(), ADD_BOTTOM, true);
			mCombos[state] = combo;
		}
	};
	setup_combo("stands", STATE_AGENT_IDLE);
    setup_combo("walks", STATE_AGENT_WALK);
	setup_combo("runs", STATE_AGENT_RUN);
	setup_combo("prejumps", STATE_AGENT_PRE_JUMP);
	setup_combo("jumps", STATE_AGENT_JUMP);
	setup_combo("turnlefts", STATE_AGENT_TURNLEFT);
	setup_combo("turnrights", STATE_AGENT_TURNRIGHT);
	setup_combo("sits", STATE_AGENT_SIT);
	setup_combo("gsits", STATE_AGENT_SIT_GROUND);
	setup_combo("hovers", STATE_AGENT_HOVER);
	setup_combo("flydowns", STATE_AGENT_HOVER_DOWN);
	setup_combo("flyups", STATE_AGENT_HOVER_UP);
	setup_combo("crouchs", STATE_AGENT_CROUCH);
	setup_combo("cwalks", STATE_AGENT_CROUCHWALK);
	setup_combo("falls", STATE_AGENT_FALLDOWN);
	setup_combo("standups", STATE_AGENT_STANDUP);
	setup_combo("lands", STATE_AGENT_LAND);
	setup_combo("flys", STATE_AGENT_FLY);
	setup_combo("flyslows", STATE_AGENT_FLYSLOW);
	setup_combo("typings", STATE_AGENT_TYPE);
	setup_combo("swimdowns", STATE_AGENT_SWIM_DOWN);
	setup_combo("swimups", STATE_AGENT_SWIM_UP);
	setup_combo("swims", STATE_AGENT_SWIM);
	setup_combo("floats", STATE_AGENT_FLOAT);
    LLUUID genxAOUsed = LLUUID(gSavedPerAccountSettings.getString("GenxAOUsed"));
    GenxAOSet genxAO = GenxAOMgr::getInstance()->getAOSet(genxAOUsed);
    if (genxAO.getID().isNull()) 
    {
        aosets_combo->selectFirstItem();
    }
    else 
    {
        aosets_combo->setValue(genxAO.getID());
        
    }
    onSelectAOSet();

    //build the map for default animations controls.
    //we need to map the control name with the global settings
    mapCombosToControlNames["walks"] = "AODefaultWalk";
	mapCombosToControlNames["runs"] = "AODefaultRun";
	mapCombosToControlNames["prejumps"] = "AODefaultPreJump";
	mapCombosToControlNames["jumps"] = "AODefaultJump";
	mapCombosToControlNames["sits"] = "AODefaultSit";
	mapCombosToControlNames["gsits"] = "AODefaultGroundSit";
	mapCombosToControlNames["hovers"] = "AODefaultHover";
	mapCombosToControlNames["flydowns"] = "AODefaultFlyDown";
	mapCombosToControlNames["flyups"] = "AODefaultFlyUp";
	mapCombosToControlNames["crouchs"] = "AODefaultCrouch";
	mapCombosToControlNames["cwalks"] = "AODefaultCrouchWalk";
	mapCombosToControlNames["falls"] = "AODefaultFall";
	mapCombosToControlNames["standups"] = "AODefaultStandUp";
	mapCombosToControlNames["lands"] = "AODefaultLand";
	mapCombosToControlNames["flys"] = "AODefaultFly";
	mapCombosToControlNames["flyslows"] = "AODefaultFlySlow";
	mapCombosToControlNames["typings"] = "AODefaultTyping";
	mapCombosToControlNames["swimdowns"] = "AODefaultSwimDown";
	mapCombosToControlNames["swimups"] = "AODefaultSwimUp";
	mapCombosToControlNames["swims"] = "AODefaultSwim";
	mapCombosToControlNames["floats"] = "AODefaultFloat";
    return true;
}
void GenxFloaterAO::onChangeStandTime() {
    LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    LLUUID aoId = aosets_combo->getSelectedValue();
    GenxAOSet genxAO = GenxAOMgr::getInstance()->getAOSet(aoId);
    genxAO.setStandTime(getChild<LLSpinCtrl>("standtime")->getValue().asInteger());
    gSavedSettings.setF32("AOStandInterval", genxAO.getStandTime());
    GenxAOMgr::getInstance()->updateAOSet(genxAO);

}
void GenxFloaterAO::onClickStandRandomize() {
    LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    LLUUID aoId = aosets_combo->getSelectedValue();
    GenxAOSet genxAO = GenxAOMgr::getInstance()->getAOSet(aoId);
    genxAO.setRandomizeStand(getChild<LLCheckBoxCtrl>("AOStandRandomize")->get());
    gSavedSettings.setBOOL("AOStandRandomize", genxAO.getRandomizeStand());
    GenxAOMgr::getInstance()->updateAOSet(genxAO);

}
void GenxFloaterAO::onClickNoStandInMouselook() {
    LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    LLUUID aoId = aosets_combo->getSelectedValue();
    GenxAOSet genxAO = GenxAOMgr::getInstance()->getAOSet(aoId);
    genxAO.setDisableStandMouselook(getChild<LLCheckBoxCtrl>("AONoStandsInMouselook")->get());
    gSavedSettings.setBOOL("AONoStandsInMouselook", genxAO.getDisableStandMouselook());
    GenxAOMgr::getInstance()->updateAOSet(genxAO);

}
void GenxFloaterAO::onClickCycleStand(bool next) const
{
	auto ao = AOSystem::getIfExists();
	if (!ao) return;
	auto stand = ao->cycleStand(next, false);
	ao->mAOStandTimer.reset();
	if (stand < 0) return;
	
}
void GenxFloaterAO::onComboBoxCommit(LLUICtrl* ctrl) const
{
    if (LLComboBox* box = (LLComboBox*)ctrl)
	{
		std::string stranim = box->getValue().asString();
		if (auto ao = AOSystem::getIfExists())
		{
			if (box == mCombos[STATE_AGENT_IDLE])
			{
				ao->stand_iterator = box->getCurrentIndex();
				llassert(ao->stand_iterator < ao->mAOStands.size());
				const auto& name = ao->mAOStands[ao->stand_iterator].anim_name;
				llassert(name == box->getValue().asStringRef());
				
				ao->updateStand();
				ao->mAOStandTimer.reset();
			}
			else
			{
				auto end = mCombos.end();
				auto it = std::find(mCombos.begin(), end, box);
				llassert(it != end);
				const auto state = std::distance(mCombos.begin(), it);
				if (auto aop = ao->mAOOverrides[state])
				{
					LLUUID getAssetIDByName(const std::string & name);
//					LL_INFOS() << "state " << state << " - " << aop->playing() << LL_ENDL;
					bool was_playing = aop->playing;
					if (was_playing) aop->play(false);
					aop->ao_id = getAssetIDByName(stranim);
					if (was_playing) aop->play();
				}
			}
		}
        LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
        LLUUID aoId = aosets_combo->getSelectedValue();
        GenxAOSet genxAO = GenxAOMgr::getInstance()->getAOSet(aoId);
        
        const std::string ctrlName = ctrl->getName();
        genxAO.setDefaultAnim(ctrlName,stranim);
        
        GenxAOMgr::getInstance()->updateAOSet(genxAO);
        
        if (mapCombosToControlNames.count(ctrlName)) {
            std::string controlName = mapCombosToControlNames.at(ctrlName);
		    gSavedPerAccountSettings.setString(controlName, stranim);
        }
	}
}
void GenxFloaterAO::onClickOpenCard() const
{
	LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    LLUUID aoId = aosets_combo->getSelectedValue();
    GenxAOSet genxAO = GenxAOMgr::getInstance()->getAOSet(aoId);
	if (genxAO.getID().notNull() && genxAO.getNoteCardID().notNull())
		if (LLViewerInventoryItem* item = gInventory.getItem(genxAO.getNoteCardID()))
			if (gAgent.allowOperation(PERM_COPY, item->getPermissions(), GP_OBJECT_MANIPULATE))
				if(!item->getAssetUUID().isNull())
					open_notecard(item, "Note: " + item->getName(), LLUUID::null, false);
}
void GenxFloaterAO::onClickNewAOSet()
{
    LLUUID newUUID = GenxAOMgr::getInstance()->newAOSet();
    refreshAOSets();
    LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    aosets_combo->setValue(newUUID);
    onSelectAOSet();
}
void GenxFloaterAO::refreshAOSets() {
    std::vector<GenxAOSet> aoSets = GenxAOMgr::getInstance()->getAoSets();
    LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    aosets_combo->clearRows();
    for (auto& key : aoSets) {
        aosets_combo->add(key.getName(),key.getID());
    }
}
void GenxFloaterAO::onSelectAOSet()
{
    LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    LLUUID id = aosets_combo->getSelectedValue();
    gSavedPerAccountSettings.setString("GenxAOUsed",id.asString());
    getChild<LLButton>("rename_ao_set")->setVisible(true);
    getChild<LLLineEditor>("new_ao_set_name")->setVisible(false);
    getChild<LLButton>("new_ao_set_name_ok")->setVisible(false);
    getChild<LLButton>("new_ao_set_name_cancel")->setVisible(false);
	
    GenxAOSet genxAO = GenxAOMgr::getInstance()->getAOSet(id);
    if (!genxAO.getID().isNull() && !genxAO.getNoteCardID().isNull())
    {
        getChild<GenxDropTarget>("ao_notecard")->setValue(genxAO.getNoteCardID());
        gSavedPerAccountSettings.setString("AOConfigNotecardID",genxAO.getNoteCardID().asString());
        getChild<LLButton>("viewcard")->setVisible(true);
        getChild<LLButton>("reload")->setVisible(true);
        getChild<LLButton>("newcard")->setVisible(false);
        
        AOSystem::instance().initSingleton();
        
    } else {
        getChild<GenxDropTarget>("ao_notecard")->setValue(LLUUID());
        getChild<LLButton>("viewcard")->setVisible(false);
        getChild<LLButton>("reload")->setVisible(false);
        getChild<LLButton>("newcard")->setVisible(true);
    }
    if (genxAO.getID().notNull()) {
        getChild<LLCheckBoxCtrl>("AOStandRandomize")->set(genxAO.getRandomizeStand());
        gSavedSettings.setBOOL("AOStandRandomize", genxAO.getRandomizeStand());

        getChild<LLCheckBoxCtrl>("AONoStandsInMouselook")->set(genxAO.getDisableStandMouselook());
        gSavedSettings.setBOOL("AONoStandsInMouselook", genxAO.getDisableStandMouselook());

        getChild<LLSpinCtrl>("standtime")->setValue(genxAO.getStandTime());
        gSavedSettings.setF32("AOStandInterval", genxAO.getStandTime());

        //default anims
        std::map<std::string, std::string> defaultAnims = genxAO.getDefaultAnims();
        
        for (auto const& defaultanim_iter : genxAO.getDefaultAnims() )
        {
            getChild<LLComboBox>(defaultanim_iter.first)->setValue(defaultanim_iter.second);
            if (mapCombosToControlNames.count(defaultanim_iter.first)){
        
                gSavedPerAccountSettings.setString(mapCombosToControlNames[defaultanim_iter.first],defaultanim_iter.second);
            }    
        }  
        AOSystem::instance().initSingleton();
        
    }
    
}
void GenxFloaterAO::onDeleteAOSet()
{
    LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    LLUUID id = aosets_combo->getSelectedValue();
    GenxAOSet genxAO = GenxAOMgr::getInstance()->getAOSet(id);
    if (!genxAO.getID().isNull())
    {
        GenxAOMgr::getInstance()->deleteAOSet(genxAO);
        refreshAOSets();
        aosets_combo->selectFirstItem();
        onSelectAOSet();
    }
	
}
void GenxFloaterAO::onClickReloadCard() const
{
	AOSystem::instance().initSingleton();
}
void GenxFloaterAO::onRenameAOSet()
{
    LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    LLUUID id = aosets_combo->getSelectedValue();
    GenxAOSet genxAO = GenxAOMgr::getInstance()->getAOSet(id);
    if (!genxAO.getID().isNull())
    {
        LLLineEditor *newNameField = getChild<LLLineEditor>("new_ao_set_name");
        newNameField->setValue(genxAO.getName());
        getChild<LLButton>("rename_ao_set")->setVisible(false);
        newNameField->setVisible(true);
        getChild<LLButton>("new_ao_set_name_ok")->setVisible(true);
        getChild<LLButton>("new_ao_set_name_cancel")->setVisible(true);
    }
	
}
void GenxFloaterAO::onRenameAOSetOK()
{
    LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    LLLineEditor *newNameField = getChild<LLLineEditor>("new_ao_set_name");
    LLUUID id = aosets_combo->getSelectedValue();
    GenxAOSet genxAO = GenxAOMgr::getInstance()->getAOSet(id);
    if (!genxAO.getID().isNull())
    {
        
        genxAO.setName(newNameField->getText());
        GenxAOMgr::getInstance()->updateAOSet(genxAO);
        refreshAOSets();
        aosets_combo->setValue(id);
        onSelectAOSet();
       
    }
	getChild<LLButton>("rename_ao_set")->setVisible(true);
    newNameField->setVisible(false);
    getChild<LLButton>("new_ao_set_name_ok")->setVisible(false);
    getChild<LLButton>("new_ao_set_name_cancel")->setVisible(false);
}
void GenxFloaterAO::onRenameAOSetCancel()
{
    
	getChild<LLButton>("rename_ao_set")->setVisible(true);
    getChild<LLLineEditor>("new_ao_set_name")->setVisible(false);
    getChild<LLButton>("new_ao_set_name_ok")->setVisible(false);
    getChild<LLButton>("new_ao_set_name_cancel")->setVisible(false);
}
void GenxFloaterAO::onClickNewCard() const
{
	// load the template file from app_settings/ao_template.ini then
	// create a new properly-formatted notecard in the user's inventory
	std::string ao_template = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "ao_template.ini");
	if (ao_template.empty())
	{
		ao_template = "#Can't find ao_template.ini in app_settings!";
		LL_WARNS() << ao_template << LL_ENDL;
		ao_template += "\n#ZHAO II Style Notecards are supported.";
	}

	create_inventory_item(gAgentID, gAgentSessionID,
						LLUUID::null, LLTransactionID::tnull, "New AO Notecard", 
						"Drop this notecard in your AO window to use", LLAssetType::AT_NOTECARD,
						LLInventoryType::IT_NOTECARD, NOT_WEARABLE, PERM_ALL, new GenxAONotecardCallback(ao_template));
}
void GenxFloaterAO::onInventoryItemDropped() {
    
    LLInventoryItem* item = getChild<GenxDropTarget>("ao_notecard")->getItem();
    
    LLComboBox *aosets_combo = getChild<LLComboBox>("genxaosettings");
    LLUUID id = aosets_combo->getSelectedValue();
    GenxAOSet genxAO = GenxAOMgr::getInstance()->getAOSet(id);
    if (!genxAO.getID().isNull())
    {
        genxAO.setNotecardID(item->getUUID());
        GenxAOMgr::getInstance()->updateAOSet(genxAO);
        onSelectAOSet();
    }
    
}