/** 
 * @file llpanelskins.cpp
 * @brief General preferences panel in preferences floater
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llpanelskins.h"

// linden library includes
#include "llradiogroup.h"
#include "llbutton.h"
#include "lluictrlfactory.h"
#include "lldiriterator.h"
#include "llnotificationsutil.h"

// project includes
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llcombobox.h"
#include "llsdserialize.h"


LLPanelSkins::LLPanelSkins()
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_skins.xml");
}

LLPanelSkins::~LLPanelSkins()
{
}

BOOL LLPanelSkins::postBuild()
{
	mSkin = gSavedSettings.getString("SkinCurrent");
	oldSkin=mSkin;
	getChild<LLComboBox>("custom_skin_combo")->setCommitCallback(boost::bind(&LLPanelSkins::onComboBoxCommit, this, _2));
	refresh();
	return TRUE;
}

void LLPanelSkins::refresh()
{
	if(mSkin=="")
	{
		oldSkin=mSkin="default";
		gSavedSettings.setString("SkinCurrent",mSkin);
	}
	LLComboBox* comboBox = getChild<LLComboBox>("custom_skin_combo");

	if(comboBox != NULL) 
	{
		comboBox->removeall();
		datas.clear();
		//comboBox->add("===OFF===");
		std::string path_name(gDirUtilp->getSkinBaseDir()+gDirUtilp->getDirDelimiter());
		LL_INFOS() << "Reading skin listing from " << path_name << LL_ENDL;
		bool found = true;	
		std::string currentSkinName("");
		LLDirIterator iter(path_name, "*.xml");
		while(found) 
		{
			std::string name;
			found = iter.next(name);
			//LL_INFOS() << "path name " << path_name << " and name " << name << " and found " << found << LL_ENDL;
			if(found)
			{
				LLSD data;
				llifstream importer(path_name+name);
				LLSDSerialize::fromXMLDocument(data, importer);

				if(data.has("folder_name"))
				{
					datas.push_back(data);
					comboBox->add(data["skin_name"].asString());
					/*LL_INFOS() << "data is length " << datas.size() << " foldername field is "
						<< data["folder_name"].asString() << " and looking for " << gSavedSettings.getString("SkinCurrent") <<LL_ENDL;*/
					if(data["folder_name"].asString()==mSkin)
					{
						//LL_INFOS() << "found!!!!!!1!1" << LL_ENDL;
						currentSkinName = data["skin_name"].asString();

						//LLButton* b;
						//b.setImageOverlay()
						childSetValue("custom_skin_author",data["author_name"].asString());
						childSetValue("custom_skin_ad_authors",data["additional_author_names"].asString());
						childSetValue("custom_skin_info",data["skin_info"].asString());
						childSetValue("custom_skin_folder",data["folder_name"].asString());
						LLButton* b = getChild<LLButton>("custom_skin_preview");
						std::string imagename = data["preview_image"].asString();
						if(imagename == "" || imagename == " " || !LLFile::isfile(imagename)) imagename = "preview.png";
						std::string imageprev(path_name+
							data["folder_name"].asString()+gDirUtilp->getDirDelimiter()+
							"textures"+gDirUtilp->getDirDelimiter()+
							imagename);
						b->setImages(imageprev,imageprev);
						b->setImageHoverSelected(LLUI::getUIImage(imageprev));
						b->setImageHoverUnselected(LLUI::getUIImage(imageprev));
	
						//<button scale_image="true" image_selected="skin_thumbnail_default.png"
						//image_unselected="skin_thumbnail_default.png" 
						//	image_hover_selected="skin_thumbnail_default.png" 
						//	image_hover_unselected="skin_thumbnail_default.png"/>

						//set the rest here!
					}
				}
			}
		}
		comboBox->setSimple(currentSkinName);
	}
}

void LLPanelSkins::apply()
{
	if (oldSkin != mSkin)
	{
		  oldSkin=mSkin;
		  LLNotificationsUtil::add("ChangeSkin");
		  refresh();
	}
}

void LLPanelSkins::cancel()
{
	// reverts any changes to current skin
	gSavedSettings.setString("SkinCurrent", oldSkin);
}

void LLPanelSkins::onComboBoxCommit(const LLSD& value)
{
	const std::string skinName = value.asString();
	for(int i = 0; i < (int)datas.size(); ++i)
	{
		const LLSD tdata = datas[i];
		if (tdata["skin_name"].asString() == skinName)
		{
			std::string newFolder(tdata["folder_name"].asString());
			gSavedSettings.setString("SkinCurrent", newFolder);
			mSkin = newFolder;

			refresh();
			return;
		}
	}
}

