/**
 * @file llfloaterurlentry.cpp
 * @brief LLFloaterURLEntry class implementation
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llhttpclient.h"

#include "llfloaterurlentry.h"

#include "llpanellandmedia.h"
#include "llpanelface.h"

#include "llcombobox.h"
#include "llmimetypes.h"
#include "llnotificationsutil.h"
#include "llurlhistory.h"
#include "lluictrlfactory.h"
#include "llwindow.h"
#include "llviewerwindow.h"
#include "llhttpclient.h"

class AIHTTPTimeoutPolicy;
extern AIHTTPTimeoutPolicy mediaTypeResponder_timeout;

static LLFloaterURLEntry* sInstance = NULL;

// Move this to its own file.
// helper class that tries to download a URL from a web site and calls a method
// on the Panel Land Media and to discover the MIME type
class LLMediaTypeResponder : public LLHTTPClient::ResponderHeadersOnly
{
public:
	LLMediaTypeResponder( const LLHandle<LLFloater> parent ) :
	  mParent( parent )
	  {}

	  LLHandle<LLFloater> mParent;

	  /*virtual*/ void completedHeaders(void)
	  {
		  if (isGoodStatus(mStatus))
		  {
			  std::string media_type;
			  if (mReceivedHeaders.getFirstValue("content-type", media_type))
			  {
				  std::string::size_type idx1 = media_type.find_first_of(";");
				  std::string mime_type = media_type.substr(0, idx1);
				  completeAny(mStatus, mime_type);
				  return;
			  }
			  LL_WARNS() << "LLMediaTypeResponder::completedHeaders: OK HTTP status (" << mStatus << ") but no Content-Type! Received headers: " << mReceivedHeaders << LL_ENDL;
		  }
		  completeAny(mStatus, "none/none");
	  }

	  void completeAny(U32 status, const std::string& mime_type)
	  {
		  // Set empty type to none/none.  Empty string is reserved for legacy parcels
		  // which have no mime type set.
		  std::string resolved_mime_type = ! mime_type.empty() ? mime_type : LLMIMETypes::getDefaultMimeType();
		  LLFloaterURLEntry* floater_url_entry = (LLFloaterURLEntry*)mParent.get();
		  if ( floater_url_entry )
			  floater_url_entry->headerFetchComplete( status, resolved_mime_type );
	  }

	  /*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return mediaTypeResponder_timeout; }

	  /*virtual*/ char const* getName(void) const { return "LLMediaTypeResponder"; }
};

//-----------------------------------------------------------------------------
// LLFloaterURLEntry()
//-----------------------------------------------------------------------------
LLFloaterURLEntry::LLFloaterURLEntry(LLHandle<LLPanel> parent)
	:
	LLFloater(),
	mMediaURLEdit(nullptr),
	mPanelLandMediaHandle(parent)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_url_entry.xml");
}

//-----------------------------------------------------------------------------
// ~LLFloaterURLEntry()
//-----------------------------------------------------------------------------
LLFloaterURLEntry::~LLFloaterURLEntry()
{
	sInstance = NULL;
}

BOOL LLFloaterURLEntry::postBuild()
{
	mMediaURLEdit = getChild<LLComboBox>("media_entry");

	// Cancel button
	childSetAction("cancel_btn", onBtnCancel, this);

	// Cancel button
	childSetAction("clear_btn", onBtnClear, this);
	// clear media list button
	LLSD parcel_history = LLURLHistory::getURLHistory("parcel");
	bool enable_clear_button = parcel_history.size() > 0 ? true : false;
	getChildView("clear_btn")->setEnabled(enable_clear_button );

	// OK button
	childSetAction("ok_btn", onBtnOK, this);

	setDefaultBtn("ok_btn");
	buildURLHistory();

	return TRUE;
}
void LLFloaterURLEntry::buildURLHistory()
{
	LLCtrlListInterface* url_list = childGetListInterface("media_entry");
	if (url_list)
	{
		url_list->operateOnAll(LLCtrlListInterface::OP_DELETE);
	}

	// Get all of the entries in the "parcel" collection
	LLSD parcel_history = LLURLHistory::getURLHistory("parcel");

	LLSD::array_iterator iter_history =
		parcel_history.beginArray();
	LLSD::array_iterator end_history =
		parcel_history.endArray();
	for(; iter_history != end_history; ++iter_history)
	{
		url_list->addSimpleElement((*iter_history).asString());
	}
}

void LLFloaterURLEntry::headerFetchComplete(U32 status, const std::string& mime_type)
{
	LLPanelLandMedia* panel_media = dynamic_cast<LLPanelLandMedia*>(mPanelLandMediaHandle.get());
	if (panel_media)
	{
		// status is ignored for now -- error = "none/none"
		panel_media->setMediaType(mime_type);
		panel_media->setMediaURL(mMediaURLEdit->getValue().asString());
	}
	else
	{
		LLPanelFace* panel_face = dynamic_cast<LLPanelFace*>(mPanelLandMediaHandle.get());
		if(panel_face)
		{
			panel_face->setMediaType(mime_type);
			panel_face->setMediaURL(mMediaURLEdit->getValue().asString());
		}

	}
	// Decrement the cursor
	getWindow()->decBusyCount();
	getChildView("loading_label")->setVisible( false);
	close();
}

// static
LLHandle<LLFloater> LLFloaterURLEntry::show(LLHandle<LLPanel> parent, const std::string media_url)
{
	if (!sInstance)
	{
		sInstance = new LLFloaterURLEntry(parent);
	}
	sInstance->open();
	sInstance->addURLToCombobox(media_url);
	return sInstance->getHandle();
}



bool LLFloaterURLEntry::addURLToCombobox(const std::string& media_url)
{
	if(! mMediaURLEdit->setSimple( media_url ) && ! media_url.empty())
	{
		mMediaURLEdit->add( media_url );
		mMediaURLEdit->setSimple( media_url );
		return true;
	}

	// URL was not added for whatever reason (either it was empty or already existed)
	return false;
}

// static
//-----------------------------------------------------------------------------
// onBtnOK()
//-----------------------------------------------------------------------------
void LLFloaterURLEntry::onBtnOK( void* userdata )
{
	LLFloaterURLEntry *self =(LLFloaterURLEntry *)userdata;

	std::string media_url	= self->mMediaURLEdit->getValue().asString();
	self->mMediaURLEdit->remove(media_url);
	LLURLHistory::removeURL("parcel", media_url);
	if(self->addURLToCombobox(media_url))
	{
		// Add this url to the parcel collection
		LLURLHistory::addURL("parcel", media_url);
	}

	// leading whitespace causes problems with the MIME-type detection so strip it
	LLStringUtil::trim( media_url );

	// First check the URL scheme
	LLURI url(media_url);
	std::string scheme = url.scheme();

	// We assume that an empty scheme is an http url, as this is how we will treat it.
	if(scheme == "")
	{
		scheme = "http";
	}

	// Discover the MIME type only for "http" scheme.
	if(!media_url.empty() && 
	   (scheme == "http" || scheme == "https"))
	{
		LLHTTPClient::getHeaderOnly( media_url,
			new LLMediaTypeResponder(self->getHandle()));
	}
	else
	{
		self->headerFetchComplete(0, scheme);
	}

	// Grey the buttons until we get the header response
	self->getChildView("ok_btn")->setEnabled(false);
	self->getChildView("cancel_btn")->setEnabled(false);
	self->getChildView("media_entry")->setEnabled(false);

	// show progress bar here?
	getWindow()->incBusyCount();
	self->getChildView("loading_label")->setVisible( true);
}

// static
//-----------------------------------------------------------------------------
// onBtnCancel()
//-----------------------------------------------------------------------------
void LLFloaterURLEntry::onBtnCancel( void* userdata )
{
	LLFloaterURLEntry *self =(LLFloaterURLEntry *)userdata;
	self->close();
}

// static
//-----------------------------------------------------------------------------
// onBtnClear()
//-----------------------------------------------------------------------------
void LLFloaterURLEntry::onBtnClear( void* userdata )
{
	LLNotificationsUtil::add( "ConfirmClearMediaUrlList", LLSD(), LLSD(), 
									boost::bind(&LLFloaterURLEntry::callback_clear_url_list, (LLFloaterURLEntry*)userdata, _1, _2) );
}

bool LLFloaterURLEntry::callback_clear_url_list(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if ( option == 0 ) // YES
	{
		// clear saved list
		LLCtrlListInterface* url_list = childGetListInterface("media_entry");
		if ( url_list )
		{
			url_list->operateOnAll( LLCtrlListInterface::OP_DELETE );
		}

		// clear current contents of combo box
		mMediaURLEdit->clear();

		// clear stored version of list
		LLURLHistory::clear("parcel");

		// cleared the list so disable Clear button
		getChildView("clear_btn")->setEnabled(false );
	}
	return false;
}
