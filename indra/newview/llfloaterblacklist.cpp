// <edit>
#include "llviewerprecompiledheaders.h"

#include "llfloaterblacklist.h"

#include "llaudioengine.h"
#include "llcombobox.h"
#include "lldate.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llsdserialize.h"
#include "lluictrlfactory.h"
#include "llvfs.h"
#include "llwindow.h"

#include "llagent.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "statemachine/aifilepicker.h"

LLFloaterBlacklist* LLFloaterBlacklist::sInstance;

uuid_vec_t LLFloaterBlacklist::blacklist_textures;
uuid_vec_t LLFloaterBlacklist::blacklist_objects;

boost::unordered_map<LLUUID,LLSD> LLFloaterBlacklist::blacklist_entries;

LLFloaterBlacklist::LLFloaterBlacklist()
:	LLFloater()
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_blacklist.xml");
}
LLFloaterBlacklist::~LLFloaterBlacklist()
{
	if(sInstance == this)
		sInstance = NULL;
}
// static
void LLFloaterBlacklist::show()
{
	if(sInstance)
		sInstance->open();
	else
	{
		sInstance = new LLFloaterBlacklist();
		sInstance->open();
	}
}
// static
void LLFloaterBlacklist::toggle()
{
	if (sInstance && sInstance->getVisible())
	{
		delete sInstance;
	}
	else
	{
		show();
	}
}
// static
BOOL LLFloaterBlacklist::visible()
{
	if (sInstance && sInstance->getVisible())
		return TRUE;
	return FALSE;
}
BOOL LLFloaterBlacklist::postBuild()
{
	childSetAction("add_btn", onClickAdd, this);
	childSetAction("clear_btn", onClickClear, this);
	childSetAction("copy_uuid_btn", onClickCopyUUID, this);
	childSetAction("remove_btn", onClickRemove, this);
	childSetAction("save_btn", onClickSave, this);
	childSetAction("load_btn", onClickLoad, this);
	//childSetAction("rerender_btn", onClickRerender, this);
	childSetVisible("copy_uuid_btn",false);
	LLComboBox* box = getChild<LLComboBox>("asset_combo");
	box->add("Texture",LLSD(0));
	box->add("Sound",LLSD(1));
	box->add("Animation",LLSD(20));
	box->add("Object",LLSD(6));
	refresh();
	return TRUE;
}
void LLFloaterBlacklist::refresh()
{

	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("file_list");
	list->clearRows();
	for(auto iter = blacklist_entries.begin(); iter != blacklist_entries.end(); ++iter)
	{
		if(iter->first.isNull()) continue;
		LLSD element;
		std::string agent;
		gCacheName->getFullName(LLUUID(iter->second["entry_agent"].asString()), agent);

		element["id"] = iter->first.asString();
		{
			LLSD& column = element["columns"][0];
			column["column"] = "asset_id";
			column["value"] = iter->first.asString();
		}
		{
			LLSD& column = element["columns"][1];
			column["column"] = "entry_name";
			column["value"] = iter->second["entry_name"].asString();
		}
		{
			LLSD& column = element["columns"][2];
			column["column"] = "entry_type";
			column["value"] = std::string(LLAssetType::lookupHumanReadable( (LLAssetType::EType)iter->second["entry_type"].asInteger() ));
		}
		{
			LLSD& column = element["columns"][3];
			column["column"] = "entry_agent";
			column["value"] = agent;
		}
		{
			LLSD& column = element["columns"][4];
			column["column"] = "entry_date";
			column["value"] = iter->second["entry_date"].asString();
		}
		{
			LLSD& column = element["columns"][5];
			column["column"] = "temporary";
			if (iter->second.has("temporary")) {
				column["value"] = iter->second["temporary"]?"Yes":"No";
			} else {
				column["value"] ="No";
			}
			
		}
		list->addElement(element, ADD_BOTTOM);
	}
}

// static
void LLFloaterBlacklist::onClickAdd(void* user_data)
{
	LLFloaterBlacklist* floaterp = (LLFloaterBlacklist*)user_data;
	if(!floaterp) return;
	LLUUID add_id(floaterp->childGetValue("id_edit").asString());
	if(add_id.isNull()) return;
	std::string name(floaterp->childGetValue("name_edit").asString());
	if(name.size() == 0) return;
	LLComboBox* mTypeComboBox = floaterp->getChild<LLComboBox>("asset_combo");
	LLSD indata;
	indata["entry_type"] = (LLAssetType::EType)mTypeComboBox->getValue().asInteger();
	indata["entry_name"] = name;
	indata["entry_agent"] = gAgent.getID().asString();

	addEntry(add_id,indata);
}
//static
void LLFloaterBlacklist::removeTemporaryAssets()
{
	
	for(auto itr = blacklist_entries.begin(); itr != blacklist_entries.end();)
	{
		if (itr->second.has("temporary") && itr->second["temporary"])
			itr = blacklist_entries.erase(itr);
		else
			++itr;
	}
	updateBlacklists();
}
//static
void LLFloaterBlacklist::addEntry(LLUUID key, LLSD data)
{
	if(key.notNull())
	{
		if(!data.has("entry_type"))
		{
			LL_WARNS("FloaterBlacklistAdd") << "addEntry called with no entry type, specify LLAssetType::Etype" << LL_ENDL;
		}
		else if(!data.has("entry_name"))
		{
			LL_WARNS("FloaterBlacklistAdd") << "addEntry called with no entry name, specify the name that should appear in the listing for this entry." << LL_ENDL;
		}
		else
		{
			if(!data.has("entry_date"))
			{
				LLDate* curdate = new LLDate(time_corrected());
				std::string input_date = curdate->asString();
				input_date.replace(input_date.find("T"),1," ");
				input_date.resize(input_date.size() - 1);
				data["entry_date"] = input_date;
			}
			if(data["entry_type"].asString() == "1")
			{
				//remove sounds
				LLUUID sound_id=LLUUID(key);
				gVFS->removeFile(sound_id,LLAssetType::AT_SOUND);
				std::string wav_path= gDirUtilp->getExpandedFilename(LL_PATH_CACHE,sound_id.asString()) + ".dsf";
				if(LLAPRFile::isExist(wav_path, LL_APR_RPB))
					LLAPRFile::remove(wav_path);
				if(gAudiop)
					gAudiop->removeAudioData(sound_id);
			}
			blacklist_entries.insert(std::pair<LLUUID,LLSD>(key,data));
			updateBlacklists();
		}
	}
	else
	{
		LL_WARNS("FloaterBlacklistAdd") << "addEntry called with a null entry key, please specify LLUUID of asset." << LL_ENDL;
	}
}

// static
void LLFloaterBlacklist::onClickClear(void* user_data)
{
	blacklist_entries.clear();
	updateBlacklists();
}
// static
void LLFloaterBlacklist::onClickCopyUUID(void* user_data)
{
	LLFloaterBlacklist* floaterp = (LLFloaterBlacklist*)user_data;
	LLScrollListCtrl* list = floaterp->getChild<LLScrollListCtrl>("file_list");
	gViewerWindow->getWindow()->copyTextToClipboard(utf8str_to_wstring(list->getFirstSelected()->getColumn(0)->getValue().asString()));
}
// static
void LLFloaterBlacklist::onClickRemove(void* user_data)
{
	LLFloaterBlacklist* floaterp = (LLFloaterBlacklist*)user_data;
	LLScrollListCtrl* list = floaterp->getChild<LLScrollListCtrl>("file_list");
	if(list->getFirstSelected())
	{
		uuid_vec_t selectedIDs = list->getSelectedIDs();
		uuid_vec_t::const_iterator iterator;
		for(iterator  = selectedIDs.begin();
		    iterator != selectedIDs.end();
		    ++iterator)
		{
			LLUUID selectedID = *iterator;
			blacklist_entries.erase(selectedID);
		}
		list->deleteSelectedItems();
		updateBlacklists();
	}
}
// static
void LLFloaterBlacklist::loadFromSave()
{
	std::string file_name = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "blacklist_sg1.xml");
	llifstream xml_file(file_name);
	if(!xml_file.is_open()) return;
	LLSD data;
	if(LLSDSerialize::fromXML(data, xml_file) >= 1)
	{
		for(LLSD::map_iterator iter = data.beginMap(); iter != data.endMap(); ++iter)
		{
			//Mely : when an object is setted as temporary blacklisted, don't load on startup.
			if (iter->second.has("temporary") && iter->second["temporary"])
				continue;
			else	
				blacklist_entries.insert(std::pair<LLUUID,LLSD>(LLUUID(iter->first),iter->second));
		}
		updateBlacklists();
	}
	xml_file.close();
}

//static
void LLFloaterBlacklist::updateBlacklists()
{
	if(gAssetStorage)
	{
		blacklist_textures.clear();
		blacklist_objects.clear();
		gAssetStorage->mBlackListedAsset.clear();
		for(auto iter = blacklist_entries.begin(); iter != blacklist_entries.end(); ++iter)
		{
			if(blacklist_entries[iter->first]["entry_type"].asString() == "0")
			{
				blacklist_textures.push_back(LLUUID(iter->first));
			}
			else
			{
				gAssetStorage->mBlackListedAsset.push_back(LLUUID(iter->first));
			}

			if(blacklist_entries[iter->first]["entry_type"].asString() == "6")
			{
				blacklist_objects.push_back(LLUUID(iter->first));
			}

		}
		saveToDisk();
		LLFloaterBlacklist* instance = LLFloaterBlacklist::getInstance();
		if(instance)
			instance->refresh();
	}
}

//static
void LLFloaterBlacklist::saveToDisk()
{
	std::string file_name = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "blacklist_sg1.xml");
	llofstream export_file(file_name);
	LLSD data;
	for(auto iter = blacklist_entries.begin(); iter != blacklist_entries.end(); ++iter)
	{
		data[iter->first.asString()] = iter->second;
	}
	LLSDSerialize::toPrettyXML(data, export_file);
	export_file.close();
}

//static
void LLFloaterBlacklist::onClickSave(void* user_data)
{
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open("untitled.blacklist", FFSAVE_BLACKLIST);
	filepicker->run(boost::bind(&LLFloaterBlacklist::onClickSave_continued, filepicker));
}

//static
void LLFloaterBlacklist::onClickSave_continued(AIFilePicker* filepicker)
{
	if (filepicker->hasFilename())
	{
		std::string file_name = filepicker->getFilename();
		llofstream export_file(file_name);
		LLSD data;
		for(auto iter = blacklist_entries.begin(); iter != blacklist_entries.end(); ++iter)
		{
			data[iter->first.asString()] = iter->second;
		}
		LLSDSerialize::toPrettyXML(data, export_file);
		export_file.close();
	}
}

//static
void LLFloaterBlacklist::onClickLoad(void* user_data)
{
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(FFLOAD_BLACKLIST);
	filepicker->run(boost::bind(&LLFloaterBlacklist::onClickLoad_continued, filepicker));
}

void LLFloaterBlacklist::onClickLoad_continued(AIFilePicker* filepicker)
{
	if (filepicker->hasFilename())
	{
		std::string file_name = filepicker->getFilename();
		llifstream xml_file(file_name);
		if(!xml_file.is_open()) return;
		LLSD data;
		if(LLSDSerialize::fromXML(data, xml_file) >= 1)
		{
			for(LLSD::map_iterator iter = data.beginMap(); iter != data.endMap(); ++iter)
			{
				blacklist_entries.insert(std::pair<LLUUID,LLSD>(LLUUID(iter->first),iter->second));
			}
			updateBlacklists();
		}
		xml_file.close();
	}
}


void LLFloaterBlacklist::onClickRerender(void* user_data)
{
	for(auto itr = blacklist_entries.begin(); itr != blacklist_entries.end();)
	{
		if (blacklist_entries[itr->first]["entry_type"].asString() == "6")
			itr = blacklist_entries.erase(itr);
		else
			++itr;
	}
	saveToDisk();
	LLFloaterBlacklist* instance = LLFloaterBlacklist::getInstance();
	if(instance)
	{
		instance->refresh();
	}

}

// </edit>
