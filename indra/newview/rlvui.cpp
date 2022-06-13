/** 
 *
 * Copyright (c) 2009-2011, Kitty Barnett
 * 
 * The source code in this file is provided to you under the terms of the 
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt 
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 * 
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to 
 * abide by those obligations.
 * 
 */

#include "llviewerprecompiledheaders.h"
#include "llagent.h"
#include "llavataractions.h"			// LLAvatarActions::profileVisible()
#include "llavatarnamecache.h"
#include "llenvmanager.h"
#include "llhudtext.h"					// LLHUDText::refreshAllObjectText()
#include "llimview.h"					// LLIMMgr::computeSessionID()
#include "llmoveview.h"					// Movement panel (contains "Stand" and "Stop Flying" buttons)
#include "llparcel.h"
#include "lltabcontainer.h"
#include "lltoolmgr.h"
#include "llviewerparcelmgr.h"
#include "llvoavatar.h"
#include "roles_constants.h"			// Group "powers"

#include "lffloaterinvpanel.h"
#include "llfloaterbeacons.h"
#include "llfloatertools.h"
#include "llfloaterenvsettings.h"
#include "llfloaterwindlight.h"
#include "llfloaterwater.h"
#include "llfloaterdaycycle.h"
#include "llagentcamera.h"
#include "llviewerwindow.h"
#include "llpanelmaininventory.h"
#include "llappviewer.h"
#include "llfloaterland.h"
#include "llfloatergodtools.h"
#include "llfloaterregioninfo.h"
#include "llfloatermap.h"
#include "llfloaterchat.h"
#include "llfloateravatarlist.h"
#include "llfloaterworldmap.h"
#include "llmenugl.h"
#include "lltoolbar.h"
#include "lluictrlfactory.h"
#include "llviewerregion.h"

#include "rlvui.h"
#include "rlvhandler.h"
#include "rlvextensions.h"

// ============================================================================

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
RlvUIEnabler::RlvUIEnabler()
{
	// Connect us to the behaviour toggle signal
	gRlvHandler.setBehaviourToggleCallback(boost::bind(&RlvUIEnabler::onBehaviourToggle, this, _1, _2));

	// onRefreshHoverText()
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWLOC, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWNAMES, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWNAMETAGS, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWHOVERTEXTALL, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWHOVERTEXTWORLD, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWHOVERTEXTHUD, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));

	// onToggleMovement
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_FLY, boost::bind(&RlvUIEnabler::onToggleMovement, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_ALWAYSRUN, boost::bind(&RlvUIEnabler::onToggleMovement, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_TEMPRUN, boost::bind(&RlvUIEnabler::onToggleMovement, this)));

	// onToggleViewXXX
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_VIEWNOTE, boost::bind(&RlvUIEnabler::onToggleViewXXX, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_VIEWSCRIPT, boost::bind(&RlvUIEnabler::onToggleViewXXX, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_VIEWTEXTURE, boost::bind(&RlvUIEnabler::onToggleViewXXX, this)));

	// onToggleXXX
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_EDIT, boost::bind(&RlvUIEnabler::onToggleEdit, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SENDIM, boost::bind(&RlvUIEnabler::onToggleSendIM, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SETDEBUG, boost::bind(&RlvUIEnabler::onToggleSetDebug, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SETENV, boost::bind(&RlvUIEnabler::onToggleSetEnv, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWINV, boost::bind(&RlvUIEnabler::onToggleShowInv, this, _1)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWLOC, boost::bind(&RlvUIEnabler::onToggleShowLoc, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWMINIMAP, boost::bind(&RlvUIEnabler::onToggleShowMinimap, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWNAMES, boost::bind(&RlvUIEnabler::onToggleShowNames, this, _1)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWNAMETAGS, boost::bind(&RlvUIEnabler::onToggleShowNameTags, this, _1)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWWORLDMAP, boost::bind(&RlvUIEnabler::onToggleShowWorldMap, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_UNSIT, boost::bind(&RlvUIEnabler::onToggleUnsit, this)));

	// onToggleCamXXX
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_CAMUNLOCK, boost::bind(&RlvUIEnabler::onToggleCamUnlock, this)));
	void onToggleCamZoom(const ERlvBehaviour& eBhvr);
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_CAMZOOMMAX, boost::bind(onToggleCamZoom, RLV_BHVR_CAMZOOMMAX)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_CAMZOOMMIN, boost::bind(onToggleCamZoom, RLV_BHVR_CAMZOOMMIN)));
	void onToggleCamDist(const ERlvBehaviour& eBhvr);
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_CAMDISTMAX, boost::bind(onToggleCamDist, RLV_BHVR_CAMDISTMAX)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_CAMDISTMIN, boost::bind(onToggleCamDist, RLV_BHVR_CAMDISTMIN)));

	// onToggleTp
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_TPLOC, boost::bind(&RlvUIEnabler::onToggleTp, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_TPLM, boost::bind(&RlvUIEnabler::onToggleTp, this)));

	// onUpdateLoginLastLocation
	#ifdef RLV_EXTENSION_STARTLOCATION
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_TPLOC, boost::bind(&RlvUIEnabler::onUpdateLoginLastLocation, this, _1)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_UNSIT, boost::bind(&RlvUIEnabler::onUpdateLoginLastLocation, this, _1)));
	#endif // RLV_EXTENSION_STARTLOCATION
}

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::onBehaviourToggle(ERlvBehaviour eBhvr, ERlvParamType eType)
{
	bool fQuitting = LLApp::isQuitting();
	for (behaviour_handler_map_t::const_iterator itHandler = m_Handlers.lower_bound(eBhvr), endHandler = m_Handlers.upper_bound(eBhvr);
			itHandler != endHandler; ++itHandler)
	{
		itHandler->second(fQuitting);
	}
}

// ============================================================================

// Checked: 2010-03-02 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::onRefreshHoverText()
{
	// Refresh all hover text each time any of the monitored behaviours get set or unset
	LLHUDText::refreshAllObjectText();
}

// Checked: 2010-03-17 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleEdit()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_EDIT);

	if (!fEnable)
	{
		// Turn off "View / Highlight Transparent"
		LLDrawPoolAlpha::sShowDebugAlpha = FALSE;

		// Hide the Beacons floater if it's currently visible
		if (LLFloaterBeacons::instanceVisible())
			LLFloaterBeacons::toggleInstance();

		// Get rid of the build floater if it's open [copy/paste from toggle_build_mode()]
		if (gFloaterTools->getVisible())
		{
			gAgentCamera.resetView(FALSE);
			gFloaterTools->close();
			gViewerWindow->showCursor();
		}

	}
}

// Checked: 2010-03-02 (RLVa-1.4.0a) | Modified: RLVa-1.4.0a
void RlvUIEnabler::onToggleMovement()
{
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_FLY)) && (gAgent.getFlying()) )
		gAgent.setFlying(FALSE);
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_ALWAYSRUN)) && (gAgent.getAlwaysRun()) )
		gAgent.clearAlwaysRun();
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_TEMPRUN)) && (gAgent.getTempRun()) )
		gAgent.clearTempRun();
}

// Checked: 2013-05-11 (RLVa-1.4.9)
void RlvUIEnabler::onToggleSendIM()
{
	/* Singu Note: Don't hide settings just because they won't be used, that's just wrong.
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SENDIM);
	gSavedPerAccountSettings.getControl("BusyModeResponse")->setHiddenFromSettingsEditor(!fEnable);
	*/
}

// Checked: 2011-05-28 (RLVa-1.4.0a) | Added: RLVa-1.4.0a
void RlvUIEnabler::onToggleSetDebug()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SETDEBUG);
	for (std::map<std::string, S16>::const_iterator itSetting = RlvExtGetSet::m_DbgAllowed.begin(); 
			itSetting != RlvExtGetSet::m_DbgAllowed.end(); ++itSetting)
	{
		if (itSetting->second & RlvExtGetSet::DBG_WRITE)
			gSavedSettings.getControl(itSetting->first)->setHiddenFromSettingsEditor(!fEnable);
	}
}

// Checked: 2011-09-04 (RLVa-1.4.1a) | Modified: RLVa-1.4.1a
void RlvUIEnabler::onToggleSetEnv()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV);

	if (!fEnable)
	{
		// Only close the floaters if their instance exists and they're actually visible
		if ( (LLFloaterEnvSettings::isOpen()) )
			LLFloaterEnvSettings::instance()->close();
		if ( (LLFloaterWindLight::isOpen()) )
			LLFloaterWindLight::instance()->close();
		if ( (LLFloaterWater::isOpen()) )
			LLFloaterWater::instance()->close();
		if ( (LLFloaterDayCycle::isOpen()) )
			LLFloaterDayCycle::instance()->close();
	}

	// Don't allow toggling "Basic Shaders" and/or "Atmopsheric Shaders" through the debug settings under @setenv=n
	gSavedSettings.getControl("VertexShaderEnable")->setHiddenFromSettingsEditor(!fEnable);
	gSavedSettings.getControl("WindLightUseAtmosShaders")->setHiddenFromSettingsEditor(!fEnable);

	// Restore the user's WindLight preferences when releasing
	if (fEnable)
		LLEnvManagerNew::instance().usePrefs();
}

// Checked: 2011-11-04 (RLVa-1.4.4a) | Modified: RLVa-1.4.4a
void RlvUIEnabler::onToggleShowInv(bool fQuitting)
{
	if (fQuitting)
		return;	// Nothing to do if the viewer is shutting down

	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWINV);

	//
	// When disabling, close any inventory floaters that may be open
	//
	if (!fEnable)
	{
		LLPanelMainInventory::closeAll();
		LFFloaterInvPanel::closeAll();

		// Singu Note: Hide our standalone inventory favorites floater if present
		if (LLFloater* floater = LLUICtrlFactory::getInstance()->getBuiltFloater("floater_inventory_favs.xml"))
			floater->close();
	}

	//
	// Enable/disable the "My Outfits" panel on the "My Appearance" sidebar tab
	//
	LLFloater* pAppearancePanel = LLUICtrlFactory::getInstance()->getBuiltFloater("floater_my_outfits.xml");
	if (pAppearancePanel)
	{
		// Singu Note: No RLV_ASSERTs. This is all a lie about our standalone My Outfits floater anyway.
		if (!fEnable) pAppearancePanel->close();
	}

	//
	// Filter (or stop filtering) opening new inventory floaters
	//
	if (!fEnable)
	{
		LLMenuGL::sMenuContainer->childSetEnabled("My Outfits", false);
		LLMenuGL::sMenuContainer->childSetEnabled("Favorites", false);
	}
	else
	{
		LLMenuGL::sMenuContainer->childSetEnabled("My Outfits", true);
		LLMenuGL::sMenuContainer->childSetEnabled("Favorites", true);
	}
	gToolBar->childSetEnabled("outfits_btn", fEnable);
	gToolBar->childSetEnabled("favs_btn", fEnable);
}

// Checked: 2010-04-22 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
void RlvUIEnabler::onToggleShowLoc()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);

	/* Singu TODO: LLNavigationBar
	if (LLNavigationBar::instanceExists())
		LLNavigationBar::instance().refreshLocationCtrl();
	if (LLPanelTopInfoBar::instanceExists())
		LLPanelTopInfoBar::instance().update();
	*/

	if (!fEnable)
	{
		// Hide the "About Land" floater if it's currently visible
		if (LLFloaterLand::instanceVisible())
			LLFloaterLand::hideInstance();

		// Hide the "Region / Estate" floater if it's currently visible
		if (LLFloaterRegionInfo::instanceVisible())
			LLFloaterRegionInfo::hideInstance();
		// Hide the "God Tools" floater if it's currently visible
		LLFloaterGodTools::hide();
	}
}

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleShowMinimap()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWMINIMAP);

	// Start or stop filtering showing the mini-map floater
	if ((!fEnable) && LLFloaterMap::instanceVisible())
		LLFloaterMap::hideInstance();
}

// Checked: 2010-12-08 (RLVa-1.4.0a) | Modified: RLVa-1.2.2c
void RlvUIEnabler::onToggleShowNames(bool fQuitting)
{
	if (fQuitting)
		return;							// Nothing to do if the viewer is shutting down

	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES);

	// Force the use of the "display name" cache so we can filter both display and legacy names (or return back to the user's preference)
	if (!fEnable)
	{
		// Close the "Active Speakers" panel if it's currently visible
		LLFloaterChat::getInstance()->childSetVisible("active_speakers_panel", false);

		LLAvatarNameCache::setForceDisplayNames(true);
	}
	else
	{
		LLAvatarNameCache::setForceDisplayNames(false);
		const S32 namesys = gSavedSettings.getS32("PhoenixNameSystem");
		LLAvatarNameCache::setUseDisplayNames(namesys > 0 && namesys < 4);
	}

	// Reset the names in radar, if it exists
	if (LLFloaterAvatarList::instanceExists())
		LLFloaterAvatarList::instance().resetAvatarNames();

	LLVOAvatar::invalidateNameTags();	// See handleDisplayNamesOptionChanged()
}

// Checked: 2015-05-20 (RLVa:LF)
void RlvUIEnabler::onToggleShowNameTags(bool fQuitting)
{
	if (fQuitting) return; // Nothing to do if the viewer is shutting down

	// Reset the names in radar, if it exists
	if (LLFloaterAvatarList::instanceExists())
		LLFloaterAvatarList::instance().resetAvatarNames();

	LLVOAvatar::invalidateNameTags();	// See handleDisplayNamesOptionChanged()
}

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleShowWorldMap()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWWORLDMAP);

	// Simulate clicking the Map button [see LLToolBar::onClickMap()]
	if ((!fEnable) && gFloaterWorldMap->getVisible())
		LLFloaterWorldMap::toggle();
}

// Checked: 2015-05-25 (RLVa:LF)
void RlvUIEnabler::onToggleCamUnlock()
{
	if (gRlvHandler.hasBehaviour(RLV_BHVR_CAMUNLOCK))
		gAgentCamera.resetView(true, true);
}

// Checked: 2015-06-04 (RLVa:LF)
void onToggleCamZoom(const ERlvBehaviour& eBhvr)
{
	if (gRlvHandler.hasBehaviour(eBhvr))
	{
		LLViewerCamera& inst(LLViewerCamera::instance());
		inst.mSavedFOVLoaded ? inst.loadDefaultFOV() : inst.setDefaultFOV(gSavedSettings.getF32("CameraAngle"));
	}
}

// Checked: 2015-06-04 (RLVa:LF)
void onToggleCamDist(const ERlvBehaviour& eBhvr)
{
	if (gRlvHandler.hasBehaviour(eBhvr))
	{
		if (eBhvr == RLV_BHVR_CAMDISTMAX && gRlvHandler.camPole(eBhvr) <= 0)
		{
			if (!gAgentCamera.cameraMouselook())
				gAgentCamera.changeCameraToMouselook();
		}
		else
		{
			if (eBhvr == RLV_BHVR_CAMDISTMIN && gRlvHandler.camPole(eBhvr) > 0 && gAgentCamera.cameraMouselook())
				gAgentCamera.changeCameraToDefault();
			gAgentCamera.cameraPanUp(0); // Hacky, but meh, issues a proper call to calcCameraPositionTargetGlobal().
		}
	}
}

// Checked: 2010-08-22 (RLVa-1.2.1a) | Added: RLVa-1.2.1a
void RlvUIEnabler::onToggleTp()
{
/* Singu TODO: LLNavigationBar
	// Disable the navigation bar "Home" button if both @tplm=n *and* @tploc=n restricted
	LLButton* pNavBarHomeBtn = LLNavigationBar::getInstance()->findChild<LLButton>("home_btn");
	RLV_ASSERT(pNavBarHomeBtn);
	if (pNavBarHomeBtn)
		pNavBarHomeBtn->setEnabled(!(gRlvHandler.hasBehaviour(RLV_BHVR_TPLM) && gRlvHandler.hasBehaviour(RLV_BHVR_TPLOC)));
*/
}

// Checked: 2010-03-01 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleUnsit()
{
/* Singu TODO?
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT);

	LLPanelStandStopFlying* pPanelStand = LLPanelStandStopFlying::getInstance();
	RLV_ASSERT(pPanelStand);
	if (pPanelStand)
	{
		LLButton* pBtnStand = pPanelStand->findChild<LLButton>("stand_btn");
		RLV_ASSERT(pBtnStand);
		if (pBtnStand)
			pBtnStand->setEnabled(fEnable);
	}
*/
}

// Checked: 2010-03-01 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleViewXXX()
{
/* Singu TODO?
	// If any of the three are still active then we keep filtering
	bool fHasViewXXX = (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE)) ||
		(gRlvHandler.hasBehaviour(RLV_BHVR_VIEWSCRIPT)) || (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE));

	// Start or stop filtering opening the preview floaters
	if ( (fHasViewXXX) && (!m_ConnFloaterViewXXX.connected()) )
		m_ConnFloaterViewXXX = LLFloaterReg::setValidateCallback(boost::bind(&RlvUIEnabler::filterFloaterViewXXX, this, _1, _2));
	else if ( (!fHasViewXXX) && (m_ConnFloaterViewXXX.connected()) )
		m_ConnFloaterViewXXX.disconnect();
*/
}

// Checked: 2010-04-01 (RLVa-1.2.0c) | Added: RLVa-1.2.0c
void RlvUIEnabler::onUpdateLoginLastLocation(bool fQuitting)
{
	if (!fQuitting)
		RlvSettings::updateLoginLastLocation();
}

// ============================================================================

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
/*void RlvUIEnabler::addGenericFloaterFilter(const std::string& strFloaterName)
{
	m_FilteredFloaters.insert(strFloaterName);

	if (!m_ConnFloaterGeneric.connected())
		m_ConnFloaterGeneric = LLFloaterReg::setValidateCallback(boost::bind(&RlvUIEnabler::filterFloaterGeneric, this, _1, _2));
}

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::removeGenericFloaterFilter(const std::string& strFloaterName)
{
	std::multiset<std::string>::iterator itFloater = m_FilteredFloaters.find(strFloaterName);
	RLV_ASSERT_DBG(itFloater != m_FilteredFloaters.end());
	m_FilteredFloaters.erase(itFloater);

	RLV_ASSERT_DBG(m_ConnFloaterGeneric.connected());
	if (m_FilteredFloaters.empty())
		m_ConnFloaterGeneric.disconnect();
}

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
bool RlvUIEnabler::filterFloaterGeneric(const std::string& strName, const LLSD&)
{
	return m_FilteredFloaters.end() == m_FilteredFloaters.find(strName);
}

// Checked: 2010-04-22 (RLVa-1.4.5) | Added: RLVa-1.2.0
bool RlvUIEnabler::filterFloaterShowLoc(const std::string& strName, const LLSD&)
{
	if ("about_land" == strName)
		return canViewParcelProperties();
	else if ("region_info" == strName)
		return canViewRegionProperties();
	else if ("god_tools" == strName)
		return false;
	return true;
}

// Checked: 2012-02-07 (RLVa-1.4.5) | Added: RLVa-1.4.5
bool RlvUIEnabler::filterPanelShowLoc(const std::string& strFloater, const std::string&, const LLSD& sdKey)
{
	if ("places" == strFloater)
	{
		const std::string strType = sdKey["type"].asString();
		if ("create_landmark" == strType)
			return false;
		else if ("agent" == strType)
			return canViewParcelProperties();
	}
	return true;
}

// Checked: 2010-03-01 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
bool RlvUIEnabler::filterFloaterViewXXX(const std::string& strName, const LLSD&)
{
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE)) && ("preview_notecard" == strName) )
	{
		RlvUtil::notifyBlockedViewXXX(LLAssetType::AT_NOTECARD);
		return false;
	}
	else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWSCRIPT)) && (("preview_script" == strName) || ("preview_scriptedit" == strName)) )
	{
		RlvUtil::notifyBlockedViewXXX(LLAssetType::AT_SCRIPT);
		return false;
	}
	else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE)) && ("preview_texture" == strName) )
	{
		RlvUtil::notifyBlockedViewXXX(LLAssetType::AT_TEXTURE);
		return false;
	}
	return true;
}
*/

// ============================================================================

// Checked: 2012-02-09 (RLVa-1.4.5) | Modified: RLVa-1.4.5
bool RlvUIEnabler::canViewParcelProperties()
{
	// We'll allow "About Land" as long as the user has the ability to return prims (through ownership or through group powers)
	bool fShow = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
	{
		// RELEASE-RLVa: [SL-3.2] Check that opening the "About Land" floater still sets focus to the current parcel is none is selected
		const LLParcel* pParcel = NULL;
		if (LLViewerParcelMgr::getInstance()->selectionEmpty())
		{
			pParcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		}
		else
		{
			LLParcelSelection* pParcelSel = LLViewerParcelMgr::getInstance()->getFloatingParcelSelection();
			if (pParcelSel->hasOthersSelected())
				return false;
			pParcel = pParcelSel->getParcel();
		}

		// Ideally we could just use LLViewerParcelMgr::isParcelOwnedByAgent(), but that has that sneaky exemption for "god-like"
		if (pParcel)
		{
			const LLUUID& idOwner = pParcel->getOwnerID();
			if ( (idOwner != gAgent.getID()) )
			{
				S32 count = gAgent.mGroups.size();
				for (S32 i = 0; i < count; ++i)
				{
					if (gAgent.mGroups[i].mID == idOwner)
					{
						fShow = ((gAgent.mGroups[i].mPowers & GP_LAND_RETURN) > 0);
						break;
					}
				}
			}
			else
			{
				fShow = true;
			}
		}
	}
	return fShow;
}

// Checked: 2010-04-20 (RLVa-1.4.5) | Modified: RLVa-1.2.0
bool RlvUIEnabler::canViewRegionProperties()
{
	// We'll allow "Region / Estate" if the user is either the region owner or an estate manager
	bool fShow = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
	{
		// [See LLRegion::canManageEstate() but without the "god-like" exception]
		const LLViewerRegion* pRegion = gAgent.getRegion();
		if (pRegion)
			fShow = (pRegion->isEstateManager()) || (pRegion->getOwner() == gAgent.getID());
	}
	return fShow;
}

// Checked: 2010-04-20 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
bool RlvUIEnabler::hasOpenIM(const LLUUID& idAgent)
{
	LLUUID idSession = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, idAgent);
	return gIMMgr->hasSession(idSession);
}

// Checked: 2011-11-04 (RLVa-1.4.4a) | Modified: RLVa-1.4.4a
bool RlvUIEnabler::hasOpenProfile(const LLUUID& idAgent)
{
	//   -> SL-2.1.0 added the ability to "share" inventory by dropping it on the avatar picker floater so we should check for that
	//   -> we can drag/drop inventory onto calling cards but probably noone knows about it and it would be too involved to check for that
	// TODO-RLVa: [RLVa-1.2.1] Check the avatar picker as well

	// Check if the user has the specified agent's profile open
	return LLAvatarActions::profileVisible(idAgent);
}

// Checked: 2010-09-11 (RLVa-1.2.1d) | Added: RLVa-1.2.1d
bool RlvUIEnabler::isBuildEnabled()
{
	return (gAgent.canEditParcel()) && ((!gRlvHandler.hasBehaviour(RLV_BHVR_EDIT)) || (!gRlvHandler.hasBehaviour(RLV_BHVR_REZ)));
}

// ============================================================================
