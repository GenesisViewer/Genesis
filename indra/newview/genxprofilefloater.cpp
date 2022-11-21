#include "llviewerprecompiledheaders.h"

#include "llagent.h"
#include "genxprofilefloater.h"
#include "lluictrlfactory.h"
#include "llavatarpropertiesprocessor.h"
#include "llavatarnamecache.h"
#include "llavataractions.h"
#include "lltexturectrl.h"
#include "llnameeditor.h"
#include "lltexteditor.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llcombobox.h"
#include "llgroupactions.h"
#include "llmutelist.h"
#include <boost/date_time.hpp>
#include "llvoavatar.h"
//Genesis
#include "genxcontactset.h"


GenxFloaterAvatarInfo::floater_positions_t GenxFloaterAvatarInfo::floater_positions;
LLUUID GenxFloaterAvatarInfo::lastMoved;

BOOL is_agent_mappable(const LLUUID& agent_id);

void genx_show_log_browser(const LLUUID& id, const LFIDBearer::Type& type)
{
	void show_log_browser(const std::string& name, const LLUUID& id);
	std::string name;
	if (type == LFIDBearer::AVATAR)
	{
		LLAvatarName av_name;
		LLAvatarNameCache::get(id, &av_name);
		name = av_name.getLegacyName();
	}
	else // GROUP
	{
		gCacheName->getGroupName(id, name);
	}
	show_log_browser(name, id);
}

GenxFloaterAvatarInfo::GenxFloaterAvatarInfo(const std::string& name, const LLUUID &avatar_id)
:	LLFloater(name), LLInstanceTracker<GenxFloaterAvatarInfo, LLUUID>(avatar_id),
	mAvatarID(avatar_id)
{
	setAutoFocus(true);

	LLCallbackMap::map_t factory_map;
	
	LLUICtrlFactory::getInstance()->buildFloater(this, "genx_floater_profile.xml");
	setTitle(name);
    LLAvatarPropertiesProcessor::getInstance()->addObserver(mAvatarID, this);
    LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(mAvatarID);
    getChild<LLNameEditor>("dnname")->setNameID(avatar_id, LFIDBearer::AVATAR);
    getChildView("avatar_key")->setValue(avatar_id);
    
    //load contact set values
	LLComboBox* contact_set = getChild<LLComboBox>("avatar_contact_set");
	
	if (contact_set) {
		contact_set->removeall();
		contact_set->add(" "," ");
		std::map<std::string, ContactSet> contact_sets = GenxContactSetMgr::instance().getContactSets();
		for (auto& key : contact_sets) {
			contact_set->add(key.second.getName(),key.first);
		}
		contact_sets.clear();

	}
	ContactSet contactSet = GenxContactSetMgr::instance().getAvatarContactSet(mAvatarID.asString());
	std::string avatar_contact_set = contactSet.getId();
	contact_set->setValue(avatar_contact_set);

	contact_set->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::onSelectedContactSet, this));

    bool godlike(gAgent.isGodlike());
    getChild<LLUICtrl>("find_on_map")->setCommitCallback(boost::bind(LLAvatarActions::showOnMap, mAvatarID));
    getChild<LLUICtrl>("find_on_map")->setEnabled(godlike || is_agent_mappable(mAvatarID));

    getChild<LLUICtrl>("btn_teleport")->setCommitCallback(boost::bind(static_cast<void(*)(const LLUUID&)>(LLAvatarActions::offerTeleport), mAvatarID));

    LLTextureCtrl* ctrl = getChild<LLTextureCtrl>("img2ndLife");
	ctrl->setFallbackImageName("default_profile_picture.j2c");

    bool own_avatar(mAvatarID == gAgentID);
    bool isFriend = LLAvatarActions::isFriend(mAvatarID);
    getChild<LLUICtrl>("btn_add_friend")->setCommitCallback(boost::bind(LLAvatarActions::requestFriendshipDialog, mAvatarID));
    getChild<LLUICtrl>("btn_add_friend")->setEnabled(!own_avatar && !isFriend);


    getChild<LLUICtrl>("btn_log")->setCommitCallback(boost::bind(genx_show_log_browser, mAvatarID, LFIDBearer::AVATAR));

    getChild<LLUICtrl>("btn_pay")->setCommitCallback(boost::bind(LLAvatarActions::pay,mAvatarID));
    getChild<LLUICtrl>("btn_instant_message")->setCommitCallback(boost::bind(LLAvatarActions::startIM,mAvatarID));

   
    manageMuteButtons();
    getChild<LLUICtrl>("btn_mute")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::toggleBlock,this));
	getChild<LLUICtrl>("btn_unmute")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::toggleBlock,this));

    getChild<LLUICtrl>("btn_invite_group")->setCommitCallback(boost::bind(static_cast<void(*)(const LLUUID&)>(LLAvatarActions::inviteToGroup), mAvatarID));
	
	
}
void GenxFloaterAvatarInfo::toggleBlock() 
{
    LLAvatarActions::toggleBlock(mAvatarID);
    manageMuteButtons();
}
void GenxFloaterAvatarInfo::manageMuteButtons() 
{
     bool isMuted = LLMuteList::instance().isMuted(mAvatarID);
     getChild<LLUICtrl>("btn_mute")->setVisible(!isMuted);
     getChild<LLUICtrl>("btn_unmute")->setVisible(isMuted);
}
void GenxFloaterAvatarInfo::onSelectedContactSet()
{
	LLComboBox* contact_set = getChild<LLComboBox>("avatar_contact_set");
	std::string selected_contact_set = contact_set->getSelectedValue();
    std::string avatarId = mAvatarID.asString();
    if (selected_contact_set == " ") {
        GenxContactSetMgr::instance().resetAvatarContactSet(avatarId);
    } else {
        GenxContactSetMgr::instance().updateAvatarContactSet(avatarId,selected_contact_set);
    }
    //reset the client tag manager to update all avatars tag
    SHClientTagMgr::getInstance()->resetAvatarTags();
	
	
}
void GenxFloaterAvatarInfo::handleReshape(const LLRect& new_rect, bool by_user) {

	if (by_user && !isMinimized()) {
		GenxFloaterAvatarInfo::floater_positions[this->mAvatarID]=new_rect;
		GenxFloaterAvatarInfo::lastMoved=this->mAvatarID;
		gSavedSettings.setRect("FloaterProfileRect",new_rect);
	}
	LLFloater::handleReshape(new_rect, by_user);
}
void GenxFloaterAvatarInfo::setOpenedPosition() {
	if (GenxFloaterAvatarInfo::lastMoved.isNull()) {
		LLRect lastfloaterProfilePosition = gSavedSettings.getRect("FloaterProfileRect");
		if (lastfloaterProfilePosition.mLeft==0 && lastfloaterProfilePosition.mTop==0) {
			this->center();
		}
		else {
			this->setRect(lastfloaterProfilePosition);
		}
		GenxFloaterAvatarInfo::floater_positions[this->mAvatarID]=this->getRect();
		GenxFloaterAvatarInfo::lastMoved=this->mAvatarID;
	} else {
		
		//LLRect lastFloaterPosition = GenxFloaterAvatarInfo::floater_positions[this->mAvatarID.asString()];
		floater_positions_t::iterator it = floater_positions.find(this->mAvatarID);
		if(it == floater_positions.end())
		{
			
			floater_positions_t::iterator lasFloaterMovedPosition = floater_positions.find(GenxFloaterAvatarInfo::lastMoved);
			this->setRect(lasFloaterMovedPosition->second);
			GenxFloaterAvatarInfo::floater_positions[this->mAvatarID]=this->getRect();
			GenxFloaterAvatarInfo::lastMoved=this->mAvatarID;
			
		} else {
			
			this->setRect(it->second);
		}
		
	}

	BOOL overlapse=TRUE;
	while (overlapse) {
		//if I am on an existing floater
		overlapse=FALSE;
		for(floater_positions_t::iterator it = floater_positions.begin(); it != floater_positions.end();++it )
		{
			if((*it).second == getRect() && (*it).first != this->mAvatarID)
			{
				
				this->translate(20,-20);
				GenxFloaterAvatarInfo::floater_positions[this->mAvatarID]=this->getRect();
				GenxFloaterAvatarInfo::lastMoved=this->mAvatarID;
				
				overlapse=TRUE;
			}
			
			
			
		}
		
	}
	
}
// virtual
void GenxFloaterAvatarInfo::processProperties(void* data, EAvatarProcessorType type)
{
    const LLAvatarData* pAvatarData = static_cast<const LLAvatarData*>( data );
    if (pAvatarData && (mAvatarID == pAvatarData->avatar_id) && (pAvatarData->avatar_id != LLUUID::null))  
    {
        if(type == APT_PROPERTIES)
	    {
            getChild<LLTextureCtrl>("img2ndLife")->setImageAssetID(pAvatarData->image_id);
            setOnlineStatus(pAvatarData->flags & AVATAR_ONLINE ? true : false);

            LLStringUtil::format_map_t args;
            // Show avatar age in days.
			{
				using namespace boost::gregorian;
				int year, month, day;
				const auto& born = pAvatarData->born_on;
				if (!born.empty() && sscanf(born.c_str(),"%d/%d/%d", &month, &day, &year) == 3 // Make sure input is valid
				&& month > 0 && month <= 12 && day > 0 && day <= 31 && year >= 1400) // Don't use numbers that gregorian will choke on
				{
					date birthday(year, month, day), today(day_clock::local_day());
					std::ostringstream born_on;
					const std::locale date_fmt(std::locale::classic(), new date_facet(gSavedSettings.getString("ShortDateFormat").data()));
					born_on.imbue(date_fmt);
					born_on << birthday;
					
                    args["[SLBirthday]"] = born_on.str();
				}
				else args["[SLBirthday]"] = born;
			}

            
            args["[ACCTTYPE]"] = LLAvatarPropertiesProcessor::accountType(pAvatarData);
            args["[PAYMENTINFO]"] = LLAvatarPropertiesProcessor::paymentInfo(pAvatarData);
            {
                const auto account_info = getString("CaptionTextAcctInfo", args);
                auto acct = getChild<LLUICtrl>("acct");
                LL_INFOS() << account_info <<LL_ENDL;
                acct->setValue(account_info);
                acct->setToolTip(account_info);
            }

			getChildView("avatar_partner")->setValue(pAvatarData->partner_id);

            getChild<LLTextEditor>("about")->setText(pAvatarData->about_text, false);
        }
        else if (type == APT_GROUPS)
        {
            const LLAvatarGroups* pAvatarGroups = static_cast<const LLAvatarGroups*>(data);
            if (pAvatarGroups && pAvatarGroups->avatar_id == mAvatarID && pAvatarGroups->avatar_id.notNull())
            {
                LLScrollListCtrl* group_list = getChild<LLScrollListCtrl>("groups");
                if (!pAvatarGroups->group_list.size())
                {
                    group_list->setCommentText(getString("None"));
                }

                for(LLAvatarGroups::group_list_t::const_iterator it = pAvatarGroups->group_list.begin();
                    it != pAvatarGroups->group_list.end(); ++it)
                {
                    // Is this really necessary?  Remove existing entry if it exists.
                    // TODO: clear the whole list when a request for data is made
                    S32 index = group_list->getItemIndex(it->group_id);
                    if (index >= 0)
                        group_list->deleteSingleItem(index);

                    LLScrollListItem::Params row;
                    row.value(it->group_id);

                    std::string font_style("NORMAL"); // Set normal color if not found or if group is visible in profile
                    if (pAvatarGroups->avatar_id == pAvatarGroups->agent_id) // own avatar
                        for (std::vector<LLGroupData>::iterator i = gAgent.mGroups.begin(); i != gAgent.mGroups.end(); ++i) // Search for this group in the agent's groups list
                            if (i->mID == it->group_id)
                            {
                                if (i->mListInProfile)
                                    font_style = "BOLD";
                                break;
                            }

                    if (it->group_id == gAgent.getGroupID())
                        font_style.append("|ITALIC");
                    row.columns.add(LLScrollListCell::Params().value(it->group_id.notNull() ? it->group_name : "").font("SANSSERIF_SMALL").font_style(font_style));
                    group_list->addRow(row,ADD_SORTED);
                }
            }
        }
    }
}
void GenxFloaterAvatarInfo::setOnlineStatus(bool online_status)
{
    this->getChild<LLButton>("avatar_online_info")->setVisible(online_status);
    this->getChild<LLButton>("avatar_offline_info")->setVisible(!online_status);
}
// virtual
GenxFloaterAvatarInfo::~GenxFloaterAvatarInfo()
{
    LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarID, this);
}