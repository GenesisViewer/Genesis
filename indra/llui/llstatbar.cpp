/** 
 * @file llstatbar.cpp
 * @brief A little map of the world with network information
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

//#include "llviewerprecompiledheaders.h"
#include "linden_common.h"

#include "llstatbar.h"

#include "llmath.h"
#include "llui.h"
#include "llgl.h"
#include "llfontgl.h"

#include "llstat.h"
#include "lluictrlfactory.h"

///////////////////////////////////////////////////////////////////////////////////

LLStatBar::LLStatBar(const std::string& name, const LLRect& rect, LLStat* stat, const Parameters& parameters, const std::string& setting,
					 BOOL default_bar, BOOL default_history)
	:	LLView(name, rect, TRUE),
		mSetting(setting),
		mLabel(name),
		mParameters(parameters),
		mStatp(stat),
		mValue(0.f),
		mMinShift(0),
		mMaxShift(0)
{
	S32 mode = -1;
	if (mSetting.length() > 0)
	{
		mode = LLUI::sConfigGroup->getS32(mSetting);

	}

	if (mode != -1)
	{
		mDisplayBar     = (mode & STAT_BAR_FLAG) ? TRUE : FALSE;
		mDisplayHistory = (mode & STAT_HISTORY_FLAG) ? TRUE : FALSE;
	}
	else
	{
		mDisplayBar     = default_bar;
		mDisplayHistory = default_history;
	}
}

BOOL LLStatBar::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mDisplayBar)
	{
		if (mDisplayHistory)
		{
			mDisplayBar = FALSE;
			mDisplayHistory = FALSE;
		}
		else
		{
			mDisplayHistory = TRUE;
		}
	}
	else
	{
		mDisplayBar = TRUE;
	}

	LLView* parent = getParent();
	parent->reshape(parent->getRect().getWidth(), parent->getRect().getHeight(), FALSE);

	// save view mode
	if (mSetting.length() > 0)
	{
		S32 mode = 0;
		if (mDisplayBar)
		{
			mode |= STAT_BAR_FLAG;
		}
		if (mDisplayHistory)
		{
			mode |= STAT_HISTORY_FLAG;
		}

		LLUI::sConfigGroup->setS32(mSetting, mode);
	}
	
	
	return TRUE;
}

void LLStatBar::draw()
{
	if (!mStatp)
	{
//		LL_INFOS() << "No stats for statistics bar!" << LL_ENDL;
		return;
	}

	// Get the values.
	F32 current, min, max, mean;
	if (mParameters.mPerSec)
	{
		current = mStatp->getCurrentPerSec();
		min = mStatp->getMinPerSec();
		max = mStatp->getMaxPerSec();
		mean = mStatp->getMeanPerSec();
	}
	else
	{
		current = mStatp->getCurrent();
		min = mStatp->getMin();
		max = mStatp->getMax();
		mean = mStatp->getMean();
	}


	if ((mParameters.mUpdatesPerSec == 0.f) || (mUpdateTimer.getElapsedTimeF32() > 1.f/mParameters.mUpdatesPerSec) || (mValue == 0.f))
	{
		if (mParameters.mDisplayMean)
		{
			mValue = mean;
		}
		else
		{
			mValue = current;
		}
		mUpdateTimer.reset();
	}

	S32 width = getRect().getWidth() - 40;
	S32 max_width = width;
	S32 bar_top = getRect().getHeight() - 15; // 16 pixels from top.
	S32 bar_height = bar_top - 20;
	S32 tick_height = 4;
	S32 tick_width = 1;
	S32 left, top, right, bottom;

	F32 value_scale = max_width/((mParameters.mMaxBar + mMaxShift) - (mParameters.mMinBar + mMinShift));

	LLFontGL::getFontMonospace()->renderUTF8(mLabel, 0, 0, getRect().getHeight(), LLColor4(1.f, 1.f, 1.f, 1.f),
							LLFontGL::LEFT, LLFontGL::TOP);


	std::string value_str;
	if (!mParameters.mUnitLabel.empty())
	{
		value_str = llformat( "%.*f%s", mParameters.mPrecision, mValue, mParameters.mUnitLabel.c_str());
	}
	else
	{
		value_str = llformat("%.*f", mParameters.mPrecision, mValue);
	}

	// Draw the value.
	LLFontGL::getFontMonospace()->renderUTF8(value_str, 0, width, getRect().getHeight(), 
		LLColor4(1.f, 1.f, 1.f, 0.5f),
		LLFontGL::RIGHT, LLFontGL::TOP);

	if (mDisplayBar)
	{
		// Draw the tick marks.
		F32 tick_value;
		top = bar_top;
		bottom = bar_top - bar_height - tick_height/2;

		LLGLSUIDefault gls_ui;
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		for (tick_value = mParameters.mMinBar + mMinShift; tick_value <= (mParameters.mMaxBar + mMaxShift); tick_value += mParameters.mTickSpacing)
		{
			left = llfloor((tick_value - (mParameters.mMinBar + mMinShift))*value_scale);
			right = left + tick_width;
			gl_rect_2d(left, top, right, bottom, LLColor4(1.f, 1.f, 1.f, 0.1f));
		}

		// Draw the tick labels (and big ticks).
		bottom = bar_top - bar_height - tick_height;
		for (tick_value = mParameters.mMinBar + mMinShift; tick_value <= (mParameters.mMaxBar + mMaxShift); tick_value += mParameters.mLabelSpacing)
		{
			left = llfloor((tick_value - (mParameters.mMinBar + mMinShift))*value_scale);
			right = left + tick_width;
			gl_rect_2d(left, top, right, bottom, LLColor4(1.f, 1.f, 1.f, 0.25f));

			// draw labels for the tick marks
			LLFontGL::getFontMonospace()->renderUTF8(llformat("%.*f", mParameters.mPrecision, tick_value), 0, left - 1, bar_top - bar_height - tick_height,
											 LLColor4(1.f, 1.f, 1.f, 0.5f),
											 LLFontGL::LEFT, LLFontGL::TOP);
		}

		// Now, draw the bars
		top = bar_top;
		bottom = bar_top - bar_height;

		// draw background bar.
		left = 0;
		right = width;
		gl_rect_2d(left, top, right, bottom, LLColor4(0.f, 0.f, 0.f, 0.25f));

		if (mStatp->getNumValues() == 0)
		{
			// No data, don't draw anything...
			return;
		}
		// draw min and max
		left = (S32) ((min - (mParameters.mMinBar + mMinShift)) * value_scale);

		if (left < 0)
		{
			left = 0;
			LL_WARNS() << "Min:" << min << LL_ENDL;
		}

		right = (S32) ((max - (mParameters.mMinBar + mMinShift)) * value_scale);
		gl_rect_2d(left, top, right, bottom, LLColor4(1.f, 0.f, 0.f, 0.25f));

		S32 num_values = mStatp->getNumValues() - 1;
		if (mDisplayHistory)
		{
			S32 i;
			for (i = 0; i < num_values; i++)
			{
				if (i == mStatp->getNextBin())
				{
					continue;
				}
				if (mParameters.mPerSec)
				{
					left = (S32)((mStatp->getPrevPerSec(i) - (mParameters.mMinBar + mMinShift)) * value_scale);
					gl_rect_2d(left, bottom+i+1, left+1, bottom+i, LLColor4(1.f, 0.f, 0.f, 1.f));
				}
				else
				{
					left = (S32)((mStatp->getPrev(i) - (mParameters.mMinBar + mMinShift)) * value_scale);
					gl_rect_2d(left, bottom+i+1, left + 1, bottom+i, LLColor4(1.f, 0.f, 0.f, 1.f));
				}
			}
		}
		else
		{
			// draw current
			left = (S32) ((current - (mParameters.mMinBar + mMinShift)) * value_scale) - 1;
			gl_rect_2d(left, top, left + 2, bottom, LLColor4(1.f, 0.f, 0.f, 1.f));
		}

		// draw mean bar
		top = bar_top + 2;
		bottom = bar_top - bar_height - 2;
		left = (S32)((mean - (mParameters.mMinBar + mMinShift)) * value_scale) - 1;
		gl_rect_2d(left, top, left + 2, bottom, LLColor4(0.f, 1.f, 0.f, 1.f));
	}
	
	LLView::draw();
}

LLRect LLStatBar::getRequiredRect()
{
	LLRect rect;

	if (mDisplayBar)
	{
		if (mDisplayHistory)
		{
			rect.mTop = 35 + mStatp->getNumBins();
		}
		else
		{
			rect.mTop = 40;
		}
	}
	else
	{
		rect.mTop = 14;
	}
	return rect;
}
