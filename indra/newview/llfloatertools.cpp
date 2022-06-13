/** 
 * @file llfloatertools.cpp
 * @brief The edit tools, including move, position, land, etc.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llfloatertools.h"

#include "llfontgl.h"
#include "llcoord.h"
#include "llgl.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldraghandle.h"
#include "llfloaterbuildoptions.h"
#include "llfloatermediasettings.h"
#include "llfloateropenobject.h"
#include "llfloaterobjectweights.h"
#include "llfocusmgr.h"
#include "llmediaentry.h"
#include "llmediactrl.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "llpanelcontents.h"
#include "llpanelface.h"
#include "llpanelland.h"
#include "llpanelobjectinventory.h"
#include "llpanelobject.h"
#include "llpanelvolume.h"
#include "llpanelpermissions.h"
#include "llparcel.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llslider.h"
#include "llstatusbar.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltoolbrush.h"
#include "lltoolcomp.h"
#include "lltooldraganddrop.h"
#include "lltoolface.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolgrab.h"
#include "lltoolindividual.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "lltoolpipette.h"
#include "lltoolplacer.h"
#include "lltoolselectland.h"
#include "lltrans.h"
#include "llui.h"
#include "llviewercontrol.h"
#include "llviewerjoystick.h"
#include "llviewerregion.h"
#include "llviewermenu.h"
#include "llviewerparcelmgr.h"
#include "llviewerwindow.h"
#include "llvovolume.h"
#include "lluictrlfactory.h"
#include "llmeshrepository.h"

#include "qtoolalign.h" //Thank Qarl!

#include "llvograss.h"
#include "llvotree.h"

// Globals
LLFloaterTools *gFloaterTools = NULL;

const std::string PANEL_NAMES[LLFloaterTools::PANEL_COUNT] =
{
	std::string("General"), 	// PANEL_GENERAL,
	std::string("Object"), 	// PANEL_OBJECT,
	std::string("Features"),	// PANEL_FEATURES,
	std::string("Texture"),	// PANEL_FACE,
	std::string("Content"),	// PANEL_CONTENTS,
};


// Local prototypes
void commit_grid_mode(LLUICtrl *ctrl);
void click_show_more(void*);
void click_popup_info(void*);
void click_popup_done(void*);
void click_popup_minimize(void*);
void commit_slider_dozer_size(LLUICtrl *);
void commit_slider_dozer_force(LLUICtrl *);
void click_apply_to_selection(void*);
void commit_radio_group_focus(LLUICtrl* ctrl);
void commit_radio_group_move(LLUICtrl* ctrl);
void commit_radio_group_edit(LLUICtrl* ctrl);
void commit_radio_group_land(LLUICtrl* ctrl);
void commit_slider_zoom(LLUICtrl *ctrl);
void commit_select_tool(LLUICtrl *ctrl, void *data);

/**
 * Class LLLandImpactsObserver
 *
 * An observer class to monitor parcel selection and update
 * the land impacts data from a parcel containing the selected object.
 */
class LLLandImpactsObserver : public LLParcelObserver
{
public:
	virtual void changed()
	{
		LLFloaterTools* tools_floater = gFloaterTools;
		if(tools_floater)
		{
			tools_floater->updateLandImpacts();
		}
	}
};

//static
void*	LLFloaterTools::createPanelPermissions(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelPermissions = new LLPanelPermissions("General");
	return floater->mPanelPermissions;
}
//static
void*	LLFloaterTools::createPanelObject(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelObject = new LLPanelObject("Object");
	return floater->mPanelObject;
}

//static
void*	LLFloaterTools::createPanelVolume(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelVolume = new LLPanelVolume("Features");
	return floater->mPanelVolume;
}

//static
void*	LLFloaterTools::createPanelFace(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelFace = new LLPanelFace("Texture");
	return floater->mPanelFace;
}

//static
void*	LLFloaterTools::createPanelContents(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelContents = new LLPanelContents("Contents");
	return floater->mPanelContents;
}

//static
void*	LLFloaterTools::createPanelContentsInventory(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelContents->mPanelInventory = new LLPanelObjectInventory(std::string("ContentsInventory"), LLRect());
	return floater->mPanelContents->mPanelInventory;
}

//static
void*	LLFloaterTools::createPanelLandInfo(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelLandInfo = new LLPanelLandInfo(std::string("land info panel"));
	return floater->mPanelLandInfo;
}

static	const std::string	toolNames[]={
	"ToolCube",
	"ToolPrism",
	"ToolPyramid",
	"ToolTetrahedron",
	"ToolCylinder",
	"ToolHemiCylinder",
	"ToolCone",
	"ToolHemiCone",
	"ToolSphere",
	"ToolHemiSphere",
	"ToolTorus",
	"ToolTube",
	"ToolRing",
	"ToolTree",
	"ToolGrass"};
LLPCode toolData[]={
	LL_PCODE_CUBE,
	LL_PCODE_PRISM,
	LL_PCODE_PYRAMID,
	LL_PCODE_TETRAHEDRON,
	LL_PCODE_CYLINDER,
	LL_PCODE_CYLINDER_HEMI,
	LL_PCODE_CONE,
	LL_PCODE_CONE_HEMI,
	LL_PCODE_SPHERE,
	LL_PCODE_SPHERE_HEMI,
	LL_PCODE_TORUS,
	LLViewerObject::LL_VO_SQUARE_TORUS,
	LLViewerObject::LL_VO_TRIANGLE_TORUS,
	LL_PCODE_LEGACY_TREE,
	LL_PCODE_LEGACY_GRASS};

BOOL	LLFloaterTools::postBuild()
{	
	// Hide until tool selected
	setVisible(FALSE);

	// Since we constantly show and hide this during drags, don't
	// make sounds on visibility changes.
	setSoundFlags(LLView::SILENT);

	getDragHandle()->setEnabled( !gSavedSettings.getBOOL("ToolboxAutoMove") );

	LLRect rect;
	mBtnFocus			= getChild<LLButton>("button focus");//btn;
	mBtnMove			= getChild<LLButton>("button move");
	mBtnEdit			= getChild<LLButton>("button edit");
	mBtnCreate			= getChild<LLButton>("button create");
	mBtnLand			= getChild<LLButton>("button land" );
	mTextStatus			= getChild<LLTextBox>("text status");


	mRadioZoom				= getChild<LLCheckBoxCtrl>("radio zoom");
	mRadioOrbit				= getChild<LLCheckBoxCtrl>("radio orbit");
	mRadioPan				= getChild<LLCheckBoxCtrl>("radio pan");

	mRadioMove				= getChild<LLCheckBoxCtrl>("radio move");
	mRadioLift				= getChild<LLCheckBoxCtrl>("radio lift");
	mRadioSpin				= getChild<LLCheckBoxCtrl>("radio spin");
	mRadioPosition			= getChild<LLCheckBoxCtrl>("radio position");
	mRadioRotate			= getChild<LLCheckBoxCtrl>("radio rotate");
	mRadioStretch			= getChild<LLCheckBoxCtrl>("radio stretch");
	mRadioSelectFace		= getChild<LLCheckBoxCtrl>("radio select face");
	mRadioAlign				= getChild<LLCheckBoxCtrl>("radio align");
	mBtnGridOptions			= getChild<LLButton>("Options...");
	mTitleMedia			= getChild<LLMediaCtrl>("title_media");
	
	mCheckSelectIndividual	= getChild<LLCheckBoxCtrl>("checkbox edit linked parts");
	getChild<LLUICtrl>("checkbox edit linked parts")->setValue((BOOL)gSavedSettings.getBOOL("EditLinkedParts"));
	mCheckSnapToGrid		= getChild<LLCheckBoxCtrl>("checkbox snap to grid");
	getChild<LLUICtrl>("checkbox snap to grid")->setValue((BOOL)gSavedSettings.getBOOL("SnapEnabled"));
	mCheckStretchUniform	= getChild<LLCheckBoxCtrl>("checkbox uniform");
	getChild<LLUICtrl>("checkbox uniform")->setValue((BOOL)gSavedSettings.getBOOL("ScaleUniform"));
	mCheckStretchTexture	= getChild<LLCheckBoxCtrl>("checkbox stretch textures");
	getChild<LLUICtrl>("checkbox stretch textures")->setValue((BOOL)gSavedSettings.getBOOL("ScaleStretchTextures"));
	mCheckLimitDrag			= getChild<LLCheckBoxCtrl>("checkbox limit drag distance");
	childSetValue("checkbox limit drag distance",(BOOL)gSavedSettings.getBOOL("LimitDragDistance"));
	mTextGridMode			= getChild<LLTextBox>("text ruler mode");
	mComboGridMode			= getChild<LLComboBox>("combobox grid mode");

	mCheckShowHighlight = getChild<LLCheckBoxCtrl>("checkbox show highlight");
	mCheckActualRoot = getChild<LLCheckBoxCtrl>("checkbox actual root");

	//
	// Create Buttons
	//

	for(size_t t=0; t<LL_ARRAY_SIZE(toolNames); ++t)
	{
		LLButton *found = getChild<LLButton>(toolNames[t]);
		if(found)
		{
			found->setClickedCallback(boost::bind(&LLFloaterTools::setObjectType, toolData[t]));
			mButtons.push_back( found );
		}else{
			LL_WARNS() << "Tool button not found! DOA Pending." << LL_ENDL;
		}
	}
	if ((mComboTreesGrass = findChild<LLComboBox>("trees_grass")))
		mComboTreesGrass->setCommitCallback(boost::bind(&LLFloaterTools::onSelectTreesGrass, this));
	mCheckCopySelection = getChild<LLCheckBoxCtrl>("checkbox copy selection");
	getChild<LLUICtrl>("checkbox copy selection")->setValue((BOOL)gSavedSettings.getBOOL("CreateToolCopySelection"));
	mCheckSticky = getChild<LLCheckBoxCtrl>("checkbox sticky");
	getChild<LLUICtrl>("checkbox sticky")->setValue((BOOL)gSavedSettings.getBOOL("CreateToolKeepSelected"));
	mCheckCopyCenters = getChild<LLCheckBoxCtrl>("checkbox copy centers");
	getChild<LLUICtrl>("checkbox copy centers")->setValue((BOOL)gSavedSettings.getBOOL("CreateToolCopyCenters"));
	mCheckCopyRotates = getChild<LLCheckBoxCtrl>("checkbox copy rotates");
	getChild<LLUICtrl>("checkbox copy rotates")->setValue((BOOL)gSavedSettings.getBOOL("CreateToolCopyRotates"));
	mRadioSelectLand = getChild<LLCheckBoxCtrl>("radio select land");
	mRadioDozerFlatten = getChild<LLCheckBoxCtrl>("radio flatten");
	mRadioDozerRaise = getChild<LLCheckBoxCtrl>("radio raise");
	mRadioDozerLower = getChild<LLCheckBoxCtrl>("radio lower");
	mRadioDozerSmooth = getChild<LLCheckBoxCtrl>("radio smooth");
	mRadioDozerNoise = getChild<LLCheckBoxCtrl>("radio noise");
	mRadioDozerRevert = getChild<LLCheckBoxCtrl>("radio revert");
	mBtnApplyToSelection = getChild<LLButton>("button apply to selection");

	mSliderDozerSize		= getChild<LLSlider>("slider brush size");
	getChild<LLUICtrl>("slider brush size")->setValue(gSavedSettings.getF32("LandBrushSize"));
	mSliderDozerForce		= getChild<LLSlider>("slider force");
	// the setting stores the actual force multiplier, but the slider is logarithmic, so we convert here
	getChild<LLUICtrl>("slider force")->setValue(log10(gSavedSettings.getF32("LandBrushForce")));

	mTab = getChild<LLTabContainer>("Object Info Tabs");
	if(mTab)
	{
		mTab->setFollows(FOLLOWS_TOP | FOLLOWS_LEFT);
		mTab->setBorderVisible(FALSE);
		mTab->selectFirstTab();
	}

	mStatusText["rotate"] = getString("status_rotate");
	mStatusText["scale"] = getString("status_scale");
	mStatusText["move"] = getString("status_move");
	mStatusText["align"] = getString("status_align");
	mStatusText["modifyland"] = getString("status_modifyland");
	mStatusText["camera"] = getString("status_camera");
	mStatusText["grab"] = getString("status_grab");
	mStatusText["place"] = getString("status_place");
	mStatusText["selectland"] = getString("status_selectland");
	
	return TRUE;
}

// Create the popupview with a dummy center.  It will be moved into place
// during LLViewerWindow's per-frame hover processing.
LLFloaterTools::LLFloaterTools()
:	LLFloater(std::string("toolbox floater")),
	mBtnFocus(NULL),
	mBtnMove(NULL),
	mBtnEdit(NULL),
	mBtnCreate(NULL),
	mBtnLand(NULL),
	mTextStatus(NULL),

	//Camera Focus
	mRadioOrbit(NULL),
	mRadioZoom(NULL),
	mRadioPan(NULL),

	//Move via physics
	mRadioMove(NULL),
	mRadioLift(NULL),
	mRadioSpin(NULL),

	//Edit prim
	mRadioPosition(NULL),
	mRadioRotate(NULL),
	mRadioStretch(NULL),
	mRadioSelectFace(NULL),
	mRadioAlign(NULL),
	mCheckSelectIndividual(NULL),

	mCheckSnapToGrid(NULL),
	mBtnGridOptions(NULL),
	mTextGridMode(NULL),
	mTitleMedia(NULL),
	mComboGridMode(NULL),
	mCheckStretchUniform(NULL),
	mCheckStretchTexture(NULL),
	mCheckLimitDrag(NULL),
	mCheckShowHighlight(NULL),
	mCheckActualRoot(NULL),

	mBtnRotateLeft(NULL),
	mBtnRotateReset(NULL),
	mBtnRotateRight(NULL),

	mBtnDelete(NULL),
	mBtnDuplicate(NULL),
	mBtnDuplicateInPlace(NULL),

	mComboTreesGrass(NULL),
	mCheckSticky(NULL),
	mCheckCopySelection(NULL),
	mCheckCopyCenters(NULL),
	mCheckCopyRotates(NULL),
	
	//Edit land
	mRadioSelectLand(NULL),
	mRadioDozerFlatten(NULL),
	mRadioDozerRaise(NULL),
	mRadioDozerLower(NULL),
	mRadioDozerSmooth(NULL),
	mRadioDozerNoise(NULL),
	mRadioDozerRevert(NULL),
	
	mSliderDozerSize(NULL),
	mSliderDozerForce(NULL),
	mBtnApplyToSelection(NULL),

	mTab(NULL),
	mPanelPermissions(NULL),
	mPanelObject(NULL),
	mPanelVolume(NULL),
	mPanelContents(NULL),
	mPanelFace(NULL),
	mPanelLandInfo(NULL),

	mLandImpactsObserver(NULL),

	mDirty(TRUE),
	mNeedMediaTitle(TRUE)
{
	setAutoFocus(FALSE);
	LLCallbackMap::map_t factory_map;
	factory_map["General"] = LLCallbackMap(createPanelPermissions, this);//LLPanelPermissions
	factory_map["Object"] = LLCallbackMap(createPanelObject, this);//LLPanelObject
	factory_map["Features"] = LLCallbackMap(createPanelVolume, this);//LLPanelVolume
	factory_map["Texture"] = LLCallbackMap(createPanelFace, this);//LLPanelFace
	factory_map["Contents"] = LLCallbackMap(createPanelContents, this);//LLPanelContents
	factory_map["ContentsInventory"] = LLCallbackMap(createPanelContentsInventory, this);//LLPanelContents
	factory_map["land info panel"] = LLCallbackMap(createPanelLandInfo, this);//LLPanelLandInfo

	mCommitCallbackRegistrar.add("BuildTool.setTool",			boost::bind(&LLFloaterTools::setTool,this, _2));	
	mCommitCallbackRegistrar.add("BuildTool.commitDozerSize",	boost::bind(&commit_slider_dozer_size, _1));
	mCommitCallbackRegistrar.add("BuildTool.commitZoom",		boost::bind(&commit_slider_zoom, _1));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioFocus",	boost::bind(&commit_radio_group_focus, _1));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioMove",	boost::bind(&commit_radio_group_move,_1));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioEdit",	boost::bind(&commit_radio_group_edit,_1));

	mCommitCallbackRegistrar.add("BuildTool.gridMode",			boost::bind(&commit_grid_mode,_1));
	mCommitCallbackRegistrar.add("BuildTool.selectComponent",	boost::bind(&LLFloaterTools::commitSelectComponent, this, _2));
	mCommitCallbackRegistrar.add("BuildTool.gridOptions",		boost::bind(&LLFloaterTools::onClickGridOptions,this));
	mCommitCallbackRegistrar.add("BuildTool.applyToSelection",	boost::bind(&click_apply_to_selection, this));
	mCommitCallbackRegistrar.add("BuildTool.commitRadioLand",	boost::bind(&commit_radio_group_land,_1));
	mCommitCallbackRegistrar.add("BuildTool.LandBrushForce",	boost::bind(&commit_slider_dozer_force,_1));
	mCommitCallbackRegistrar.add("BuildTool.AddMedia",			boost::bind(&LLFloaterTools::onClickBtnAddMedia,this));
	mCommitCallbackRegistrar.add("BuildTool.DeleteMedia",		boost::bind(&LLFloaterTools::onClickBtnDeleteMedia,this));
	mCommitCallbackRegistrar.add("BuildTool.EditMedia",			boost::bind(&LLFloaterTools::onClickBtnEditMedia,this));
	
	mLandImpactsObserver = new LLLandImpactsObserver();
	LLViewerParcelMgr::getInstance()->addObserver(mLandImpactsObserver);

	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_tools.xml",&factory_map,FALSE);
	if (LLTextBox* text = findChild<LLTextBox>("selection_count"))
	{
		text->setClickedCallback(boost::bind(LLFloaterObjectWeights::toggleInstance, LLSD()));
		text->setColor(gSavedSettings.getColor4("HTMLLinkColor"));
	}
}

LLFloaterTools::~LLFloaterTools()
{
	// children automatically deleted
	gFloaterTools = NULL;

	LLViewerParcelMgr::getInstance()->removeObserver(mLandImpactsObserver);
	delete mLandImpactsObserver;
}

void LLFloaterTools::setStatusText(const std::string& text)
{
	std::map<std::string, std::string>::iterator iter = mStatusText.find(text);
	if (iter != mStatusText.end())
	{
		mTextStatus->setText(iter->second);
	}
	else
	{
		mTextStatus->setText(text);
	}
}

void LLFloaterTools::refresh()
{
	const S32 INFO_WIDTH = getRect().getWidth();
	const S32 INFO_HEIGHT = 384;
	LLRect object_info_rect(0, 0, INFO_WIDTH, -INFO_HEIGHT);
	BOOL all_volume = LLSelectMgr::getInstance()->selectionAllPCode( LL_PCODE_VOLUME );

	S32 idx_features = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_FEATURES]);
	S32 idx_face = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_FACE]);
	S32 idx_contents = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_CONTENTS]);

	S32 selected_index = mTab->getCurrentPanelIndex();

	if (!all_volume && (selected_index == idx_features || selected_index == idx_face ||
		selected_index == idx_contents))
	{
		mTab->selectFirstTab();
	}

	mTab->enableTabButton(idx_features, all_volume);
	mTab->enableTabButton(idx_face, all_volume);
	mTab->enableTabButton(idx_contents, all_volume);

	// Refresh object and prim count labels
	LLLocale locale(LLLocale::USER_LOCALE);
#if 0
	if (!gMeshRepo.meshRezEnabled())
	{
		std::string obj_count_string;
		LLResMgr::getInstance()->getIntegerString(obj_count_string, LLSelectMgr::getInstance()->getSelection()->getRootObjectCount());
		getChild<LLUICtrl>("selection_count")->setTextArg("[OBJ_COUNT]", obj_count_string);
		std::string prim_count_string;
		LLResMgr::getInstance()->getIntegerString(prim_count_string, LLSelectMgr::getInstance()->getSelection()->getObjectCount());
		getChild<LLUICtrl>("selection_count")->setTextArg("[PRIM_COUNT]", prim_count_string);

		// calculate selection rendering cost
		if (sShowObjectCost)
		{
			std::string prim_cost_string;
			S32 render_cost = LLSelectMgr::getInstance()->getSelection()->getSelectedObjectRenderCost();
			LLResMgr::getInstance()->getIntegerString(prim_cost_string, render_cost);
			getChild<LLUICtrl>("RenderingCost")->setTextArg("[COUNT]", prim_cost_string);
		}

		// disable the object and prim counts if nothing selected
		bool have_selection = ! LLSelectMgr::getInstance()->getSelection()->isEmpty();
		getChildView("obj_count")->setEnabled(have_selection);
		getChildView("prim_count")->setEnabled(have_selection);
		getChildView("RenderingCost")->setEnabled(have_selection && sShowObjectCost);
	}
	else
#endif
	{
		auto selection = LLSelectMgr::getInstance()->getSelection();
		F32 link_cost  = selection->getSelectedLinksetCost();
		S32 link_count = selection->getRootObjectCount();
		S32 prim_count = selection->getObjectCount();
		auto child = getChild<LLUICtrl>("link_num_obj_count");
		auto selected_face = -1;
		if (auto node = prim_count == 1 ? *selection->begin() : nullptr)
		//for (const auto& node : selection) // All selected objects
		{
			// TODO: Count selected faces? What use is there for this?
			if (node->getTESelectMask() == 0xFFFFFFFF) // All faces selected
			{
				selected_face = -2; // Multiple
			}
			if (const auto& obj = node->getObject()) // This node's object
			{
				//if (!obj->isSelected()) continue;
				const auto& numTEs = obj->getNumTEs();
				for (S32 te = 0; selected_face != -2 && te < numTEs; ++te) // Faces
				{
					if (!node->isTESelected(te)) continue;
					if (selected_face != -1)
					{
						selected_face = -2; // Multiple
					}
					else selected_face = te;
				}
			}
			//if (selected_face == -2) break;
		}
		// Added in Link Num value -HgB
		std::string value_string;
		bool edit_linked(mCheckSelectIndividual->getValue());
		if (mRadioSelectFace->getValue() || selected_face > 0) // In select faces mode, show either multiple or the face number, otherwise only show if single face selected
		{
			child->setTextArg("[DESC]", getString("Selected Face:"));
			if (selected_face < 0)
			{
				value_string = LLTrans::getString(selected_face == -1 && !prim_count ? "None" : "multiple_textures");
			}
			else LLResMgr::getInstance()->getIntegerString(value_string, selected_face);
		}
		else if (edit_linked && prim_count == 1) //Selecting a single prim in "Edit Linked" mode, show link number
		{
			link_cost = selection->getSelectedObjectCost();
			child->setTextArg("[DESC]", getString("Link number:"));

			if (LLViewerObject* selected = selection->getFirstObject())
			{
				if (auto root = selected->getRootEdit())
				{
					LLViewerObject::child_list_t children = root->getChildren();
					if (children.empty())
					{
						value_string = "0"; // An unlinked prim is "link 0".
					}
					else
					{
						children.push_front(selected->getRootEdit()); // need root in the list too
						S32 index = 1;
						for (const auto& child : children)
						{
							if (child->isSelected())
							{
								LLResMgr::getInstance()->getIntegerString(value_string, index);
								break;
							}
							++index;
						}
					}
				}
			}
		}
		else if (edit_linked)
		{
			child->setTextArg("[DESC]", getString("Selected prims:"));
			LLResMgr::getInstance()->getIntegerString(value_string, prim_count);
			link_cost = selection->getSelectedObjectCost();
		}
		else
		{
			child->setTextArg("[DESC]", getString("Selected objects:"));
			LLResMgr::getInstance()->getIntegerString(value_string, link_count);
		}
		child->setTextArg("[NUM]", value_string);

		/* Singu Note: We're not using this yet because we have no place to put it
		LLCrossParcelFunctor func;
		if (LLSelectMgr::getInstance()->getSelection()->applyToRootObjects(&func, true))
		{
			// Selection crosses parcel bounds.
			// We don't display remaining land capacity in this case.
			const LLStringExplicit empty_str("");
			childSetTextArg("remaining_capacity", "[CAPACITY_STRING]", empty_str);
		}
		else
		*/
		{
			LLViewerObject* selected_object = mObjectSelection->getFirstObject();
			if (selected_object)
			{
				// Select a parcel at the currently selected object's position.
				LLViewerParcelMgr::getInstance()->selectParcelAt(selected_object->getPositionGlobal());
			}
			else
			{
				//LL_WARNS() << "Failed to get selected object" << LL_ENDL;
			}
		}

		auto sel_count = getChild<LLTextBox>("selection_count");
		bool have_selection = !selection->isEmpty();
		if (have_selection)
		{
			LLStringUtil::format_map_t selection_args;
			selection_args["OBJ_COUNT"] = llformat("%.1d", prim_count);
			selection_args["LAND_IMPACT"] = llformat(edit_linked ? "%.2f" : "%.0f", link_cost);

			sel_count->setText(getString("status_selectcount", selection_args));
		}
		sel_count->setVisible(have_selection);
		//childSetVisible("remaining_capacity", have_selection);
		childSetVisible("selection_empty", !have_selection);
	}


	// Refresh child tabs
	mPanelPermissions->refresh();
	mPanelObject->refresh();
	mPanelVolume->refresh();
	mPanelFace->refresh();
	if(mTitleMedia)
		refreshMedia();
	mPanelContents->refresh();
	mPanelLandInfo->refresh();

	// Refresh the advanced weights floater
	if (LLFloaterObjectWeights::instanceVisible())
	{
		LLFloaterObjectWeights::getInstance()->refresh();
	}
}

void LLFloaterTools::draw()
{
	if (mDirty)
	{
		refresh();
		mDirty = FALSE;
	}

	// grab media name/title and update the UI widget
	if(mTitleMedia)
		updateMediaTitle();

	//	mCheckSelectIndividual->set(gSavedSettings.getBOOL("EditLinkedParts"));
	LLFloater::draw();
}

void LLFloaterTools::dirty()
{
	mDirty = TRUE; 
	LLFloaterOpenObject::dirty();
}

// Clean up any tool state that should not persist when the
// floater is closed.
void LLFloaterTools::resetToolState()
{
	gCameraBtnZoom = TRUE;
	gCameraBtnOrbit = FALSE;
	gCameraBtnPan = FALSE;

	gGrabBtnSpin = FALSE;
	gGrabBtnVertical = FALSE;
}

void LLFloaterTools::updatePopup(LLCoordGL center, MASK mask)
{
	LLTool *tool = LLToolMgr::getInstance()->getCurrentTool();

	// HACK to allow seeing the buttons when you have the app in a window.
	// Keep the visibility the same as it 
	if (tool == gToolNull)
	{
		return;
	}

	if ( isMinimized() )
	{	// SL looks odd if we draw the tools while the window is minimized
		return;
	}
	
	// Focus buttons
	BOOL focus_visible = (	tool == LLToolCamera::getInstance() );

	mBtnFocus	->setToggleState( focus_visible );

	mRadioZoom	->setVisible( focus_visible );
	mRadioOrbit	->setVisible( focus_visible );
	mRadioPan	->setVisible( focus_visible );
	getChildView("slider zoom")->setVisible(focus_visible);
	getChildView("slider zoom")->setEnabled(gCameraBtnZoom);

	mRadioZoom	->set(!gCameraBtnOrbit &&
						!gCameraBtnPan &&
						!(mask == MASK_ORBIT) &&
						!(mask == (MASK_ORBIT | MASK_ALT)) &&
						!(mask == MASK_PAN) &&
						!(mask == (MASK_PAN | MASK_ALT)) );

	mRadioOrbit	->set(	gCameraBtnOrbit || 
						(mask == MASK_ORBIT) ||
						(mask == (MASK_ORBIT | MASK_ALT)) );

	mRadioPan	->set(	gCameraBtnPan ||
						(mask == MASK_PAN) ||
						(mask == (MASK_PAN | MASK_ALT)) );

	// multiply by correction factor because volume sliders go [0, 0.5]
	getChild<LLUICtrl>("slider zoom")->setValue(gAgentCamera.getCameraZoomFraction() * 0.5f);

	// Move buttons
	BOOL move_visible = (tool == LLToolGrab::getInstance());

	if (mBtnMove) mBtnMove	->setToggleState( move_visible );

	// HACK - highlight buttons for next click
	if (mRadioMove)
	{
		mRadioMove	->setVisible( move_visible );
		mRadioMove	->set(	!gGrabBtnSpin && 
							!gGrabBtnVertical &&
							!(mask == MASK_VERTICAL) && 
							!(mask == MASK_SPIN) );
	}

	if (mRadioLift)
	{
		mRadioLift	->setVisible( move_visible );
		mRadioLift	->set(	gGrabBtnVertical || 
							(mask == MASK_VERTICAL) );
	}

	if (mRadioSpin)
	{
		mRadioSpin	->setVisible( move_visible );
		mRadioSpin	->set(	gGrabBtnSpin || 
							(mask == MASK_SPIN) );
	}

	// Edit buttons
	BOOL edit_visible = tool == LLToolCompTranslate::getInstance() ||
						tool == LLToolCompRotate::getInstance() ||
						tool == LLToolCompScale::getInstance() ||
						tool == LLToolFace::getInstance() ||
						tool == QToolAlign::getInstance() ||
						tool == LLToolIndividual::getInstance() ||
						tool == LLToolPipette::getInstance();

	mBtnEdit	->setToggleState( edit_visible );

	mRadioPosition	->setVisible( edit_visible );
	mRadioRotate	->setVisible( edit_visible );
	mRadioStretch	->setVisible( edit_visible );
	mRadioAlign		->setVisible( edit_visible );
	if (mRadioSelectFace)
	{
		mRadioSelectFace->setVisible( edit_visible );
		mRadioSelectFace->set( tool == LLToolFace::getInstance() );
	}

	if (mCheckSelectIndividual)
	{
		mCheckSelectIndividual->setVisible(edit_visible);
		//mCheckSelectIndividual->set(gSavedSettings.getBOOL("EditLinkedParts"));
	}

	mRadioPosition	->set( tool == LLToolCompTranslate::getInstance() );
	mRadioRotate	->set( tool == LLToolCompRotate::getInstance() );
	mRadioStretch	->set( tool == LLToolCompScale::getInstance() );
	mRadioAlign		->set( tool == QToolAlign::getInstance() );

	if (mComboGridMode) 
	{
		mComboGridMode->setVisible( edit_visible );
		S32 index = mComboGridMode->getCurrentIndex();
		mComboGridMode->removeall();

		switch (mObjectSelection->getSelectType())
		{
			case SELECT_TYPE_HUD:
				mComboGridMode->add(getString("grid_screen_text"));
				mComboGridMode->add(getString("grid_local_text"));
				break;
			case SELECT_TYPE_WORLD:
				mComboGridMode->add(getString("grid_world_text"));
				mComboGridMode->add(getString("grid_local_text"));
				mComboGridMode->add(getString("grid_reference_text"));
				break;
			case SELECT_TYPE_ATTACHMENT:
				mComboGridMode->add(getString("grid_attachment_text"));
				mComboGridMode->add(getString("grid_local_text"));
				mComboGridMode->add(getString("grid_reference_text"));
				break;
		}

		mComboGridMode->setCurrentByIndex(index);
	}
	if (mTextGridMode) mTextGridMode->setVisible( edit_visible );

	// Snap to grid disabled for grab tool - very confusing
	if (mCheckSnapToGrid) mCheckSnapToGrid->setVisible( edit_visible /* || tool == LLToolGrab::getInstance() */ );
	if (mBtnGridOptions) mBtnGridOptions->setVisible( edit_visible /* || tool == LLToolGrab::getInstance() */ );

	//mCheckSelectLinked	->setVisible( edit_visible );
	if (mCheckStretchUniform) mCheckStretchUniform->setVisible( edit_visible );
	if (mCheckStretchTexture) mCheckStretchTexture->setVisible( edit_visible );
	if (mCheckLimitDrag) mCheckLimitDrag->setVisible( edit_visible );
	if (mCheckShowHighlight) mCheckShowHighlight->setVisible(edit_visible);
	if (mCheckActualRoot) mCheckActualRoot->setVisible(edit_visible);

	// Create buttons
	BOOL create_visible = (tool == LLToolCompCreate::getInstance());

	mBtnCreate	->setToggleState(create_visible);

	if (mComboTreesGrass)
		updateTreeGrassCombo(create_visible);

	if (mCheckCopySelection
		&& mCheckCopySelection->get())
	{
		// don't highlight any placer button
		for (std::vector<LLButton*>::size_type i = 0; i < mButtons.size(); i++)
		{
			mButtons[i]->setToggleState(FALSE);
			mButtons[i]->setVisible( create_visible );
		}
	}
	else
	{
		// Highlight the correct placer button
		for( S32 t = 0; t < (S32)mButtons.size(); t++ )
		{
			LLPCode pcode = LLToolPlacer::getObjectType();
			LLPCode button_pcode = toolData[t];
			BOOL state = (pcode == button_pcode);
			mButtons[t]->setToggleState( state );
			mButtons[t]->setVisible( create_visible );
		}
	}

	if (mCheckSticky) mCheckSticky		->setVisible( create_visible );
	if (mCheckCopySelection) mCheckCopySelection	->setVisible( create_visible );
	if (mCheckCopyCenters) mCheckCopyCenters	->setVisible( create_visible );
	if (mCheckCopyRotates) mCheckCopyRotates	->setVisible( create_visible );

	if (mCheckCopyCenters && mCheckCopySelection) mCheckCopyCenters->setEnabled( mCheckCopySelection->get() );
	if (mCheckCopyRotates && mCheckCopySelection) mCheckCopyRotates->setEnabled( mCheckCopySelection->get() );

	// Land buttons
	BOOL land_visible = (tool == LLToolBrushLand::getInstance() || tool == LLToolSelectLand::getInstance() );

	if (mBtnLand)	mBtnLand	->setToggleState( land_visible );

	if (mRadioSelectLand)	mRadioSelectLand->set( tool == LLToolSelectLand::getInstance() );

	if (mRadioSelectLand)	mRadioSelectLand->setVisible( land_visible );

	S32 dozer_mode = gSavedSettings.getS32("RadioLandBrushAction");

	if (mRadioDozerFlatten)
	{
		mRadioDozerFlatten	->set( tool == LLToolBrushLand::getInstance() && dozer_mode == 0);
		mRadioDozerFlatten	->setVisible( land_visible );
	}
	if (mRadioDozerRaise)
	{
		mRadioDozerRaise	->set( tool == LLToolBrushLand::getInstance() && dozer_mode == 1);
		mRadioDozerRaise	->setVisible( land_visible );
	}
	if (mRadioDozerLower)
	{
		mRadioDozerLower	->set( tool == LLToolBrushLand::getInstance() && dozer_mode == 2);
		mRadioDozerLower	->setVisible( land_visible );
	}
	if (mRadioDozerSmooth)
	{
		mRadioDozerSmooth	->set( tool == LLToolBrushLand::getInstance() && dozer_mode == 3);
		mRadioDozerSmooth	->setVisible( land_visible );
	}
	if (mRadioDozerNoise)
	{
		mRadioDozerNoise	->set( tool == LLToolBrushLand::getInstance() && dozer_mode == 4);
		mRadioDozerNoise	->setVisible( land_visible );
	}
	if (mRadioDozerRevert)
	{
		mRadioDozerRevert	->set( tool == LLToolBrushLand::getInstance() && dozer_mode == 5);
		mRadioDozerRevert	->setVisible( land_visible );
	}

	if (mBtnApplyToSelection)
	{
		mBtnApplyToSelection->setVisible( land_visible );
		mBtnApplyToSelection->setEnabled( land_visible && !LLViewerParcelMgr::getInstance()->selectionEmpty() && tool != LLToolSelectLand::getInstance());
	}
	if (mSliderDozerSize)
	{
		mSliderDozerSize	->setVisible( land_visible );
		getChildView("Bulldozer:")->setVisible( land_visible);
		getChildView("Dozer Size:")->setVisible( land_visible);
	}
	if (mSliderDozerForce)
	{
		mSliderDozerForce	->setVisible( land_visible );
		getChildView("Strength:")->setVisible( land_visible);
	}

	bool have_selection = !LLSelectMgr::getInstance()->getSelection()->isEmpty();

	getChildView("link_num_obj_count")->setVisible(!land_visible && have_selection);
	getChildView("selection_count")->setVisible(!land_visible && have_selection);
	//getChildView("remaining_capacity")->setVisible(!land_visible && have_selection);
	getChildView("selection_empty")->setVisible(!land_visible && !have_selection);

	static const LLCachedControl<bool> mini("LiruMiniBuildFloater");
	mTab->setVisible(!mini && !land_visible);
	getChildView("mini_button")->setVisible(!land_visible);
	bool small = mini && !land_visible;
	const S32 cur_height = getRect().getHeight();
	static const S32 full_height = cur_height;
	if (small == (cur_height == full_height))
	{
		S32 new_height = small ? full_height - mTab->getRect().getHeight() + 8 : full_height;
		translate(0, cur_height - new_height);
		reshape(getRect().getWidth(), new_height);
	}
	mPanelLandInfo->setVisible(land_visible);
}


// virtual
BOOL LLFloaterTools::canClose()
{
	// don't close when quitting, so camera will stay put
	return !LLApp::isExiting();
}

// virtual
void LLFloaterTools::onOpen()
{
	mParcelSelection = LLViewerParcelMgr::getInstance()->getFloatingParcelSelection();
	mObjectSelection = LLSelectMgr::getInstance()->getEditSelection();
	
	//gMenuBarView->setItemVisible(std::string("Tools"), TRUE);
	//gMenuBarView->arrange();
}

// virtual
void LLFloaterTools::onClose(bool app_quitting)
{
	setMinimized(FALSE);
	setVisible(FALSE);
	mTab->setVisible(FALSE);

	LLViewerJoystick::getInstance()->moveAvatar(false);

	// destroy media source used to grab media title
	if( mTitleMedia )
		mTitleMedia->unloadMediaSource();

    // Different from handle_reset_view in that it doesn't actually 
	//   move the camera if EditCameraMovement is not set.
	gAgentCamera.resetView(gSavedSettings.getBOOL("EditCameraMovement"));
	
	// exit component selection mode
	LLSelectMgr::getInstance()->promoteSelectionToRoot();
	gSavedSettings.setBOOL("EditLinkedParts", FALSE);

	gViewerWindow->showCursor();

	resetToolState();

	mParcelSelection = NULL;
	mObjectSelection = NULL;

	if (!gAgentCamera.cameraMouselook())
	{
		// Switch back to basic toolset
		LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);
		// we were already in basic toolset, using build tools
		// so manually reset tool to default (pie menu tool)
		LLToolMgr::getInstance()->getCurrentToolset()->selectFirstTool();
	}
	else
	{
		// Switch back to mouselook toolset
		LLToolMgr::getInstance()->setCurrentToolset(gMouselookToolset);

		gViewerWindow->hideCursor();
		gViewerWindow->moveCursorToCenter();
	}

	// gMenuBarView->setItemVisible(std::string("Tools"), FALSE);
	// gMenuBarView->arrange();

	if(LLFloaterMediaSettings::instanceExists())
		LLFloaterMediaSettings::getInstance()->close();

	// hide the advanced object weights floater
	LLFloaterObjectWeights::hideInstance();
}

void LLFloaterTools::showPanel(EInfoPanel panel)
{
	llassert(panel >= 0 && panel < PANEL_COUNT);
	mTab->selectTabByName(PANEL_NAMES[panel]);
}

void click_popup_info(void*)
{
}

void click_popup_done(void*)
{
	handle_reset_view();
}

void commit_radio_group_move(LLUICtrl* ctrl)
{
	std::string selected = ctrl->getName();
	if (selected == "radio move")
	{
		gGrabBtnVertical = FALSE;
		gGrabBtnSpin = FALSE;
	}
	else if (selected == "radio lift")
	{
		gGrabBtnVertical = TRUE;
		gGrabBtnSpin = FALSE;
	}
	else if (selected == "radio spin")
	{
		gGrabBtnVertical = FALSE;
		gGrabBtnSpin = TRUE;
	}
}

void commit_radio_group_focus(LLUICtrl* ctrl)
{
	std::string selected = ctrl->getName();
	if (selected == "radio zoom")
	{
		gCameraBtnZoom = TRUE;
		gCameraBtnOrbit = FALSE;
		gCameraBtnPan = FALSE;
	}
	else if (selected == "radio orbit")
	{
		gCameraBtnZoom = FALSE;
		gCameraBtnOrbit = TRUE;
		gCameraBtnPan = FALSE;
	}
	else if (selected == "radio pan")
	{
		gCameraBtnZoom = FALSE;
		gCameraBtnOrbit = FALSE;
		gCameraBtnPan = TRUE;
	}
}

void commit_slider_zoom(LLUICtrl *ctrl)
{
	// renormalize value, since max "volume" level is 0.5 for some reason
	F32 zoom_level = (F32)ctrl->getValue().asReal() * 2.f; // / 0.5f;
	gAgentCamera.setCameraZoomFraction(zoom_level);
}

void commit_slider_dozer_size(LLUICtrl *ctrl)
{
	F32 size = (F32)ctrl->getValue().asReal();
	gSavedSettings.setF32("LandBrushSize", size);
}

void commit_slider_dozer_force(LLUICtrl *ctrl)
{
	// the slider is logarithmic, so we exponentiate to get the actual force multiplier
	F32 dozer_force = pow(10.f, (F32)ctrl->getValue().asReal());
	gSavedSettings.setF32("LandBrushForce", dozer_force);
}

void click_apply_to_selection(void*)
{
	LLToolBrushLand::getInstance()->modifyLandInSelectionGlobal();
}

void commit_radio_group_edit(LLUICtrl *ctrl)
{
	S32 show_owners = gSavedSettings.getBOOL("ShowParcelOwners");

	std::string selected = ctrl->getName();
	if (selected == "radio position")
	{
		LLFloaterTools::setEditTool( LLToolCompTranslate::getInstance() );
	}
	else if (selected == "radio rotate")
	{
		LLFloaterTools::setEditTool( LLToolCompRotate::getInstance() );
	}
	else if (selected == "radio stretch")
	{
		LLFloaterTools::setEditTool( LLToolCompScale::getInstance() );
	}
	else if (selected == "radio select face")
	{
		LLFloaterTools::setEditTool( LLToolFace::getInstance() );
	}
	else if (selected == "radio align")
	{
		LLFloaterTools::setEditTool( QToolAlign::getInstance() );
	}
	gSavedSettings.setBOOL("ShowParcelOwners", show_owners);
}

void commit_radio_group_land(LLUICtrl* ctrl)
{
	std::string selected = ctrl->getName();
	if (selected == "radio select land")
	{
		LLFloaterTools::setEditTool( LLToolSelectLand::getInstance() );
	}
	else
	{
		LLFloaterTools::setEditTool( LLToolBrushLand::getInstance() );
		S32 dozer_mode = gSavedSettings.getS32("RadioLandBrushAction");
		if (selected == "radio flatten")
			dozer_mode = 0;
		else if (selected == "radio raise")
			dozer_mode = 1;
		else if (selected == "radio lower")
			dozer_mode = 2;
		else if (selected == "radio smooth")
			dozer_mode = 3;
		else if (selected == "radio noise")
			dozer_mode = 4;
		else if (selected == "radio revert")
			dozer_mode = 5;
		gSavedSettings.setS32("RadioLandBrushAction", dozer_mode);
	}
}

void LLFloaterTools::commitSelectComponent(bool select_individuals)
{
	//forfeit focus
	if (gFocusMgr.childHasKeyboardFocus(this))
	{
		gFocusMgr.setKeyboardFocus(NULL);
	}
	dirty();

	if (select_individuals)
	{
		LLSelectMgr::getInstance()->demoteSelectionToIndividuals();
	}
	else
	{
		LLSelectMgr::getInstance()->promoteSelectionToRoot();
	}
}

// static 
void LLFloaterTools::setObjectType( LLPCode pcode )
{
	LLToolPlacer::setObjectType( pcode );
	gSavedSettings.setBOOL("CreateToolCopySelection", FALSE);
	gFocusMgr.setMouseCapture(NULL);
}

void commit_grid_mode(LLUICtrl *ctrl)
{
	LLComboBox* combo = (LLComboBox*)ctrl;

	LLSelectMgr::getInstance()->setGridMode((EGridMode)combo->getCurrentIndex());
}

void LLFloaterTools::onClickGridOptions()
{
	LLFloaterBuildOptions::show(NULL);
	// RN: this makes grid options dependent on build tools window
	//floaterp->addDependentFloater(LLFloaterBuildOptions::getInstance(), FALSE);
}

// static
void LLFloaterTools::setEditTool(void* tool_pointer)
{
	LLTool *tool = (LLTool *)tool_pointer;
	LLToolMgr::getInstance()->getCurrentToolset()->selectTool( tool );
}

void LLFloaterTools::setTool(const LLSD& user_data)
{
	std::string control_name = user_data.asString();
	if(control_name == "Focus")
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool((LLTool *) LLToolCamera::getInstance() );
	else if (control_name == "Move" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *)LLToolGrab::getInstance() );
	else if (control_name == "Edit" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *) LLToolCompTranslate::getInstance());
	else if (control_name == "Create" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *) LLToolCompCreate::getInstance());
	else if (control_name == "Land" )
		LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *) LLToolSelectLand::getInstance());
	else
		LL_WARNS() <<" no parameter name "<<control_name<<" found!! No Tool selected!!"<< LL_ENDL;
}

void LLFloaterTools::onFocusReceived()
{
	LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);
	LLFloater::onFocusReceived();
}

// Media stuff
void LLFloaterTools::refreshMedia()
{
	if(!mTitleMedia)
		return;
	getMediaState();	
}

bool LLFloaterTools::selectedMediaEditable()
{
	U32 owner_mask_on;
	U32 owner_mask_off;
	U32 valid_owner_perms = LLSelectMgr::getInstance()->selectGetPerm( PERM_OWNER, 
																	  &owner_mask_on, &owner_mask_off );
	U32 group_mask_on;
	U32 group_mask_off;
	U32 valid_group_perms = LLSelectMgr::getInstance()->selectGetPerm( PERM_GROUP, 
																	  &group_mask_on, &group_mask_off );
	U32 everyone_mask_on;
	U32 everyone_mask_off;
	S32 valid_everyone_perms = LLSelectMgr::getInstance()->selectGetPerm( PERM_EVERYONE, 
																		 &everyone_mask_on, &everyone_mask_off );
	
	bool selected_Media_editable = false;
	
	// if perms we got back are valid
	if ( valid_owner_perms &&
		valid_group_perms && 
		valid_everyone_perms )
	{
		
		if ( ( owner_mask_on & PERM_MODIFY ) ||
			( group_mask_on & PERM_MODIFY ) || 
			( group_mask_on & PERM_MODIFY ) )
		{
			selected_Media_editable = true;
		}
		else
			// user is NOT allowed to press the RESET button
		{
			selected_Media_editable = false;
		};
	};
	
	return selected_Media_editable;
}

void LLFloaterTools::updateLandImpacts()
{
	LLParcel *parcel = mParcelSelection->getParcel();
	if (!parcel)
	{
		return;
	}

	/* Singu Note: We're not using this yet because we have no place to put it
	S32 rezzed_prims = parcel->getSimWidePrimCount();
	S32 total_capacity = parcel->getSimWideMaxPrimCapacity();

	std::string remaining_capacity_str = "";

	bool show_mesh_cost = gMeshRepo.meshRezEnabled();
	if (show_mesh_cost)
	{
		LLStringUtil::format_map_t remaining_capacity_args;
		remaining_capacity_args["LAND_CAPACITY"] = llformat("%d", total_capacity - rezzed_prims);
		remaining_capacity_str = getString("status_remaining_capacity", remaining_capacity_args);
	}

	childSetTextArg("remaining_capacity", "[CAPACITY_STRING]", remaining_capacity_str);
	*/

	// Update land impacts info in the weights floater
	if (LLFloaterObjectWeights::instanceVisible())
	{
		LLFloaterObjectWeights::getInstance()->updateLandImpacts(parcel);
	}
}

void LLFloaterTools::getMediaState()
{
	LLObjectSelectionHandle selected_objects =LLSelectMgr::getInstance()->getSelection();
	LLViewerObject* first_object = selected_objects->getFirstObject();
	LLTextBox* media_info = getChild<LLTextBox>("media_info");
	
	if( !(first_object 
		  && first_object->getPCode() == LL_PCODE_VOLUME
		  &&first_object->permModify() 
	      ))
	{
		getChildView("Add_Media")->setEnabled(FALSE);
		media_info->setValue("");
		clearMediaSettings();
		return;
	}
	
	std::string url = first_object->getRegion()->getCapability("ObjectMedia");
	bool has_media_capability = (!url.empty());
	
	if(!has_media_capability)
	{
		getChildView("Add_Media")->setEnabled(FALSE);
		LL_WARNS("LLFloaterTools: media") << "Media not enabled (no capability) in this region!" << LL_ENDL;
		clearMediaSettings();
		return;
	}
	
	BOOL is_nonpermanent_enforced = (LLSelectMgr::getInstance()->getSelection()->getFirstRootNode() 
		&& LLSelectMgr::getInstance()->selectGetRootsNonPermanentEnforced())
		|| LLSelectMgr::getInstance()->selectGetNonPermanentEnforced();
	bool editable = is_nonpermanent_enforced && (first_object->permModify() || selectedMediaEditable());

	// Check modify permissions and whether any selected objects are in
	// the process of being fetched.  If they are, then we're not editable
	if (editable)
	{
		LLObjectSelection::iterator iter = selected_objects->begin(); 
		LLObjectSelection::iterator end = selected_objects->end();
		for ( ; iter != end; ++iter)
		{
			LLSelectNode* node = *iter;
			LLVOVolume* object = node ? node->getObject()->asVolume() : nullptr;
			if (nullptr != object)
			{
				if (!object->permModify())
				{
					LL_INFOS("LLFloaterTools: media")
						<< "Selection not editable due to lack of modify permissions on object id "
						<< object->getID() << LL_ENDL;
					
					editable = false;
					break;
				}
				// XXX DISABLE this for now, because when the fetch finally 
				// does come in, the state of this floater doesn't properly
				// update.  Re-selecting fixes the problem, but there is 
				// contention as to whether this is a sufficient solution.
//				if (object->isMediaDataBeingFetched())
//				{
//					LL_INFOS("LLFloaterTools: media")
//						<< "Selection not editable due to media data being fetched for object id "
//						<< object->getID() << LL_ENDL;
//						
//					editable = false;
//					break;
//				}
			}
		}
	}

	// Media settings
	bool bool_has_media = false;
	struct media_functor : public LLSelectedTEGetFunctor<bool>
	{
		bool get(LLViewerObject* object, S32 face)
		{
			LLTextureEntry *te = object->getTE(face);
			if (te)
			{
				return te->hasMedia();
			}
			return false;
		}
	} func;
	
	
	// check if all faces have media(or, all dont have media)
	LLFloaterMediaSettings::getInstance()->mIdenticalHasMediaInfo = selected_objects->getSelectedTEValue( &func, bool_has_media );
	
	const LLMediaEntry default_media_data;
	
	struct functor_getter_media_data : public LLSelectedTEGetFunctor< LLMediaEntry>
    {
		functor_getter_media_data(const LLMediaEntry& entry): mMediaEntry(entry) {}	

        LLMediaEntry get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return *(object->getTE(face)->getMediaData());
			return mMediaEntry;
        };
		
		const LLMediaEntry& mMediaEntry;
		
    } func_media_data(default_media_data);

	LLMediaEntry media_data_get;
    LLFloaterMediaSettings::getInstance()->mMultipleMedia = !(selected_objects->getSelectedTEValue( &func_media_data, media_data_get ));
	
	std::string multi_media_info_str = LLTrans::getString("Multiple Media");
	std::string media_title = "";
	mNeedMediaTitle = false;
	// update UI depending on whether "object" (prim or face) has media
	// and whether or not you are allowed to edit it.
	
	getChildView("Add_Media")->setEnabled(editable);
	// IF all the faces have media (or all dont have media)
	if ( LLFloaterMediaSettings::getInstance()->mIdenticalHasMediaInfo )
	{
		// TODO: get media title and set it.
		media_info->setValue("");
		// if identical is set, all faces are same (whether all empty or has the same media)
		if(!(LLFloaterMediaSettings::getInstance()->mMultipleMedia) )
		{
			// Media data is valid
			if(media_data_get!=default_media_data)
			{
				// initial media title is the media URL (until we get the name)
				media_title = media_data_get.getHomeURL();

				// kick off a navigate and flag that we need to update the title
				navigateToTitleMedia( media_data_get.getHomeURL() );
				mNeedMediaTitle = true;
			}
			// else all faces might be empty. 
		}
		else // there' re Different Medias' been set on on the faces.
		{
			media_title = multi_media_info_str;
			mNeedMediaTitle = false;
		}
		
		getChildView("media_tex")->setEnabled(bool_has_media && editable);
		getChildView("edit_media")->setEnabled(bool_has_media && LLFloaterMediaSettings::getInstance()->mIdenticalHasMediaInfo && editable );
		getChildView("delete_media")->setEnabled(bool_has_media && editable );
		getChildView("add_media")->setEnabled(editable);
			// TODO: display a list of all media on the face - use 'identical' flag
	}
	else // not all face has media but at least one does.
	{
		// seleted faces have not identical value
		LLFloaterMediaSettings::getInstance()->mMultipleValidMedia = selected_objects->isMultipleTEValue(&func_media_data, default_media_data );
	
		if(LLFloaterMediaSettings::getInstance()->mMultipleValidMedia)
		{
			media_title = multi_media_info_str;
			mNeedMediaTitle = false;
		}
		else
		{
			// Media data is valid
			if(media_data_get!=default_media_data)
			{
				// initial media title is the media URL (until we get the name)
				media_title = media_data_get.getHomeURL();

				// kick off a navigate and flag that we need to update the title
				navigateToTitleMedia( media_data_get.getHomeURL() );
				mNeedMediaTitle = true;
			}
		}
		
		getChildView("media_tex")->setEnabled(TRUE);
		getChildView("edit_media")->setEnabled(LLFloaterMediaSettings::getInstance()->mIdenticalHasMediaInfo);
		getChildView("delete_media")->setEnabled(TRUE);
		getChildView("add_media")->setEnabled(editable);
	}
	media_info->setText(media_title);
	
	// load values for media settings
	updateMediaSettings();
	
	if(mTitleMedia)
		LLFloaterMediaSettings::initValues(mMediaSettings, editable );
}


//////////////////////////////////////////////////////////////////////////////
// called when a user wants to add media to a prim or prim face
void LLFloaterTools::onClickBtnAddMedia()
{
	// check if multiple faces are selected
	if(LLSelectMgr::getInstance()->getSelection()->isMultipleTESelected())
	{
		LLNotificationsUtil::add("MultipleFacesSelected", LLSD(), LLSD(), multipleFacesSelectedConfirm);
	}
	else
	{
		onClickBtnEditMedia();
	}
}

// static
bool LLFloaterTools::multipleFacesSelectedConfirm(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	switch( option )
	{
		case 0:  // "Yes"
			gFloaterTools->onClickBtnEditMedia();
			break;
		case 1:  // "No"
		default:
			break;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////
// called when a user wants to edit existing media settings on a prim or prim face
// TODO: test if there is media on the item and only allow editing if present
void LLFloaterTools::onClickBtnEditMedia()
{
	refreshMedia();
	LLFloaterMediaSettings::getInstance()->open();
	LLFloaterMediaSettings::getInstance()->setVisible(TRUE);
	const LLRect& rect = getRect();
	U32 height_offset = rect.getHeight() - LLFloaterMediaSettings::getInstance()->getRect().getHeight();
	LLFloaterMediaSettings::getInstance()->setOrigin(rect.mRight, rect.mBottom + height_offset);
}

//////////////////////////////////////////////////////////////////////////////
// called when a user wants to delete media from a prim or prim face
void LLFloaterTools::onClickBtnDeleteMedia()
{
	LLNotificationsUtil::add("DeleteMedia", LLSD(), LLSD(), deleteMediaConfirm);
}


// static
bool LLFloaterTools::deleteMediaConfirm(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	switch( option )
	{
		case 0:  // "Yes"
			LLSelectMgr::getInstance()->selectionSetMedia( 0, LLSD() );
			if(LLFloaterMediaSettings::instanceExists())
			{
				LLFloaterMediaSettings::getInstance()->close();
			}
			break;
			
		case 1:  // "No"
		default:
			break;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////
//
void LLFloaterTools::clearMediaSettings()
{
	LLFloaterMediaSettings::clearValues(false);
}

//////////////////////////////////////////////////////////////////////////////
//
void LLFloaterTools::navigateToTitleMedia( const std::string url )
{
	if ( mTitleMedia )
	{
		LLPluginClassMedia* media_plugin = mTitleMedia->getMediaPlugin();
		if ( media_plugin )
		{
			// if it's a movie, we don't want to hear it
			media_plugin->setVolume( 0 );
		};
		mTitleMedia->navigateTo( url );
	};
}

//////////////////////////////////////////////////////////////////////////////
//
void LLFloaterTools::updateMediaTitle()
{
	// only get the media name if we need it
	if ( ! mNeedMediaTitle || !mTitleMedia )
		return;

	// get plugin impl
	LLPluginClassMedia* media_plugin = mTitleMedia->getMediaPlugin();
	if ( media_plugin )
	{
		// get the media name (asynchronous - must call repeatedly)
		std::string media_title = media_plugin->getMediaName();

		// only replace the title if what we get contains something
		if ( ! media_title.empty() )
		{
			// update the UI widget
			LLTextBox* media_title_field = getChild<LLTextBox>("media_info");
			if ( media_title_field )
			{
				media_title_field->setText( media_title );

				// stop looking for a title when we get one
				// FIXME: check this is the right approach
				mNeedMediaTitle = false;
			};
		};
	};
}

//////////////////////////////////////////////////////////////////////////////
//
void LLFloaterTools::updateMediaSettings()
{
    bool identical( false );
    std::string base_key( "" );
    std::string value_str( "" );
    int value_int = 0;
    bool value_bool = false;
	LLObjectSelectionHandle selected_objects =LLSelectMgr::getInstance()->getSelection();
    // TODO: (CP) refactor this using something clever or boost or both !!

    const LLMediaEntry default_media_data;

    // controls 
    U8 value_u8 = default_media_data.getControls();
    struct functor_getter_controls : public LLSelectedTEGetFunctor< U8 >
    {
		functor_getter_controls(const LLMediaEntry &entry) : mMediaEntry(entry) {}
		
        U8 get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getControls();
            return mMediaEntry.getControls();
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_controls(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_controls, value_u8 );
    base_key = std::string( LLMediaEntry::CONTROLS_KEY );
    mMediaSettings[ base_key ] = value_u8;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // First click (formerly left click)
    value_bool = default_media_data.getFirstClickInteract();
    struct functor_getter_first_click : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_first_click(const LLMediaEntry& entry): mMediaEntry(entry) {}		
		
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getFirstClickInteract();
            return mMediaEntry.getFirstClickInteract();
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_first_click(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_first_click, value_bool );
    base_key = std::string( LLMediaEntry::FIRST_CLICK_INTERACT_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Home URL
    value_str = default_media_data.getHomeURL();
    struct functor_getter_home_url : public LLSelectedTEGetFunctor< std::string >
    {
		functor_getter_home_url(const LLMediaEntry& entry): mMediaEntry(entry) {}		
		
        std::string get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getHomeURL();
            return mMediaEntry.getHomeURL();
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_home_url(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_home_url, value_str );
    base_key = std::string( LLMediaEntry::HOME_URL_KEY );
    mMediaSettings[ base_key ] = value_str;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Current URL
    value_str = default_media_data.getCurrentURL();
    struct functor_getter_current_url : public LLSelectedTEGetFunctor< std::string >
    {
		functor_getter_current_url(const LLMediaEntry& entry): mMediaEntry(entry) {}
        
		std::string get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getCurrentURL();
            return mMediaEntry.getCurrentURL();
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_current_url(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_current_url, value_str );
    base_key = std::string( LLMediaEntry::CURRENT_URL_KEY );
    mMediaSettings[ base_key ] = value_str;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Auto zoom
    value_bool = default_media_data.getAutoZoom();
    struct functor_getter_auto_zoom : public LLSelectedTEGetFunctor< bool >
    {
		
		functor_getter_auto_zoom(const LLMediaEntry& entry)	: mMediaEntry(entry) {}	
		
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getAutoZoom();
            return mMediaEntry.getAutoZoom();
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_auto_zoom(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_auto_zoom, value_bool );
    base_key = std::string( LLMediaEntry::AUTO_ZOOM_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Auto play
    //value_bool = default_media_data.getAutoPlay();
	// set default to auto play TRUE -- angela  EXT-5172
	value_bool = true;
    struct functor_getter_auto_play : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_auto_play(const LLMediaEntry& entry)	: mMediaEntry(entry) {}	
			
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getAutoPlay();
            //return mMediaEntry.getAutoPlay(); set default to auto play TRUE -- angela  EXT-5172
			return true;
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_auto_play(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_auto_play, value_bool );
    base_key = std::string( LLMediaEntry::AUTO_PLAY_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
	
    // Auto scale
	// set default to auto scale TRUE -- angela  EXT-5172
    //value_bool = default_media_data.getAutoScale();
	value_bool = true;
    struct functor_getter_auto_scale : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_auto_scale(const LLMediaEntry& entry): mMediaEntry(entry) {}	

        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getAutoScale();
           // return mMediaEntry.getAutoScale();  set default to auto scale TRUE -- angela  EXT-5172
			return true;
		};
		
		const LLMediaEntry &mMediaEntry;
		
    } func_auto_scale(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_auto_scale, value_bool );
    base_key = std::string( LLMediaEntry::AUTO_SCALE_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Auto loop
    value_bool = default_media_data.getAutoLoop();
    struct functor_getter_auto_loop : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_auto_loop(const LLMediaEntry& entry)	: mMediaEntry(entry) {}	

        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getAutoLoop();
            return mMediaEntry.getAutoLoop();
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_auto_loop(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_auto_loop, value_bool );
    base_key = std::string( LLMediaEntry::AUTO_LOOP_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // width pixels (if not auto scaled)
    value_int = default_media_data.getWidthPixels();
    struct functor_getter_width_pixels : public LLSelectedTEGetFunctor< int >
    {
		functor_getter_width_pixels(const LLMediaEntry& entry): mMediaEntry(entry) {}		

        int get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getWidthPixels();
            return mMediaEntry.getWidthPixels();
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_width_pixels(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_width_pixels, value_int );
    base_key = std::string( LLMediaEntry::WIDTH_PIXELS_KEY );
    mMediaSettings[ base_key ] = value_int;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // height pixels (if not auto scaled)
    value_int = default_media_data.getHeightPixels();
    struct functor_getter_height_pixels : public LLSelectedTEGetFunctor< int >
    {
		functor_getter_height_pixels(const LLMediaEntry& entry)	: mMediaEntry(entry) {}
        
		int get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getHeightPixels();
            return mMediaEntry.getHeightPixels();
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_height_pixels(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_height_pixels, value_int );
    base_key = std::string( LLMediaEntry::HEIGHT_PIXELS_KEY );
    mMediaSettings[ base_key ] = value_int;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Enable Alt image
    value_bool = default_media_data.getAltImageEnable();
    struct functor_getter_enable_alt_image : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_enable_alt_image(const LLMediaEntry& entry): mMediaEntry(entry) {}
        
		bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getAltImageEnable();
            return mMediaEntry.getAltImageEnable();
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_enable_alt_image(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_enable_alt_image, value_bool );
    base_key = std::string( LLMediaEntry::ALT_IMAGE_ENABLE_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - owner interact
    value_bool = 0 != ( default_media_data.getPermsInteract() & LLMediaEntry::PERM_OWNER );
    struct functor_getter_perms_owner_interact : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_perms_owner_interact(const LLMediaEntry& entry): mMediaEntry(entry) {}
        
		bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsInteract() & LLMediaEntry::PERM_OWNER));
            return 0 != ( mMediaEntry.getPermsInteract() & LLMediaEntry::PERM_OWNER );
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_perms_owner_interact(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_perms_owner_interact, value_bool );
    base_key = std::string( LLPanelContents::PERMS_OWNER_INTERACT_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - owner control
    value_bool = 0 != ( default_media_data.getPermsControl() & LLMediaEntry::PERM_OWNER );
    struct functor_getter_perms_owner_control : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_perms_owner_control(const LLMediaEntry& entry)	: mMediaEntry(entry) {}
        
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsControl() & LLMediaEntry::PERM_OWNER));
            return 0 != ( mMediaEntry.getPermsControl() & LLMediaEntry::PERM_OWNER );
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_perms_owner_control(default_media_data);
    identical = selected_objects ->getSelectedTEValue( &func_perms_owner_control, value_bool );
    base_key = std::string( LLPanelContents::PERMS_OWNER_CONTROL_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - group interact
    value_bool = 0 != ( default_media_data.getPermsInteract() & LLMediaEntry::PERM_GROUP );
    struct functor_getter_perms_group_interact : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_perms_group_interact(const LLMediaEntry& entry): mMediaEntry(entry) {}
        
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsInteract() & LLMediaEntry::PERM_GROUP));
            return 0 != ( mMediaEntry.getPermsInteract() & LLMediaEntry::PERM_GROUP );
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_perms_group_interact(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_perms_group_interact, value_bool );
    base_key = std::string( LLPanelContents::PERMS_GROUP_INTERACT_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - group control
    value_bool = 0 != ( default_media_data.getPermsControl() & LLMediaEntry::PERM_GROUP );
    struct functor_getter_perms_group_control : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_perms_group_control(const LLMediaEntry& entry): mMediaEntry(entry) {}
        
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsControl() & LLMediaEntry::PERM_GROUP));
            return 0 != ( mMediaEntry.getPermsControl() & LLMediaEntry::PERM_GROUP );
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_perms_group_control(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_perms_group_control, value_bool );
    base_key = std::string( LLPanelContents::PERMS_GROUP_CONTROL_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - anyone interact
    value_bool = 0 != ( default_media_data.getPermsInteract() & LLMediaEntry::PERM_ANYONE );
    struct functor_getter_perms_anyone_interact : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_perms_anyone_interact(const LLMediaEntry& entry): mMediaEntry(entry) {}
        
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsInteract() & LLMediaEntry::PERM_ANYONE));
            return 0 != ( mMediaEntry.getPermsInteract() & LLMediaEntry::PERM_ANYONE );
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_perms_anyone_interact(default_media_data);
    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func_perms_anyone_interact, value_bool );
    base_key = std::string( LLPanelContents::PERMS_ANYONE_INTERACT_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // Perms - anyone control
    value_bool = 0 != ( default_media_data.getPermsControl() & LLMediaEntry::PERM_ANYONE );
    struct functor_getter_perms_anyone_control : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_perms_anyone_control(const LLMediaEntry& entry)	: mMediaEntry(entry) {}
        
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return (0 != (object->getTE(face)->getMediaData()->getPermsControl() & LLMediaEntry::PERM_ANYONE));
            return 0 != ( mMediaEntry.getPermsControl() & LLMediaEntry::PERM_ANYONE );
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_perms_anyone_control(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_perms_anyone_control, value_bool );
    base_key = std::string( LLPanelContents::PERMS_ANYONE_CONTROL_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // security - whitelist enable
    value_bool = default_media_data.getWhiteListEnable();
    struct functor_getter_whitelist_enable : public LLSelectedTEGetFunctor< bool >
    {
		functor_getter_whitelist_enable(const LLMediaEntry& entry)	: mMediaEntry(entry) {}
        
        bool get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getWhiteListEnable();
            return mMediaEntry.getWhiteListEnable();
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_whitelist_enable(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_whitelist_enable, value_bool );
    base_key = std::string( LLMediaEntry::WHITELIST_ENABLE_KEY );
    mMediaSettings[ base_key ] = value_bool;
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
	
    // security - whitelist URLs
    std::vector<std::string> value_vector_str = default_media_data.getWhiteList();
    struct functor_getter_whitelist_urls : public LLSelectedTEGetFunctor< std::vector<std::string> >
    {
		functor_getter_whitelist_urls(const LLMediaEntry& entry): mMediaEntry(entry) {}
        
        std::vector<std::string> get( LLViewerObject* object, S32 face )
        {
            if ( object )
                if ( object->getTE(face) )
                    if ( object->getTE(face)->getMediaData() )
                        return object->getTE(face)->getMediaData()->getWhiteList();
            return mMediaEntry.getWhiteList();
        };
		
		const LLMediaEntry &mMediaEntry;
		
    } func_whitelist_urls(default_media_data);
    identical = selected_objects->getSelectedTEValue( &func_whitelist_urls, value_vector_str );
    base_key = std::string( LLMediaEntry::WHITELIST_KEY );
	mMediaSettings[ base_key ].clear();
    std::vector< std::string >::iterator iter = value_vector_str.begin();
    while( iter != value_vector_str.end() )
    {
        std::string white_list_url = *iter;
        mMediaSettings[ base_key ].append( white_list_url );
        ++iter;
    };
	
    mMediaSettings[ base_key + std::string( LLPanelContents::TENTATIVE_SUFFIX ) ] = ! identical;
}

void LLFloaterTools::onSelectTreesGrass()
{
	const std::string& selected = mComboTreesGrass->getValue();
	LLPCode pcode = LLToolPlacer::getObjectType();
	if (pcode == LL_PCODE_LEGACY_TREE)
	{
		gSavedSettings.setString("LastTree", selected);
	}
	else if (pcode == LL_PCODE_LEGACY_GRASS)
	{
		gSavedSettings.setString("LastGrass", selected);
	}
}

void LLFloaterTools::updateTreeGrassCombo(bool visible)
{
	LLTextBox* tree_grass_label = getChild<LLTextBox>("tree_grass_label");
	if (visible)
	{
		LLPCode pcode = LLToolPlacer::getObjectType();
		std::map<std::string, S32>::iterator it, end;
		std::string selected;
		if (pcode == LL_PCODE_LEGACY_TREE)
		{
			tree_grass_label->setVisible(visible);
			LLButton* button = getChild<LLButton>("ToolTree");
			tree_grass_label->setText(button->getToolTip());

			selected = gSavedSettings.getString("LastTree");
			it = LLVOTree::sSpeciesNames.begin();
			end = LLVOTree::sSpeciesNames.end();
		}
		else if (pcode == LL_PCODE_LEGACY_GRASS)
		{
			tree_grass_label->setVisible(visible);
			LLButton* button = getChild<LLButton>("ToolGrass");
			tree_grass_label->setText(button->getToolTip());

			selected = gSavedSettings.getString("LastGrass");
			it = LLVOGrass::sSpeciesNames.begin();
			end = LLVOGrass::sSpeciesNames.end();
		}
		else
		{
			mComboTreesGrass->removeall();
			mComboTreesGrass->setLabel(LLStringExplicit(""));  // LLComboBox::removeall() does not clear the label
			mComboTreesGrass->setEnabled(false);
			mComboTreesGrass->setVisible(false);
			tree_grass_label->setVisible(false);
			return;
		}

		mComboTreesGrass->removeall();
		mComboTreesGrass->add(getString("Random"));

		int select = 0, i = 0;

		while (it != end)
		{
			const std::string &species = it->first;
			mComboTreesGrass->add(species);  ++i;
			if (species == selected) select = i;
			++it;
		}
		// if saved species not found, default to "Random"
		mComboTreesGrass->selectNthItem(select);
		mComboTreesGrass->setEnabled(true);
	}
	
	mComboTreesGrass->setVisible(visible);
	tree_grass_label->setVisible(visible);
}

