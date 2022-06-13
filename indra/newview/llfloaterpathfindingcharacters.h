/**
* @file   llfloaterpathfindingcharacters.h
* @brief  "Pathfinding characters" floater, allowing for identification of pathfinding characters and their cpu usage.
* @author Stinson@lindenlab.com
*
* $LicenseInfo:firstyear=2012&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2012, Linden Research, Inc.
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
#ifndef LL_LLFLOATERPATHFINDINGCHARACTERS_H
#define LL_LLFLOATERPATHFINDINGCHARACTERS_H

#include "llfloaterpathfindingobjects.h"
#include "llpathfindingobjectlist.h"
#include "lluuid.h"
#include "v4color.h"

//class LLCheckBoxCtrl;
class LLPathfindingCharacter;
/* Singu Note: Capsule doesn't seem to work without havok
class LLQuaternion;
class LLSD;
class LLVector3;
*/

class LLFloaterPathfindingCharacters : public LLFloaterPathfindingObjects, public LLFloaterSingleton<LLFloaterPathfindingCharacters>
{
public:
/* Singu Note: Capsule doesn't seem to work without havok
	virtual void                                    onClose(bool pIsAppQuitting);

	BOOL                                            isShowPhysicsCapsule() const;
	void                                            setShowPhysicsCapsule(BOOL pIsShowPhysicsCapsule);

	BOOL                                            isPhysicsCapsuleEnabled(LLUUID& id, LLVector3& pos, LLQuaternion& rot) const;
*/

	static void                                     openCharactersWithSelectedObjects();

	LLFloaterPathfindingCharacters(const LLSD& pSeed);
	virtual ~LLFloaterPathfindingCharacters();

protected:
//	friend class LLFloaterReg;

	virtual BOOL                       postBuild();

	virtual void                       requestGetObjects();

	virtual void                       buildObjectsScrollList(const LLPathfindingObjectListPtr pObjectListPtr);

//	virtual void                       updateControlsOnScrollListChange();

	virtual S32                        getNameColumnIndex() const;
	virtual S32                        getOwnerNameColumnIndex() const;
	virtual std::string                getOwnerName(const LLPathfindingObject *pObject) const;
	virtual const LLColor4             &getBeaconColor() const;

	virtual LLPathfindingObjectListPtr getEmptyObjectList() const;

private:
//	void onShowPhysicsCapsuleClicked();


	LLSD buildCharacterScrollListItemData(const LLPathfindingCharacter *pCharacterPtr) const;

/* Singu Note: Capsule doesn't seem to work without havok
	void updateStateOnDisplayControls();
	void showSelectedCharacterCapsules();

	void showCapsule() const;
	void hideCapsule() const;

	bool getCapsuleRenderData(LLVector3& pPosition, LLQuaternion& rot) const;

	LLCheckBoxCtrl                                   *mShowPhysicsCapsuleCheckBox;
*/

	LLUUID                                           mSelectedCharacterId;

	LLColor4                                         mBeaconColor;
};

#endif // LL_LLFLOATERPATHFINDINGCHARACTERS_H
