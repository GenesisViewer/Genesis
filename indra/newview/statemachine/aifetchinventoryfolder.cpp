/**
 * @file aifetchinventoryfolder.cpp
 * @brief Implementation of AIFetchInventoryFolder
 *
 * Copyright (c) 2011, Aleric Inglewood.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution.
 *
 * CHANGELOG
 *   and additional copyright holders.
 *
 *   19/05/2011
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "linden_common.h"
#include "aifetchinventoryfolder.h"
#include "aievent.h"
#include "llagent.h"
#include "llinventoryobserver.h"
#include "llinventorymodelbackgroundfetch.h"

enum fetchinventoryfolder_state_type {
  AIFetchInventoryFolder_checkFolderExists = AIStateMachine::max_state,
  AIFetchInventoryFolder_fetchDescendents,
  AIFetchInventoryFolder_folderCompleted
};

class AIInventoryFetchDescendentsObserver : public LLInventoryFetchDescendentsObserver {
  public:
	AIInventoryFetchDescendentsObserver(AIStateMachine* statemachine, LLUUID const& folder);
	~AIInventoryFetchDescendentsObserver() { gInventory.removeObserver(this); }

  protected:
	/*virtual*/ void done()
	{
	  mStateMachine->advance_state(AIFetchInventoryFolder_folderCompleted);
	  delete this;
	}

  private:
	LLPointer<AIStateMachine> mStateMachine;
};

AIInventoryFetchDescendentsObserver::AIInventoryFetchDescendentsObserver(AIStateMachine* statemachine, LLUUID const& folder) : 
	mStateMachine(statemachine),
	LLInventoryFetchDescendentsObserver(folder)
{
	// Call idle() on the parent state machine before passing it.
	llassert(mStateMachine->waiting());
	startFetch();
	if(isFinished())
	{
		done();
	}
	else
	{
		 gInventory.addObserver(this);
	}
}

void AIFetchInventoryFolder::fetch(std::string const& foldername, bool create, bool fetch_contents)
{
  fetch(gInventory.getRootFolderID(), foldername, create, fetch_contents);
}

char const* AIFetchInventoryFolder::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
	AI_CASE_RETURN(AIFetchInventoryFolder_checkFolderExists);
	AI_CASE_RETURN(AIFetchInventoryFolder_fetchDescendents);
	AI_CASE_RETURN(AIFetchInventoryFolder_folderCompleted);
  }
  return "UNKNOWN STATE";
}

void AIFetchInventoryFolder::initialize_impl(void)
{
  mExists = false;
  mCreated = false;
  mNeedNotifyObservers = false;
  set_state(AIFetchInventoryFolder_checkFolderExists);
  if (!gInventory.isInventoryUsable())
  {
	idle();
	// This calls this->cont() when the event occurs.
	AIEvent::Register(AIEvent::LLInventoryModel_mIsAgentInvUsable_true, this);
  }
}

void AIFetchInventoryFolder::multiplex_impl(state_type run_state)
{
  switch (run_state)
  {
	case AIFetchInventoryFolder_checkFolderExists:
	{
	  // If LLInventoryModel_mIsAgentInvUsable_true then this should be and stay true forever.
	  llassert(gInventory.isInventoryUsable());
	  if (mParentFolder.isNull())
		mParentFolder = gInventory.getRootFolderID();
	  if (mFolderUUID.isNull() || !gInventory.getCategory(mFolderUUID))		// Is the UUID unknown, or doesn't exist?
	  {
		// Set this to null here in case we abort.
		mFolderUUID.setNull();
		if (mFolderName.empty())
		{
		  // We can only find a folder by name, or create it, if we know it's name.
		  LL_WARNS() << "Unknown folder ID " << mFolderUUID << LL_ENDL;
		  abort();
		  break;
		}
		// Check if the parent exists.
		if (mParentFolder != gInventory.getRootFolderID() && !gInventory.getCategory(mParentFolder))
		{
		  LL_WARNS() << "Unknown parent folder ID " << mParentFolder << LL_ENDL;
		  abort();
		  break;
		}
		// Look up UUID by name.
		LLInventoryModel::cat_array_t* categories;
		gInventory.getDirectDescendentsOf(mParentFolder, categories);
		for (U32 i = 0; i < categories->size(); ++i)
		{
		  LLPointer<LLViewerInventoryCategory> const& category(categories->at(i));
		  if (category->getName() == mFolderName)
		  {
			mFolderUUID = category->getUUID();
			break;
		  }
		}
		if (mFolderUUID.isNull())											// Does the folder exist?
		{
		  if (!mCreate)
		  {
			// We're done.
			finish();
			break;
		  }
		  // Create the folder.
		  mFolderUUID = gInventory.createNewCategory(mParentFolder, LLFolderType::FT_NONE, mFolderName);
		  llassert_always(!mFolderUUID.isNull());
		  Dout(dc::statemachine(mSMDebug), "Created folder \"" << mFolderName << "\".");
		  mNeedNotifyObservers = true;
		}
		mCreated = true;
	  }
	  // mFolderUUID is now valid.
	  mExists = true;
	  if (!mFetchContents ||							// No request to fetch contents.
		  LLInventoryModelBackgroundFetch::instance().isEverythingFetched())		// No need to fetch contents.
	  {
		// We're done.
		finish();
		break;
	  }
	  set_state(AIFetchInventoryFolder_fetchDescendents);
	  /*Fall-through*/
	}
	case AIFetchInventoryFolder_fetchDescendents:
	{
	  idle();	// Wait till the state is set to AIFetchInventoryFolder_folderCompleted.
	  // This sets the state to AIFetchInventoryFolder_folderCompleted once the folder is complete.
	  new AIInventoryFetchDescendentsObserver(this, mFolderUUID);
	  break;
	}
	case AIFetchInventoryFolder_folderCompleted:
	{
	  // Does it still exist?
	  if (!gInventory.getCategory(mFolderUUID))
	  {
		// Assume the folder was deleted in the meantime.
		abort();
		break;
	  }
	  llassert(gInventory.isCategoryComplete(mFolderUUID));
	  // The folder is complete!
	  finish();
	  break;
	}
  }
}

void AIFetchInventoryFolder::finish_impl(void)
{
  if (mNeedNotifyObservers)
	gInventory.notifyObservers();
}
