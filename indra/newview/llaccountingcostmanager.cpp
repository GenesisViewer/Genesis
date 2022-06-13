/** 
 * @file LLAccountingQuotaManager.cpp
 * @ Handles the setting and accessing for costs associated with mesh 
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llaccountingcostmanager.h"
#include "llagent.h"
#include "llcurl.h"
#include "llhttpclient.h"

//===============================================================================
LLAccountingCostManager::LLAccountingCostManager()
{	
}
//===============================================================================
class LLAccountingCostResponder : public LLHTTPClient::ResponderWithResult
{
public:
	LLAccountingCostResponder( const LLSD& objectIDs, const LLHandle<LLAccountingCostObserver>& observer_handle )
	: mObjectIDs( objectIDs ),
	  mObserverHandle( observer_handle )
	{
		LLAccountingCostObserver* observer = mObserverHandle.get();
		if (observer)
		{
			mTransactionID = observer->getTransactionID();
		}
	}
		
	void clearPendingRequests ( void )
	{
		for ( LLSD::array_iterator iter = mObjectIDs.beginArray(); iter != mObjectIDs.endArray(); ++iter )
		{
			LLAccountingCostManager::getInstance()->removePendingObject( iter->asUUID() );
		}
	}
	
	void httpFailure(void)
	{
		LL_WARNS() << "Transport error [status:" << mStatus << "]: " << mReason << LL_ENDL;
		clearPendingRequests();

		LLAccountingCostObserver* observer = mObserverHandle.get();
		if (observer && observer->getTransactionID() == mTransactionID)
		{
			observer->setErrorStatus(mStatus, mReason);
		}
	}
	
	void httpSuccess(void)
	{
		//Check for error
		if ( !mContent.isMap() || mContent.has("error") )
		{
			LL_WARNS() << "Error on fetched data" << LL_ENDL;
		}
		else if (mContent.has("selected"))
		{
			F32 physicsCost		= 0.0f;
			F32 networkCost		= 0.0f;
			F32 simulationCost	= 0.0f;

			physicsCost		= mContent["selected"]["physics"].asReal();
			networkCost		= mContent["selected"]["streaming"].asReal();
			simulationCost	= mContent["selected"]["simulation"].asReal();
				
			SelectionCost selectionCost( /*transactionID,*/ physicsCost, networkCost, simulationCost );

			LLAccountingCostObserver* observer = mObserverHandle.get();
			if (observer && observer->getTransactionID() == mTransactionID)
			{
				observer->onWeightsUpdate(selectionCost);
			}
		}

		clearPendingRequests();
	}
	
	/*virtual*/ char const* getName(void) const { return "LLAccountingCostResponder"; }

private:
	//List of posted objects
	LLSD mObjectIDs;

	// Current request ID
	LLUUID mTransactionID;

	// Cost update observer handle
	LLHandle<LLAccountingCostObserver> mObserverHandle;
};
//===============================================================================
void LLAccountingCostManager::fetchCosts( eSelectionType selectionType,
										  const std::string& url,
										  const LLHandle<LLAccountingCostObserver>& observer_handle )
{
	// Invoking system must have already determined capability availability
	if ( !url.empty() )
	{
		LLSD objectList;
		U32  objectIndex = 0;
		
		IDIt IDIter = mObjectList.begin();
		IDIt IDIterEnd = mObjectList.end();
		
		for ( ; IDIter != IDIterEnd; ++IDIter )
		{
			// Check to see if a request for this object has already been made.
			if ( mPendingObjectQuota.find( *IDIter ) ==	mPendingObjectQuota.end() )
			{
				mPendingObjectQuota.insert( *IDIter );
				objectList[objectIndex++] = *IDIter;
			}
		}
	
		mObjectList.clear();
		
		//Post results
		if ( objectList.size() > 0 )
		{
			std::string keystr;
			if ( selectionType == Roots ) 
			{ 
				keystr="selected_roots"; 
			}
			else
			if ( selectionType == Prims ) 
			{ 
				keystr="selected_prims";
			}
			else 
			{
				LL_INFOS() << "Invalid selection type " << LL_ENDL;
				mObjectList.clear();
				mPendingObjectQuota.clear();
				return;
			}
			
			LLSD dataToPost = LLSD::emptyMap();		
			dataToPost[keystr.c_str()] = objectList;

			LLHTTPClient::post( url, dataToPost, new LLAccountingCostResponder( objectList, observer_handle ));
		}
	}
	else
	{
		//url was empty - warn & continue
		LL_WARNS() << "Supplied url is empty " << LL_ENDL;
		mObjectList.clear();
		mPendingObjectQuota.clear();
	}
}
//===============================================================================
void LLAccountingCostManager::addObject( const LLUUID& objectID )
{
	mObjectList.insert( objectID );
}
//===============================================================================
void LLAccountingCostManager::removePendingObject( const LLUUID& objectID )
{
	mPendingObjectQuota.erase( objectID );
}
//===============================================================================
