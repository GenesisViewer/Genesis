/**
 * @file LLMediaCtrl.cpp
 * @brief Web browser UI control
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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


#include "llmediactrl.h"

// viewer includes
#include "llfloaterhtml.h"
#include "llfloaterworldmap.h"
#include "lluictrlfactory.h"
#include "llurldispatcher.h"
#include "llviewborder.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewertexture.h"
#include "llviewerwindow.h"
#include "llweb.h"
#include "llrender.h"
#include "llpluginclassmedia.h"
#include "llslurl.h"
#include "lluictrlfactory.h"	// LLRegisterWidget
#include "llkeyboard.h"
#include "llviewermenu.h"

// linden library includes
#include "llclipboard.h"
#include "llfocusmgr.h"
#include "llsdutil.h"
#include "lltextbox.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llnotifications.h"
#include "lllineeditor.h"

extern BOOL gRestoreGL;

static LLRegisterWidget<LLMediaCtrl> r("web_browser");

LLMediaCtrl::Params::Params()
:	start_url("start_url"),
	border_visible("border_visible", false),
	decouple_texture_size("decouple_texture_size", false),
	trusted_content("trusted_content", false),
	focus_on_click("focus_on_click", true),
	texture_width("texture_width", 1024),
	texture_height("texture_height", 1024),
	caret_color("caret_color"),
	initial_mime_type("initial_mime_type"),
	media_id("media_id"),
	error_page_url("error_page_url")
{
}

LLMediaCtrl::LLMediaCtrl( const Params& p) :
	LLPanel( p.name, p.rect, FALSE),
	LLInstanceTracker<LLMediaCtrl, LLUUID>(LLUUID::generateNewID()),
	mTextureDepthBytes( 4 ),
	mBorder(nullptr),
	mFrequentUpdates( true ),
	mForceUpdate( false ),
	mTrusted(p.trusted_content),
	mAlwaysRefresh( false ),
	mTakeFocusOnClick( p.focus_on_click ),
	mStretchToFill( true ),
	mMaintainAspectRatio ( true ),
	mClearCache(false),
	mHoverTextChanged(false),
	mDecoupleTextureSize ( false ),
	mUpdateScrolls( false ),
	mHomePageUrl( "" ),
	mHomePageMimeType(p.initial_mime_type),
	mCurrentNavUrl( "about:blank" ),
	mErrorPageURL(p.error_page_url),
	mMediaSource( nullptr ),
	mTextureWidth ( 1024 ),
	mTextureHeight ( 1024 ),
	mContextMenu()
{
	{
		LLColor4 color = p.caret_color().get();
		setCaretColor( (unsigned int)color.mV[0], (unsigned int)color.mV[1], (unsigned int)color.mV[2] );
	}

	setHomePageUrl(p.start_url, p.initial_mime_type);
	
	setBorderVisible(p.border_visible);
	
	setDecoupleTextureSize(p.decouple_texture_size);
	
	setTextureSize(p.texture_width, p.texture_height);

	if(!getDecoupleTextureSize())
	{
		S32 screen_width = ll_round((F32)getRect().getWidth() * LLUI::getScaleFactor().mV[VX]);
		S32 screen_height = ll_round((F32)getRect().getHeight() * LLUI::getScaleFactor().mV[VY]);
			
		setTextureSize(screen_width, screen_height);
	}
	
	mMediaTextureID = getKey();
	
	// We don't need to create the media source up front anymore unless we have a non-empty home URL to navigate to.
	/*if(!mHomePageUrl.empty())
	{
		navigateHome();
	}*/
		

	//LLRect border_rect( 0, getRect().getHeight() + 2, getRect().getWidth() + 2, 0 );
}

LLMediaCtrl::~LLMediaCtrl()
{
	auto menu = mContextMenu.get();
	if (menu)
	{
		menu->die();
		mContextMenu.markDead();
	}

	if (mMediaSource)
	{
		mMediaSource->remObserver( this );
		mMediaSource = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::setBorderVisible( BOOL border_visible )
{
	if(border_visible && !mBorder)
	{
		mBorder = new LLViewBorder( std::string("web control border"), getLocalRect(), LLViewBorder::BEVEL_IN );
		addChild( mBorder );
	}
	if(mBorder)
	{
		mBorder->setVisible(border_visible);
	}
};

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::setTakeFocusOnClick( bool take_focus )
{
	mTakeFocusOnClick = take_focus;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleHover( S32 x, S32 y, MASK mask )
{
	if (LLPanel::handleHover(x, y, mask)) return TRUE;
	convertInputCoords(x, y);

	if (mMediaSource)
	{
		mMediaSource->mouseMove(x, y, mask);
		gViewerWindow->setCursor(mMediaSource->getLastSetCursor());
	}
	
	// TODO: Is this the right way to handle hover text changes driven by the plugin?
	if(mHoverTextChanged)
	{
		mHoverTextChanged = false;
		//handleToolTip(x, y, mask);
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleScrollWheel( S32 x, S32 y, S32 clicks )
{
	if (LLPanel::handleScrollWheel(x, y, clicks)) return TRUE;
	if (mMediaSource && mMediaSource->hasMedia())
	{
		convertInputCoords(x, y);
		mMediaSource->scrollWheel(x, y, 0, clicks, gKeyboard->currentMask(TRUE));
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//	virtual 
BOOL LLMediaCtrl::handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect_screen)
{
	std::string hover_text;
	
	if (mMediaSource && mMediaSource->hasMedia())
		hover_text = mMediaSource->getMediaPlugin()->getHoverText();
	
	if(hover_text.empty())
	{
		return FALSE;
	}
	else
	{
		msg = hover_text;

		S32 screen_x, screen_y;

		localPointToScreen(x, y, &screen_x, &screen_y);
		LLRect sticky_rect_screen;
		sticky_rect_screen.setCenterAndSize(screen_x, screen_y, 20, 20);
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleMouseUp( S32 x, S32 y, MASK mask )
{
	if (LLPanel::handleMouseUp(x, y, mask)) return TRUE;
	convertInputCoords(x, y);

	if (mMediaSource)
	{
		mMediaSource->mouseUp(x, y, mask);
	}
	
	gFocusMgr.setMouseCapture(nullptr );

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleMouseDown( S32 x, S32 y, MASK mask )
{
	if (LLPanel::handleMouseDown(x, y, mask)) return TRUE;
	convertInputCoords(x, y);

	if (mMediaSource)
		mMediaSource->mouseDown(x, y, mask);
	
	gFocusMgr.setMouseCapture( this );

	if (mTakeFocusOnClick)
	{
		setFocus( TRUE );
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleRightMouseUp( S32 x, S32 y, MASK mask )
{
	if (LLPanel::handleRightMouseUp(x, y, mask)) return TRUE;
	convertInputCoords(x, y);

	if (mMediaSource)
	{
		mMediaSource->mouseUp(x, y, mask, 1);

		// *HACK: LLMediaImplLLMozLib automatically takes focus on mouseup,
		// in addition to the onFocusReceived() call below.  Undo this. JC
		if (!mTakeFocusOnClick)
		{
			mMediaSource->focus(false);
			gViewerWindow->focusClient();
		}
	}
	
	gFocusMgr.setMouseCapture(nullptr );

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleRightMouseDown( S32 x, S32 y, MASK mask )
{
	if (LLPanel::handleRightMouseDown(x, y, mask)) return TRUE;

	S32 media_x = x, media_y = y;
	convertInputCoords(media_x, media_y);

	if (mMediaSource)
		mMediaSource->mouseDown(media_x, media_y, mask, 1);
	
	gFocusMgr.setMouseCapture( this );

	if (mTakeFocusOnClick)
	{
		setFocus( TRUE );
	}

	auto con_menu = (LLMenuGL*)mContextMenu.get();
	if (con_menu)
	{
		/* Singu Note: Share your toys!!
		// hide/show debugging options
		bool media_plugin_debugging_enabled = gSavedSettings.getBOOL("MediaPluginDebugging");
		con_menu->setItemVisible("open_webinspector", media_plugin_debugging_enabled );
		con_menu->setItemVisible("debug_separator", media_plugin_debugging_enabled );
		*/

		con_menu->buildDrawLabels();
		con_menu->updateParent(LLMenuGL::sMenuContainer);
		LLMenuGL::showPopup(this, con_menu, x, y);
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleDoubleClick( S32 x, S32 y, MASK mask )
{
	if (LLPanel::handleDoubleClick(x, y, mask)) return TRUE;
	convertInputCoords(x, y);

	if (mMediaSource)
		mMediaSource->mouseDoubleClick( x, y, mask);

	gFocusMgr.setMouseCapture( this );

	if (mTakeFocusOnClick)
	{
		setFocus( TRUE );
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::onFocusReceived()
{
	if (mMediaSource)
	{
		mMediaSource->focus(true);
		
		// Set focus for edit menu items
		LLEditMenuHandler::gEditMenuHandler = mMediaSource;
	}
	
	LLPanel::onFocusReceived();
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::onFocusLost()
{
	if (mMediaSource)
	{
		mMediaSource->focus(false);

		if( LLEditMenuHandler::gEditMenuHandler == mMediaSource )
		{
			// Clear focus for edit menu items
			LLEditMenuHandler::gEditMenuHandler = nullptr;
		}
	}

	gViewerWindow->focusClient();

	LLPanel::onFocusLost();
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::postBuild ()
{
	/*LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registar;
	registar.add("Copy.PageURL", boost::bind(&LLMediaCtrl::onCopyURL, this));
	registar.add("Open.WebInspector", boost::bind(&LLMediaCtrl::onOpenWebInspector, this));
	registar.add("Open.ViewSource", boost::bind(&LLMediaCtrl::onShowSource, this));

	// stinson 05/05/2014 : use this as the parent of the context menu if the static menu
	// container has yet to be created
	LLView* menuParent = (gMenuHolder != nullptr) ? dynamic_cast<LLView*>(gMenuHolder) : dynamic_cast<LLView*>(this);
	llassert(menuParent != NULL);*/
	auto menu = LLUICtrlFactory::getInstance()->buildMenu("menu_media_ctrl.xml",LLMenuGL::sMenuContainer);
	if(menu)
	{
		mContextMenu = menu->getHandle();
	}

	setVisibleCallback(boost::bind(&LLMediaCtrl::onVisibilityChanged, this, _2));
	return true;
}

void LLMediaCtrl::onCopyURL() const
{
	auto wurl = utf8str_to_wstring(mCurrentNavUrl);
	gClipboard.copyFromSubstring(wurl, 0, wurl.size());
}

void LLMediaCtrl::onOpenWebInspector()
{
	if (mMediaSource && mMediaSource->hasMedia())
		mMediaSource->getMediaPlugin()->showWebInspector( true );
}

void LLMediaCtrl::onShowSource()
{
	if (mMediaSource && mMediaSource->hasMedia())
		mMediaSource->getMediaPlugin()->showPageSource();
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleKeyHere( KEY key, MASK mask )
{
	BOOL result = FALSE;
	
	if (mMediaSource)
	{
		result = mMediaSource->handleKeyHere(key, mask);
	}
	
	if ( ! result )
		result = LLPanel::handleKeyHere(key, mask);
		
	return result;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleKeyUpHere(KEY key, MASK mask)
{
	BOOL result = FALSE;

	if (mMediaSource)
	{
		result = mMediaSource->handleKeyUpHere(key, mask);
	}

	if (!result)
		result = LLPanel::handleKeyUpHere(key, mask);

	return result;
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::handleVisibilityChange ( BOOL new_visibility )
{
	LL_INFOS() << "visibility changed to " << (new_visibility?"true":"false") << LL_ENDL;
	if(mMediaSource)
	{
		mMediaSource->setVisible( new_visibility );
	}
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleUnicodeCharHere(llwchar uni_char)
{
	BOOL result = FALSE;
	
	if (mMediaSource)
	{
		result = mMediaSource->handleUnicodeCharHere(uni_char);
	}

	if ( ! result )
		result = LLPanel::handleUnicodeCharHere(uni_char);

	return result;
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::onVisibilityChanged( const LLSD& new_visibility )
{
	// set state of frequent updates automatically if visibility changes
	if ( new_visibility.asBoolean() )
	{
		mFrequentUpdates = true;
	}
	else
	{
		mFrequentUpdates = false;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::reshape( S32 width, S32 height, BOOL called_from_parent )
{
	if(!getDecoupleTextureSize())
	{
		S32 screen_width = ll_round((F32)width * LLUI::getScaleFactor().mV[VX]);
		S32 screen_height = ll_round((F32)height * LLUI::getScaleFactor().mV[VY]);

		// when floater is minimized, these sizes are negative
		if ( screen_height > 0 && screen_width > 0 )
		{
			setTextureSize(screen_width, screen_height);
		}
	}
	
	LLUICtrl::reshape( width, height, called_from_parent );
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::navigateBack()
{
	if (mMediaSource && mMediaSource->hasMedia())
	{
		mMediaSource->getMediaPlugin()->browse_back();
	}
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::navigateForward()
{
	if (mMediaSource && mMediaSource->hasMedia())
	{
		mMediaSource->getMediaPlugin()->browse_forward();
	}
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::navigateStop()
{
	if (mMediaSource && mMediaSource->hasMedia())
	{
		mMediaSource->getMediaPlugin()->browse_stop();
	}
}

////////////////////////////////////////////////////////////////////////////////
//
bool LLMediaCtrl::canNavigateBack()
{
	if (mMediaSource)
		return mMediaSource->canNavigateBack();
	else
		return false;
}

////////////////////////////////////////////////////////////////////////////////
//
bool LLMediaCtrl::canNavigateForward()
{
	if (mMediaSource)
		return mMediaSource->canNavigateForward();
	else
		return false;
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::clearCache()
{
	if(mMediaSource)
	{
		mMediaSource->clearCache();
	}
	else
	{
		mClearCache = true;
	}

}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::navigateTo( std::string url_in, std::string mime_type, bool clean_browser)
{
	// don't browse to anything that starts with secondlife:// or sl://
	const std::string protocol1 = "secondlife://";
	const std::string protocol2 = "sl://";
	if ((LLStringUtil::compareInsensitive(url_in.substr(0, protocol1.length()), protocol1) == 0) ||
	    (LLStringUtil::compareInsensitive(url_in.substr(0, protocol2.length()), protocol2) == 0))
	{
		// TODO: Print out/log this attempt?
		// LL_INFOS() << "Rejecting attempt to load restricted website :" << urlIn << LL_ENDL;
		return;
	}
	
	if (ensureMediaSourceExists())
	{
		mCurrentNavUrl = url_in;
		mMediaSource->setSize(mTextureWidth, mTextureHeight);
		mMediaSource->navigateTo(url_in, mime_type, mime_type.empty(), false, clean_browser);
	}
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::navigateToLocalPage( const std::string& subdir, const std::string& filename_in )
{
	std::string filename(gDirUtilp->add(subdir, filename_in));
	std::string expanded_filename = gDirUtilp->findSkinnedFilename("html", filename);

	if(expanded_filename.empty())
	{
		LL_WARNS() << "File " << filename << "not found" << LL_ENDL;
		return;
	}
	if (ensureMediaSourceExists())
	{
		mCurrentNavUrl = expanded_filename;
		mMediaSource->setSize(mTextureWidth, mTextureHeight);
		mMediaSource->navigateTo(LLWeb::escapeURL(expanded_filename), "text/html", false);
	}
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::navigateHome()
{
	if (ensureMediaSourceExists())
	{
		mMediaSource->setSize(mTextureWidth, mTextureHeight);
		mMediaSource->navigateHome();
	}
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::setHomePageUrl( const std::string& urlIn, const std::string& mime_type )
{
	mHomePageUrl = urlIn;
	if (mMediaSource)
	{
		mMediaSource->setHomeURL(mHomePageUrl, mime_type);
	}
}

void LLMediaCtrl::setTarget(const std::string& target)
{
	mTarget = target;
	if (mMediaSource)
	{
		mMediaSource->setTarget(mTarget);
	}
}

void LLMediaCtrl::setErrorPageURL(const std::string& url)
{
	mErrorPageURL = url;
}

const std::string& LLMediaCtrl::getErrorPageURL()
{
	return mErrorPageURL;
}

////////////////////////////////////////////////////////////////////////////////
//
bool LLMediaCtrl::setCaretColor(unsigned int red, unsigned int green, unsigned int blue)
{
	//NOOP
	return false;
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::setTextureSize(S32 width, S32 height)
{
	mTextureWidth = width;
	mTextureHeight = height;
	
	if(mMediaSource)
	{
		mMediaSource->setSize(mTextureWidth, mTextureHeight);
		mForceUpdate = true;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
std::string LLMediaCtrl::getHomePageUrl()
{
	return 	mHomePageUrl;
}

////////////////////////////////////////////////////////////////////////////////
//
bool LLMediaCtrl::ensureMediaSourceExists()
{	
	if(mMediaSource.isNull())
	{
		// If we don't already have a media source, try to create one.
		mMediaSource = LLViewerMedia::newMediaImpl(mMediaTextureID, mTextureWidth, mTextureHeight);
		if ( mMediaSource )
		{
			mMediaSource->setUsedInUI(true);
			mMediaSource->setHomeURL(mHomePageUrl, mHomePageMimeType);
			mMediaSource->setTarget(mTarget);
			mMediaSource->setVisible( getVisible() );
			mMediaSource->addObserver( this );
			mMediaSource->setBackgroundColor( getBackgroundColor() );
			mMediaSource->setTrustedBrowser(mTrusted);

			F32 scale_factor = LLUI::getScaleFactor().mV[ VX ];
			if (scale_factor != mMediaSource->getPageZoomFactor())
			{
				mMediaSource->setPageZoomFactor( scale_factor );
				mUpdateScrolls = true;
			}

			if(mClearCache)
			{
				mMediaSource->clearCache();
				mClearCache = false;
			}
		}
		else
		{
			LL_WARNS() << "media source create failed " << LL_ENDL;
			// return;
		}
	}
	
	return !mMediaSource.isNull();
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::unloadMediaSource()
{
	if (mMediaSource)
	{
		mMediaSource->remObserver(this);
		mMediaSource = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
LLPluginClassMedia* LLMediaCtrl::getMediaPlugin()
{ 
	return mMediaSource.isNull() ? NULL : mMediaSource->getMediaPlugin(); 
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::draw()
{
	if ( gRestoreGL == 1 || mUpdateScrolls)
	{
		LLRect r = getRect();
		reshape( r.getWidth(), r.getHeight(), FALSE );
		mUpdateScrolls = false;
		return;
	}

	// NOTE: optimization needed here - probably only need to do this once
	// unless tearoffs change the parent which they probably do.
	const LLUICtrl* ptr = findRootMostFocusRoot();
	if ( ptr && ptr->hasFocus() )
	{
		setFrequentUpdates( true );
	}
	else
	{
		setFrequentUpdates( false );
	};

	// alpha off for this
	LLGLSUIDefault gls_ui;
	LLGLDisable<GL_ALPHA_TEST> gls_alpha_test;

	bool draw_media = false;
	
	LLPluginClassMedia* media_plugin = nullptr;
	LLViewerMediaTexture* media_texture = nullptr;
	
	if(mMediaSource && mMediaSource->hasMedia())
	{
		media_plugin = mMediaSource->getMediaPlugin();

		if(media_plugin && (media_plugin->textureValid()))
		{
			media_texture = LLViewerTextureManager::findMediaTexture(mMediaTextureID);
			if(media_texture)
			{
				draw_media = true;
			}
		}
	}
	
	bool background_visible = isBackgroundVisible();
	bool background_opaque = isBackgroundOpaque();
	
	if(draw_media)
	{
		gGL.pushUIMatrix();
		{
			F32 scale_factor = LLUI::getScaleFactor().mV[ VX ];
			if (scale_factor != mMediaSource->getPageZoomFactor())
			{
				mMediaSource->setPageZoomFactor( scale_factor );
				mUpdateScrolls = true;
			}

			// scale texture to fit the space using texture coords
			gGL.getTexUnit(0)->bind(media_texture);
			LLColor4 media_color = LLColor4::white;
			gGL.color4fv( media_color.mV );
			F32 max_u = ( F32 )media_plugin->getWidth() / ( F32 )media_plugin->getTextureWidth();
			F32 max_v = ( F32 )media_plugin->getHeight() / ( F32 )media_plugin->getTextureHeight();

			S32 x_offset, y_offset, width, height;
			calcOffsetsAndSize(&x_offset, &y_offset, &width, &height);

			// draw the browser
			gGL.setSceneBlendType(LLRender::BT_REPLACE);
			gGL.begin( LLRender::TRIANGLE_STRIP );
			if (! media_plugin->getTextureCoordsOpenGL())
			{
				// render using web browser reported width and height, instead of trying to invert GL scale
				gGL.texCoord2f( max_u, 0.f );
				gGL.vertex2i( x_offset + width, y_offset + height );

				gGL.texCoord2f( 0.f, 0.f );
				gGL.vertex2i( x_offset, y_offset + height );

				gGL.texCoord2f(max_u, max_v);
				gGL.vertex2i(x_offset + width, y_offset);

				gGL.texCoord2f( 0.f, max_v );
				gGL.vertex2i( x_offset, y_offset );
			}
			else
			{
				// render using web browser reported width and height, instead of trying to invert GL scale
				gGL.texCoord2f( max_u, max_v );
				gGL.vertex2i( x_offset + width, y_offset + height );

				gGL.texCoord2f( 0.f, max_v );
				gGL.vertex2i( x_offset, y_offset + height );

				gGL.texCoord2f(max_u, 0.f);
				gGL.vertex2i(x_offset + width, y_offset);

				gGL.texCoord2f( 0.f, 0.f );
				gGL.vertex2i( x_offset, y_offset );
			}
			gGL.end();
			gGL.setSceneBlendType(LLRender::BT_ALPHA);
		}
		gGL.popUIMatrix();
	
	}
	else
	{
		// Setting these will make LLPanel::draw draw the opaque background color.
		setBackgroundVisible(true);
		setBackgroundOpaque(true);
	}
	
	// highlight if keyboard focus here. (TODO: this needs some work)
	if ( mBorder && mBorder->getVisible() )
		mBorder->setKeyboardFocusHighlight( gFocusMgr.childHasKeyboardFocus( this ) );

	LLPanel::draw();

	// Restore the previous values
	setBackgroundVisible(background_visible);
	setBackgroundOpaque(background_opaque);
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::calcOffsetsAndSize(S32 *x_offset, S32 *y_offset, S32 *width, S32 *height)
{
	const LLRect &r = getRect();
	*x_offset = *y_offset = 0;

	const auto& media_plugin = mMediaSource->getMediaPlugin();

	if(mStretchToFill)
	{
		if(mMaintainAspectRatio)
		{
			F32 media_aspect = (F32)(media_plugin->getWidth()) / (F32)(media_plugin->getHeight());
			F32 view_aspect = (F32)(r.getWidth()) / (F32)(r.getHeight());
			if(media_aspect > view_aspect)
			{
				// max width, adjusted height
				*width = r.getWidth();
				*height = llmin(llmax(ll_round(*width / media_aspect), 0), r.getHeight());
			}
			else
			{
				// max height, adjusted width
				*height = r.getHeight();
				*width = llmin(llmax(ll_round(*height * media_aspect), 0), r.getWidth());
			}
		}
		else
		{
			*width = r.getWidth();
			*height = r.getHeight();
		}
	}
	else
	{
		*width = llmin(media_plugin->getWidth(), r.getWidth());
		*height = llmin(media_plugin->getHeight(), r.getHeight());
	}
	
	*x_offset = (r.getWidth() - *width) / 2;
	*y_offset = (r.getHeight() - *height) / 2;
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::convertInputCoords(S32& x, S32& y)
{
	bool coords_opengl = false;
	
	if(mMediaSource && mMediaSource->hasMedia())
	{
		coords_opengl = mMediaSource->getMediaPlugin()->getTextureCoordsOpenGL();
	}
	
	x = ll_round((F32)x * LLUI::getScaleFactor().mV[VX]);
	if ( ! coords_opengl )
	{
		y = ll_round((F32)(y) * LLUI::getScaleFactor().mV[VY]);
	}
	else
	{
		y = ll_round((F32)(getRect().getHeight() - y) * LLUI::getScaleFactor().mV[VY]);
	};
}

////////////////////////////////////////////////////////////////////////////////
// inherited from LLViewerMediaObserver
//virtual 
void LLMediaCtrl::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
	switch(event)
	{
		case MEDIA_EVENT_CONTENT_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CONTENT_UPDATED " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_TIME_DURATION_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_TIME_DURATION_UPDATED, time is " << self->getCurrentTime() << " of " << self->getDuration() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_SIZE_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_SIZE_CHANGED " << LL_ENDL;
			LLRect r = getRect();
			reshape( r.getWidth(), r.getHeight(), FALSE );
		};
		break;
		
		case MEDIA_EVENT_CURSOR_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CURSOR_CHANGED, new cursor is " << self->getCursorName() << LL_ENDL;
		}
		break;
			
		case MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_BEGIN, url is " << self->getNavigateURI() << LL_ENDL;
			hideNotification();
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_COMPLETE, result string is: " << self->getNavigateResultString() << LL_ENDL;
			if(mHidingInitialLoad)
			{
				mHidingInitialLoad = false;
			}
		};
		break;

		case MEDIA_EVENT_PROGRESS_UPDATED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PROGRESS_UPDATED, loading at " << self->getProgressPercent() << "%" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_STATUS_TEXT_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_STATUS_TEXT_CHANGED, new status text is: " << self->getStatusText() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_LOCATION_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_LOCATION_CHANGED, new uri is: " << self->getLocation() << LL_ENDL;
			mCurrentNavUrl = self->getLocation();
		};
		break;

		case MEDIA_EVENT_NAVIGATE_ERROR_PAGE:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_ERROR_PAGE" << LL_ENDL;
			if ( mErrorPageURL.length() > 0 )
			{
				navigateTo(mErrorPageURL, "text/html");
			};
		};
		break;

		case MEDIA_EVENT_CLICK_LINK_HREF:
		{
			// retrieve the event parameters
			std::string url = self->getClickURL();
			std::string target = self->getClickTarget();
			std::string uuid = self->getClickUUID();
			LL_DEBUGS("Media") << "Media event:  MEDIA_EVENT_CLICK_LINK_HREF, target is \"" << target << "\", uri is " << url << LL_ENDL;

			// try as slurl first
			if (!LLURLDispatcher::dispatch(url, "clicked", nullptr, mTrusted))
			{
				LLWeb::loadURL(url, target, uuid);
			}

			// CP: removing this code because we no longer support popups so this breaks the flow.
			//     replaced with a bare call to LLWeb::LoadURL(...)
			//LLNotification::Params notify_params;
			//notify_params.name = "PopupAttempt";
			//notify_params.payload = LLSD().with("target", target).with("url", url).with("uuid", uuid).with("media_id", mMediaTextureID);
			//notify_params.functor.function = boost::bind(&LLMediaCtrl::onPopup, this, _1, _2);

			//if (mTrusted)
			//{
			//	LLNotifications::instance().forceResponse(notify_params, 0);
			//}
			//else
			//{
			//	LLNotifications::instance().add(notify_params);
			//}
			break;
		};

		case MEDIA_EVENT_CLICK_LINK_NOFOLLOW:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_NOFOLLOW, uri is " << self->getClickURL() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PLUGIN_FAILED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PLUGIN_FAILED_LAUNCH:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED_LAUNCH" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAME_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAME_CHANGED" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_CLOSE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLOSE_REQUEST" << LL_ENDL;
		}
		break;
		
		case MEDIA_EVENT_PICK_FILE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PICK_FILE_REQUEST" << LL_ENDL;
		}
		break;
		
		case MEDIA_EVENT_GEOMETRY_CHANGE:
		{
			LL_DEBUGS("Media") << "Media event:  MEDIA_EVENT_GEOMETRY_CHANGE, uuid is " << self->getClickUUID() << LL_ENDL;
		}
		break;

		case MEDIA_EVENT_AUTH_REQUEST:
		{
			LLNotification::Params auth_request_params("AuthRequest");

			// pass in host name and realm for site (may be zero length but will always exist)
			LLSD args;
			LLURL raw_url( self->getAuthURL().c_str() );
			args["HOST_NAME"] = raw_url.getAuthority();
			args["REALM"] = self->getAuthRealm();
			auth_request_params.substitutions = args;

			auth_request_params.payload = LLSD().with("media_id", mMediaTextureID);
			auth_request_params.functor(boost::bind(&LLViewerMedia::onAuthSubmit, _1, _2));
			LLNotifications::instance().add(auth_request_params);
		};
		break;

		case MEDIA_EVENT_LINK_HOVERED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_LINK_HOVERED, hover text is: " << self->getHoverText() << LL_ENDL;
			mHoverTextChanged = true;
		};
		break;

		case MEDIA_EVENT_FILE_DOWNLOAD:
		{
			//llinfos << "Media event - file download requested - filename is " << self->getFileDownloadFilename() << llendl;
			//LLNotificationsUtil::add("MediaFileDownloadUnsupported");
		};
		break;

		case MEDIA_EVENT_DEBUG_MESSAGE:
		{
			std::string level = self->getDebugMessageLevel();
			if (level == "debug")
			{
				LL_DEBUGS("Media") << self->getDebugMessageText() << LL_ENDL;
			}
			else if (level == "info")
			{
				LL_INFOS("Media") << self->getDebugMessageText() << LL_ENDL;
			}
			else if (level == "warn")
			{
				LL_WARNS("Media") << self->getDebugMessageText() << LL_ENDL;
			}
			else if (level == "error")
			{
				LL_ERRS("Media") << self->getDebugMessageText() << LL_ENDL;
			}

		};
		break;
		
		default:
		{
			LL_WARNS("Media") <<  "Media event:  unknown event type" << LL_ENDL;
		};		
	};

	// chain all events to any potential observers of this object.
	emitEvent(self, event);
}

////////////////////////////////////////////////////////////////////////////////
// 
std::string LLMediaCtrl::getCurrentNavUrl()
{
	return mCurrentNavUrl;
}

void LLMediaCtrl::onPopup(const LLSD& notification, const LLSD& response)
{
	if (response["open"])
	{
		LLWeb::loadURL(notification["payload"]["url"], notification["payload"]["target"], notification["payload"]["uuid"]);
	}
	else
	{
		// Make sure the opening instance knows its window open request was denied, so it can clean things up.
		LLViewerMedia::proxyWindowClosed(notification["payload"]["uuid"]);
	}
}

void LLMediaCtrl::showNotification(LLNotificationPtr notify)
{
/*	std::string url = self->getClickURL();
	if (LLURLDispatcher::isSLURLCommand(url)
		&& !mTrusted)
	{
		// block handling of this secondlife:///app/ URL
		LLNotificationsUtil::add("UnableToOpenCommandURL");
		return;
	}

	LLURLDispatcher::dispatch(url, this, mTrusted);*/
	LLNotifications::instance().add(notify);
}


void LLMediaCtrl::hideNotification()
{
}

void LLMediaCtrl::setTrustedContent(bool trusted)
{
	mTrusted = trusted;
	if (mMediaSource)
	{
		mMediaSource->setTrustedBrowser(trusted);
	}
}

bool LLMediaCtrl::wantsKeyUpKeyDown() const
{
    return true;
}

bool LLMediaCtrl::wantsReturnKey() const
{
    return true;
}

// virtual
LLXMLNodePtr LLMediaCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_WEB_BROWSER_CTRL_TAG);

	return node;
}

LLView* LLMediaCtrl::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	LLMediaCtrl::Params p;
	
	BOOL bval;
	LLColor4 color;
	S32 ival;
	LLRect rect;

	std::string sval("web_browser");
	node->getAttributeString("name", sval);
	p.name = sval;
	createRect(node, rect, parent, LLRect());
	p.rect = rect;

	if(node->getAttributeString("start_url", sval ))
		p.start_url = sval;
	if(node->getAttributeString("error_page_url", sval ))
		p.error_page_url = sval;
	if(node->getAttributeString("media_id", sval ))
		p.media_id = sval;
	if(node->getAttributeString("initial_mime_type", sval ))
		p.initial_mime_type = sval;
	if(node->getAttributeBOOL("border_visible", bval))
		p.border_visible = bval;
	if(node->getAttributeBOOL("focus_on_click", bval))
		p.focus_on_click = bval;
	if(node->getAttributeBOOL("decouple_texture_size", bval))
		p.decouple_texture_size = bval;
	if(node->getAttributeBOOL("trusted_content", bval))
		p.trusted_content = bval;
	if(node->getAttributeS32("texture_width", ival))
		p.texture_width = ival;
	if(node->getAttributeBOOL("texture_height", ival))
		p.texture_height = ival;
	if(LLUICtrlFactory::getAttributeColor(node, "caret_color", color))
		p.caret_color = color;

	LLMediaCtrl* web_browser = LLUICtrlFactory::create<LLMediaCtrl>(p,parent);

	web_browser->initFromXML(node, parent);

	if(!p.start_url.getValue().empty())
	{
		web_browser->navigateHome();
	}

	return web_browser;
}
