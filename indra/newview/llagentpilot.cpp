/** 
 * @file llagentpilot.cpp
 * @brief LLAgentPilot class implementation
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

#include <iostream>
#include <fstream>
#include <iomanip>

#include "llagentpilot.h"
#include "llagent.h"
#include "llframestats.h"
#include "llappviewer.h"
#include "llviewercontrol.h"

LLAgentPilot gAgentPilot;

BOOL LLAgentPilot::sLoop = TRUE;

LLAgentPilot::LLAgentPilot() :
	mNumRuns(-1),
	mQuitAfterRuns(FALSE),
	mRecording(FALSE),
	mLastRecordTime(0.f),
	mStarted(FALSE),
	mPlaying(FALSE),
	mCurrentAction(0)
{
}

LLAgentPilot::~LLAgentPilot()
{
}

void LLAgentPilot::load(const std::string& filename)
{
	if(filename.empty())
	{
		return;
	}
	
	llifstream file(filename);

	if (!file)
	{
		LL_DEBUGS() << "Couldn't open " << filename
			<< ", aborting agentpilot load!" << LL_ENDL;
		return;
	}
	else
	{
		LL_INFOS() << "Opening pilot file " << filename << LL_ENDL;
	}

	S32 num_actions;

	file >> num_actions;

	for (S32 i = 0; i < num_actions; i++)
	{
		S32 action_type;
		Action new_action;
		file >> new_action.mTime >> action_type;
		file >> new_action.mTarget.mdV[VX] >> new_action.mTarget.mdV[VY] >> new_action.mTarget.mdV[VZ];
		new_action.mType = (EActionType)action_type;
		mActions.push_back(new_action);
	}

	file.close();
}

void LLAgentPilot::save(const std::string& filename)
{
	llofstream file;
	file.open(filename);

	if (!file)
	{
		LL_INFOS() << "Couldn't open " << filename << ", aborting agentpilot save!" << LL_ENDL;
	}

	file << mActions.size() << '\n';

	U32 i;
	for (i = 0; i < mActions.size(); i++)
	{
		file << mActions[i].mTime << "\t" << mActions[i].mType << "\t";
		file << std::setprecision(32) << mActions[i].mTarget.mdV[VX] << "\t" << mActions[i].mTarget.mdV[VY] << "\t" << mActions[i].mTarget.mdV[VZ];
		file << '\n';
	}

	file.close();
}

void LLAgentPilot::startRecord()
{
	mActions.clear();
	mTimer.reset();
	addAction(STRAIGHT);
	mRecording = TRUE;
}

void LLAgentPilot::stopRecord()
{
	gAgentPilot.addAction(STRAIGHT);
	gAgentPilot.save(gSavedSettings.getString("StatsPilotFile"));
	mRecording = FALSE;
}

void LLAgentPilot::addAction(enum EActionType action_type)
{
	LL_INFOS() << "Adding waypoint: " << gAgent.getPositionGlobal() << LL_ENDL;
	Action action;
	action.mType = action_type;
	action.mTarget = gAgent.getPositionGlobal();
	action.mTime = mTimer.getElapsedTimeF32();
	mLastRecordTime = (F32)action.mTime;
	mActions.push_back(action);
}

void LLAgentPilot::startPlayback()
{
	if (!mPlaying)
	{
		mPlaying = TRUE;
		mCurrentAction = 0;
		mTimer.reset();

		if (mActions.size())
		{
			LL_INFOS() << "Starting playback, moving to waypoint 0" << LL_ENDL;
			gAgent.startAutoPilotGlobal(mActions[0].mTarget);
			mStarted = FALSE;
		}
		else
		{
			LL_INFOS() << "No autopilot data, cancelling!" << LL_ENDL;
			mPlaying = FALSE;
		}
	}
}

void LLAgentPilot::stopPlayback()
{
	if (mPlaying)
	{
		mPlaying = FALSE;
		mCurrentAction = 0;
		mTimer.reset();
		gAgent.stopAutoPilot();
	}
}

void LLAgentPilot::updateTarget()
{
	if (mPlaying)
	{
		if (mCurrentAction < (S32)mActions.size())
		{
			if (0 == mCurrentAction)
			{
				if (gAgent.getAutoPilot())
				{
					// Wait until we get to the first location before starting.
					return;
				}
				else
				{
					if (!mStarted)
					{
						LL_INFOS() << "At start, beginning playback" << LL_ENDL;
						mTimer.reset();
						LLFrameStats::startLogging(NULL);
						mStarted = TRUE;
					}
				}
			}
			if (mTimer.getElapsedTimeF32() > mActions[mCurrentAction].mTime)
			{
				//gAgent.stopAutoPilot();
				mCurrentAction++;

				if (mCurrentAction < (S32)mActions.size())
				{
					gAgent.startAutoPilotGlobal(mActions[mCurrentAction].mTarget);
				}
				else
				{
					stopPlayback();
					LLFrameStats::stopLogging(NULL);
					mNumRuns--;
					if (sLoop)
					{
						if ((mNumRuns < 0) || (mNumRuns > 0))
						{
							LL_INFOS() << "Looping, restarting playback" << LL_ENDL;
							startPlayback();
						}
						else if (mQuitAfterRuns)
						{
							LL_INFOS() << "Done with all runs, quitting viewer!" << LL_ENDL;
							LLAppViewer::instance()->forceQuit();
						}
						else
						{
							LL_INFOS() << "Done with all runs, disabling pilot" << LL_ENDL;
							stopPlayback();
						}
					}
				}
			}
		}
		else
		{
			stopPlayback();
		}
	}
	else if (mRecording)
	{
		if (mTimer.getElapsedTimeF32() - mLastRecordTime > 1.f)
		{
			addAction(STRAIGHT);
		}
	}
}

// static
void LLAgentPilot::startRecord(void *)
{
	gAgentPilot.startRecord();
}

void LLAgentPilot::saveRecord(void *)
{
	gAgentPilot.stopRecord();
}

void LLAgentPilot::addWaypoint(void *)
{
	gAgentPilot.addAction(STRAIGHT);
}

void LLAgentPilot::startPlayback(void *)
{
	gAgentPilot.mNumRuns = -1;
	gAgentPilot.startPlayback();
}

void LLAgentPilot::stopPlayback(void *)
{
	gAgentPilot.stopPlayback();
}
