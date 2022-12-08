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
#include "lldroptarget.h"
#include "llwindow.h"
#include "llgroupactions.h"
#include "llmutelist.h"
#include "llmediactrl.h"
#include <boost/date_time.hpp>
#include "llvoavatar.h"
#include "llviewerwindow.h"
#include "llweb.h"
#include "llvector4a.h"
#include "llfloaterworldmap.h"
#include "llpreviewtexture.h"
//Genesis
#include "genxcontactset.h"
#include "llhttpclient.h"
#include "llviewertexturelist.h"

GenxFloaterAvatarInfo::floater_positions_t GenxFloaterAvatarInfo::floater_positions;
LLUUID GenxFloaterAvatarInfo::lastMoved;

BOOL is_agent_mappable(const LLUUID& agent_id);
void show_pick(const LLUUID& id, const std::string& name)
{
	// Try to show and focus existing preview
	if (id.isNull() || LLPreview::show(id)) return;
	// If there isn't one, make a new preview
	S32 left, top;
	gFloaterView->getNewFloaterPosition(&left, &top);
	LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
	auto preview = new LLPreviewTexture("preview texture", rect.translate(left - rect.mLeft, rect.mTop - top), name, id);
	preview->setFocus(true);
	gFloaterView->adjustToFitScreen(preview, false);
}
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
	bool self = (avatar_id==gAgent.getID());
	setAutoFocus(true);

	LLCallbackMap::map_t factory_map;
	
	LLUICtrlFactory::getInstance()->buildFloater(this, "genx_floater_profile.xml");
	setTitle(name);
    LLAvatarPropertiesProcessor::getInstance()->addObserver(mAvatarID, this);
    LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(mAvatarID);
	LLAvatarPropertiesProcessor::getInstance()->sendAvatarPicksRequest(mAvatarID);
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

    
	getChild<LLTextureCtrl>("img2ndLife")->setFallbackImageName("default_profile_picture.j2c");
	//getChild<LLTextureCtrl>("img2ndLife")->setOnTextureSelectedCallback(boost::bind(&GenxFloaterAvatarInfo::onTextureSelectionChanged, this, _1));
    getChild<LLTextureCtrl>("img2ndLife")->setEnabled(self);
    bool isFriend = LLAvatarActions::isFriend(mAvatarID);
    getChild<LLUICtrl>("btn_add_friend")->setCommitCallback(boost::bind(LLAvatarActions::requestFriendshipDialog, mAvatarID));
    getChild<LLUICtrl>("btn_add_friend")->setEnabled(!self && !isFriend);


    getChild<LLUICtrl>("btn_log")->setCommitCallback(boost::bind(genx_show_log_browser, mAvatarID, LFIDBearer::AVATAR));

    getChild<LLUICtrl>("btn_pay")->setCommitCallback(boost::bind(LLAvatarActions::pay,mAvatarID));
    getChild<LLUICtrl>("btn_instant_message")->setCommitCallback(boost::bind(LLAvatarActions::startIM,mAvatarID));

   
    manageMuteButtons();
    getChild<LLUICtrl>("btn_mute")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::toggleBlock,this));
	getChild<LLUICtrl>("btn_unmute")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::toggleBlock,this));

    getChild<LLUICtrl>("btn_invite_group")->setCommitCallback(boost::bind(static_cast<void(*)(const LLUUID&)>(LLAvatarActions::inviteToGroup), mAvatarID));
	
	getChild<LLUICtrl>("save_profile")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::saveProfile,this));

	LLDropTarget* drop_target = findChild<LLDropTarget>("drop_target_rect");
	drop_target->setEntityID(mAvatarID);

	getChild<LLUICtrl>("copy_flyout")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::onClickCopy, this, _2));

	getChild<LLButton>("upload_SL_pic")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::onClickUploadPhoto, this));
	getChild<LLButton>("change_SL_pic")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::onClickChangePhoto, this));
	
	childSetVisible("contact_set_label",!self);
	childSetVisible("avatar_contact_set",!self);
	childSetVisible("drop_target_rect",!self);
	childSetEnabled("avatar_partner",self);

	childSetVisible("upload_SL_pic",self);
	childSetVisible("change_SL_pic",self);
	childSetVisible("remove_SL_pic",self);
	childSetVisible("change_display_name",self);

	childSetEnabled("about",self);
	

	//Feed Tab
	LLMediaCtrl* webBrowser = getChild<LLMediaCtrl>("profile_html");
    
    webBrowser->setHomePageUrl("about:blank");

	LLAvatarNameCache::get(mAvatarID, boost::bind(&GenxFloaterAvatarInfo::onAvatarNameCache, this, _1, _2));
	

	//pick tab
	getChild<LLUICtrl>("pick_next_btn")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::pickNext,this));
	getChild<LLUICtrl>("pick_prev_btn")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::pickPrev,this));
	getChild<LLUICtrl>("avatar_picks")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::onSelectedPick, this));
	getChild<LLComboBox>("avatar_picks")->setValidateCallback(boost::bind(&GenxFloaterAvatarInfo::onPreSelectedPick, this));
	getChild<LLUICtrl>("pick_teleport_btn")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::onClickPickTeleport,this));
	getChild<LLUICtrl>("pick_map_btn")->setCommitCallback(boost::bind(&GenxFloaterAvatarInfo::onClicPickkMap,this));
	auto show_pic = [this] { show_pick(getChild<LLTextureCtrl>("pick_snapshot")->getImageAssetID(), getChild<LLLineEditor>("given_name_editor")->getText()); };
	getChild<LLUICtrl>("pick_eye_btn")->setCommitCallback(std::bind(show_pic));
	getChild<LLTextureCtrl>("pick_snapshot")->setEnabled(false);	
	
	//Real Life Tab
	
	getChild<LLTextureCtrl>("imgRL")->setFallbackImageName("default_profile_picture.j2c");
	childSetEnabled("imgRL", self);
	childSetEnabled("aboutRL", self);

	//Profile actions
	childSetEnabled("genx_profile_actions", !self);
	childSetVisible("genx_profile_actions", !self);
	childSetEnabled("genx_self_profile_actions", self);
	childSetVisible("genx_self_profile_actions", self);
}
void GenxFloaterAvatarInfo::onTextureSelectionChanged(LLInventoryItem* itemp)
{
	getChild<LLTextureCtrl>("img2ndLife")->setImageAssetID(itemp->getAssetUUID());
	
}
void GenxFloaterAvatarInfo::saveProfile()
{
	sendAvatarPropertiesUpdate();
	this->close();

	
}
void GenxFloaterAvatarInfo::sendAvatarPropertiesUpdate()
{
	LL_INFOS() << "Sending avatarinfo update" << LL_ENDL;
	LLAvatarData avatar_data;
	avatar_data.image_id = getChild<LLTextureCtrl>("img2ndLife")->getImageAssetID();
	avatar_data.fl_image_id = getChild<LLTextureCtrl>("imgRL")->getImageAssetID();
	avatar_data.about_text = this->childGetValue("about").asString();
	avatar_data.fl_about_text = childGetValue("aboutRL").asString();
	avatar_data.allow_publish = true;
	//avatar_data.profile_url = mPanelWeb->childGetText("url_edit");
	LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesUpdate(&avatar_data);

	
}
void GenxFloaterAvatarInfo::sl_filepicker_callback(AIFilePicker* picker)
{
	if (picker->hasFilename())
	{
		
		std::string filename = picker->getFilename();
		
		// generate a temp texture file for coroutine
		std::string temp_file = gDirUtilp->getTempFilename();
		U32 codec = LLImageBase::getCodecFromExtension(gDirUtilp->getExtension(filename));
		const S32 MAX_DIM = 256;
		if (!LLViewerTextureList::createUploadFile(filename, temp_file, codec, MAX_DIM))
		{
			//todo: image not supported notification
			LL_WARNS("AvatarProperties") << "Failed to upload profile image of type " << "sl_image_id" ", failed to open image" << LL_ENDL;
			return;
		}

		//upload the pic
		LLSD data = LLSD::emptyMap();
		data["profile-image-asset"] = "sl_image_id";
		LL_INFOS() << "posting to " << gAgent.getRegionCapability("UploadAgentProfileImage") << LL_ENDL;
		LLHTTPClient::post(gAgent.getRegionCapability("UploadAgentProfileImage"), data,new LLCoroResponder(
                    boost::bind(&GenxFloaterAvatarInfo::sl_http_upload_first_step,this,_1,temp_file)));
	  
	}
}
void GenxFloaterAvatarInfo::sl_http_upload_first_step(const LLCoroResponder& responder,std::string filename)
{
    const auto& status = responder.getStatus();
	
    if (!responder.isGoodStatus(status))
    {
        LL_WARNS() << "HTTP status First file upload step" << status << ": " << responder.getReason() <<
            ". First file upload step failed." << LL_ENDL;
    }
    else
    {
        {
                     
			LLSD result = responder.getContent();
			if (result.has("uploader")) {
				
				std::string uploader_cap = result["uploader"].asString();

				//get the file length
				S64 fileLength;
				U8 *data;
				{
					llifstream instream(filename.c_str(), std::iostream::binary | std::iostream::ate);
					if (!instream.is_open())
					{
						LL_WARNS("AvatarProperties") << "Failed to open file " << filename << LL_ENDL;
						
					}
					fileLength = instream.tellg();
					instream.seekg(0, std::ios::beg);

					std::vector<char> buffer(fileLength);

					if (instream.read(buffer.data(), fileLength)) {
						std::string str(buffer.begin(), buffer.end());

						std::vector<U8> vec(str.begin(), str.end());

						data = (uint8_t*) malloc(sizeof(uint8_t) * vec.size());
						copy(vec.begin(), vec.end(), data);

						
					}
				}
				AIHTTPHeaders headers;
				headers.addHeader("Content-Type","application/jp2");
				headers.addHeader("Content-Length", llformat("%d", fileLength));
				
				
				
				
				LLHTTPClient::postRaw(uploader_cap,data,fileLength,new LLCoroResponder(
                    boost::bind(&GenxFloaterAvatarInfo::sl_http_upload_second_step,this,_1,filename)),headers);
				
			} else {
				LL_WARNS() << "First upload step failed, I don't have an uploader CAP" << LL_ENDL;
				
			}
            
        }
    }
}
void GenxFloaterAvatarInfo::sl_http_upload_second_step(const LLCoroResponder& responder,std::string filename)
{
    const auto& status = responder.getStatus();
	
    if (!responder.isGoodStatus(status))
    {
        LL_WARNS() << "HTTP status First file upload step" << status << ": " << responder.getReason() <<
            ". First file upload step failed." << LL_ENDL;
    }
    else
    {
        
            
            
		LLSD result = responder.getContent();
		if (result["state"].asString() != "complete")
		{
			if (result.has("message"))
			{
				LL_WARNS("AvatarProperties") << "Failed to upload image, state " << result["state"] << " message: " << result["message"] << LL_ENDL;
			}
			else
			{
				LL_WARNS("AvatarProperties") << "Failed to upload image " << result << LL_ENDL;
			}
			
		} else {
			getChild<LLTextureCtrl>("img2ndLife")->setImageAssetID(result["new_asset"].asUUID());
		}
            
        
    }
}
void GenxFloaterAvatarInfo::onClickChangePhoto()
{
	getChild<LLTextureCtrl>("img2ndLife")->showPicker(false);
	
}
void GenxFloaterAvatarInfo::onClickUploadPhoto()
{
	AIFilePicker* picker = AIFilePicker::create();
	picker->open(FFLOAD_IMAGE, "", "images");
	
	picker->run(boost::bind(&GenxFloaterAvatarInfo::sl_filepicker_callback, this, picker));
}
void GenxFloaterAvatarInfo::onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
{
	//feed tab
	
	std::string username = av_name.getAccountName();
	if (username.empty())
	{
		username = LLCacheName::buildUsername(av_name.getDisplayName());
	}
	else
	{
		LLStringUtil::replaceChar(username, ' ', '.');
	}

	std::string url = gSavedSettings.getString("WebProfileURL") + "[FEED_ONLY]";
	LLSD subs;
	subs["AGENT_NAME"] = username;
	subs["FEED_ONLY"] = "/?feed_only=true";
	url = LLWeb::expandURLSubstitutions(url, subs);
	LLStringUtil::toLower(url);
	LLMediaCtrl* webBrowser = getChild<LLMediaCtrl>("profile_html");
	
	webBrowser->navigateTo(url);
	
}
void GenxFloaterAvatarInfo::onClickPickTeleport()
{
	LLComboBox* avatar_picks = getChild<LLComboBox>("avatar_picks");
	LLUUID pickId = avatar_picks->getCurrentID();
	LLSD pickData = mPicks[pickId];
	LLVector3d posGlobal = LLVector3d(pickData.get(5));
    if (!posGlobal.isExactlyZero())
    {
        gAgent.teleportViaLocation(posGlobal);
        gFloaterWorldMap->trackLocation(posGlobal);
    }
}
void GenxFloaterAvatarInfo::onClicPickkMap()
{
	LLComboBox* avatar_picks = getChild<LLComboBox>("avatar_picks");
	LLUUID pickId = avatar_picks->getCurrentID();
	LLSD pickData = mPicks[pickId];
	LLVector3d posGlobal = LLVector3d(pickData.get(5));
    if (!posGlobal.isExactlyZero())
    {
        gFloaterWorldMap->trackLocation(posGlobal);
		LLFloaterWorldMap::show(true);
    }
}
void GenxFloaterAvatarInfo::onClickCopy(const LLSD& val)
{
	if (val.asInteger() == 0)
	{
		
		gViewerWindow->getWindow()->copyTextToClipboard(utf8str_to_wstring(mAvatarID.asString()));
	}
	else
	{
		void copy_profile_uri(const LLUUID& id, const LFIDBearer::Type& type = LFIDBearer::AVATAR);
		copy_profile_uri(mAvatarID);
	}
}
void GenxFloaterAvatarInfo::toggleBlock() 
{
    LLAvatarActions::toggleBlock(mAvatarID);
    manageMuteButtons();
}
void GenxFloaterAvatarInfo::pickNext() 
{
    LLComboBox* avatar_picks = getChild<LLComboBox>("avatar_picks");
	int selected_index = avatar_picks->getCurrentIndex();
	if (selected_index < avatar_picks->getItemCount()-1) {
		selected_index++;
		avatar_picks->setCurrentByIndex(selected_index);
		//onSelectedPick();
	}
}
void GenxFloaterAvatarInfo::pickPrev() 
{
    LLComboBox* avatar_picks = getChild<LLComboBox>("avatar_picks");
	int selected_index = avatar_picks->getCurrentIndex();
	if (selected_index >0) {
		selected_index--;
		avatar_picks->setCurrentByIndex(selected_index);
		//onSelectedPick();
	}
}
void GenxFloaterAvatarInfo::onSelectedPick() 
{
    LLComboBox* avatar_picks = getChild<LLComboBox>("avatar_picks");
	int selected_index = avatar_picks->getCurrentIndex();
	//enable or disable the prev and next buttons
	getChild<LLUICtrl>("pick_next_btn")->setEnabled(true);
	getChild<LLUICtrl>("pick_prev_btn")->setEnabled(true);
	if (avatar_picks->getItemCount() == 0 || selected_index == avatar_picks->getItemCount()-1) {
		getChild<LLUICtrl>("pick_next_btn")->setEnabled(false);
	}
	if (avatar_picks->getItemCount() == 0 || selected_index == 0) {
		getChild<LLUICtrl>("pick_prev_btn")->setEnabled(false);
	}
	LLUUID pickId=avatar_picks->getSelectedValue();
	displayPick(pickId);
}
void GenxFloaterAvatarInfo::displayPick(LLUUID pickId) 
{
	LLSD pickData = mPicks[pickId];
	getChild<LLTextureCtrl>("pick_snapshot")->setImageAssetID(pickData.get(2));
	getChild<LLLineEditor>("given_name_editor")->setText(pickData.get(1).asString());
	getChild<LLTextEditor>("desc_editor")->setText(pickData.get(3).asString());
	getChild<LLLineEditor>("location_editor")->setText(pickData.get(4).asString());
}
bool GenxFloaterAvatarInfo::onPreSelectedPick() 
{
	LLComboBox* avatar_picks = getChild<LLComboBox>("avatar_picks");
	LLUUID pickId=avatar_picks->getSelectedValue();
	displayPick(pickId);
	return true;

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
					born_on << birthday << " (" << today - birthday << ')';
					
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

			//Real Life
			getChild<LLTextEditor>("aboutRL")->setText(pAvatarData->fl_about_text, false);
			getChild<LLTextureCtrl>("imgRL")->setImageAssetID(pAvatarData->fl_image_id);
			
			
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
		else if (type == APT_PICKS) 
		{
			LLAvatarPicks* picks = static_cast<LLAvatarPicks*>(data);
			if (picks && mAvatarID == picks->target_id)
			{
				for (LLAvatarPicks::picks_list_t::iterator it = picks->picks_list.begin();
					it != picks->picks_list.end(); ++it)
				{
					
					mPicks[it->first]=LLSD();
				}
			}
			loadPicks();
		}
	}
	else if (type == APT_PICK_INFO)
	{
		
		LLPickData* pick_info = static_cast<LLPickData*>(data);
		if (pick_info->creator_id == mAvatarID) {
			LLSD pick = LLSD();
			pick.insert(0,pick_info->pick_id);
			pick.insert(1,pick_info->name);
			pick.insert(2,pick_info->snapshot_id);
			pick.insert(3,pick_info->desc);
			std::string location_text = pick_info->user_name + ", ";
			if (!pick_info->original_name.empty())
			{
				location_text += pick_info->original_name + ", ";
			}
			location_text += pick_info->sim_name + " ";

			S32 region_x = ll_round((F32)pick_info->pos_global.mdV[VX]) % REGION_WIDTH_UNITS;
			S32 region_y = ll_round((F32)pick_info->pos_global.mdV[VY]) % REGION_WIDTH_UNITS;
			S32 region_z = ll_round((F32)pick_info->pos_global.mdV[VZ]);
			location_text.append(llformat("(%d, %d, %d)", region_x, region_y, region_z));

			pick.insert(4,location_text);
			pick.insert(5,pick_info->pos_global.getValue());
			mPicks[pick_info->pick_id] = pick;
			
		} 
		loadPicks();
		
    }
}
void GenxFloaterAvatarInfo::loadPicks()
{
    for (auto& pick : mPicks) {
		if (pick.second.size()==0) {
			
			LLAvatarPropertiesProcessor::instance().sendPickInfoRequest(mAvatarID,pick.first);
			return;
		}
	}
	//no more picks to load, we can fill the picks combobox
	LLScrollListCtrl* picks_list = getChild<LLScrollListCtrl>("picks");
	
	//load contact set values
	LLComboBox* avatar_picks = getChild<LLComboBox>("avatar_picks");
	
	if (avatar_picks) {
		avatar_picks->removeall();
		
		
		for (auto& pick : mPicks) {
			avatar_picks->add(pick.second.get(1).asString(),pick.first);
		}
		avatar_picks->sortByName();
		

	}
	if (avatar_picks->getItemCount()>0) {
		avatar_picks->setCurrentByIndex(0);
		displayPick(avatar_picks->getCurrentID());
		getChild<LLUICtrl>("picks_info")->setVisible(false);
	} else {
		getChild<LLUICtrl>("pick_prev_btn")->setVisible(false);
		getChild<LLUICtrl>("pick_next_btn")->setVisible(false);
		getChild<LLUICtrl>("avatar_picks")->setVisible(false);
		getChild<LLUICtrl>("pick_snapshot")->setVisible(false);
		getChild<LLUICtrl>("pick_eye_btn")->setVisible(false);
		getChild<LLUICtrl>("given_name_editor")->setVisible(false);
		getChild<LLUICtrl>("desc_editor")->setVisible(false);
		getChild<LLUICtrl>("location_editor")->setVisible(false);
		getChild<LLUICtrl>("pick_teleport_btn")->setVisible(false);
		getChild<LLUICtrl>("pick_map_btn")->setVisible(false);

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