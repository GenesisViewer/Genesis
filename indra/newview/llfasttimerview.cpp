/** 
 * @file llfasttimerview.cpp
 * @brief LLFastTimerView class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llfasttimerview.h"

#include "llviewerwindow.h"
#include "llrect.h"
#include "llerror.h"
#include "llgl.h"
#include "llimagepng.h"
#include "llrender.h"
#include "llrendertarget.h"
#include "lllocalcliprect.h"
#include "llmath.h"
#include "llfontgl.h"
#include "llsdserialize.h"
#include "llbutton.h"

#include "llappviewer.h"
#include "llviewertexturelist.h"
#include "llui.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llstat.h"

#include "llfasttimer.h"
#include "lltreeiterators.h"
#include "llviewerstats.h"

//////////////////////////////////////////////////////////////////////////////

static const S32 MAX_VISIBLE_HISTORY = 10;
static const S32 LINE_GRAPH_HEIGHT = 240;

//static const int FTV_DISPLAY_NUM  = (sizeof(ft_display_table)/sizeof(ft_display_table[0]));
static S32 FTV_NUM_TIMERS;
const S32 FTV_MAX_DEPTH = 8;

std::vector<LLFastTimer::NamedTimer*> ft_display_idx; // line of table entry for display purposes (for collapse)

typedef LLTreeDFSIter<LLFastTimer::NamedTimer, LLFastTimer::NamedTimer::child_const_iter> timer_tree_iterator_t;

BOOL LLFastTimerView::sAnalyzePerformance = FALSE;

static timer_tree_iterator_t begin_timer_tree(LLFastTimer::NamedTimer& id) 
{ 
	return timer_tree_iterator_t(&id, 
							boost::bind(boost::mem_fn(&LLFastTimer::NamedTimer::beginChildren), _1), 
							boost::bind(boost::mem_fn(&LLFastTimer::NamedTimer::endChildren), _1));
}

static timer_tree_iterator_t end_timer_tree() 
{ 
	return timer_tree_iterator_t(); 
}


LLFastTimerView::LLFastTimerView(const std::string& name, const LLRect& rect)
 : LLFloater(name, rect, std::string(), FALSE, 1, 1, FALSE, FALSE, TRUE),
 	mHoverTimer(NULL)
{
	setVisible(FALSE);
	mDisplayMode = 0;
	mAvgCountTotal = 0;
	mMaxCountTotal = 0;
	mDisplayCenter = ALIGN_CENTER;
	mDisplayCalls = 0;
	mDisplayHz = 0;
	mScrollIndex = 0;
	mHoverID = NULL;
	mHoverBarIndex = -1;
	FTV_NUM_TIMERS = LLFastTimer::NamedTimer::instanceCount();
	mPrintStats = -1;	
	mAverageCyclesPerTimer = 0;
	// <FS:LO> Making the ledgend part of fast timers scrollable
	mOverLegend = false;
	mScrollOffset = 0;
	// </FS:LO>
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_fast_timers.xml");
}

void LLFastTimerView::onPause()
{
	LLFastTimer::sPauseHistory = !LLFastTimer::sPauseHistory;
	// reset scroll to bottom when unpausing
	if (!LLFastTimer::sPauseHistory)
	{
		mScrollIndex = 0;
		LLFastTimer::sResetHistory = true;
		getChild<LLButton>("pause_btn")->setLabel(getString("pause"));
	}
	else
	{
		getChild<LLButton>("pause_btn")->setLabel(getString("run"));
	}
}

void LLFastTimerView::onPauseHandler(void *data)
{
	((LLFastTimerView*)data)->onPause();
}

BOOL LLFastTimerView::postBuild()
{
	LLButton& pause_btn = getChildRef<LLButton>("pause_btn");

	pause_btn.setClickedCallback(&LLFastTimerView::onPauseHandler,this);
	//pause_btn.setCommitCallback(boost::bind(&LLFastTimerView::onPause, this));

	return TRUE;
}

BOOL LLFastTimerView::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (mHoverTimer )
	{
		// right click collapses timers
		if (!mHoverTimer->getCollapsed())
		{
			mHoverTimer->setCollapsed(true);
		}
		else if (mHoverTimer->getParent())
		{
			mHoverTimer->getParent()->setCollapsed(true);
		}
		return TRUE;
	}
	else if (mBarRect.pointInRect(x, y))
	{
		S32 bar_idx = MAX_VISIBLE_HISTORY - ((y - mBarRect.mBottom) * (MAX_VISIBLE_HISTORY + 2) / mBarRect.getHeight());
		bar_idx = llclamp(bar_idx, 0, MAX_VISIBLE_HISTORY);
		mPrintStats = LLFastTimer::NamedTimer::HISTORY_NUM - mScrollIndex - bar_idx;
		return TRUE;
	}
	return LLFloater::handleRightMouseDown(x, y, mask);
}

LLFastTimer::NamedTimer* LLFastTimerView::getLegendID(S32 y)
{
	S32 idx = (getRect().getHeight() - y) / ((S32) LLFontGL::getFontMonospace()->getLineHeight()+2) - 5;

	if (idx >= 0 && idx < (S32)ft_display_idx.size())
	{
		return ft_display_idx[idx];
	}
	
	return NULL;
}

BOOL LLFastTimerView::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	for(timer_tree_iterator_t it = begin_timer_tree(LLFastTimer::NamedTimer::getRootNamedTimer());
		it != end_timer_tree();
		++it)
	{
		(*it)->setCollapsed(false);
	}
	return TRUE;
}

BOOL LLFastTimerView::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (x < mBarRect.mLeft) 
	{
		LLFastTimer::NamedTimer* idp = getLegendID(y);
		if (idp)
		{
			idp->setCollapsed(!idp->getCollapsed());
		}
	}
	else if (mHoverTimer)
	{
		//left click drills down by expanding timers
		mHoverTimer->setCollapsed(false);
	}
	else if (mask & MASK_ALT)
	{
		if (mask & MASK_CONTROL)
		{
			mDisplayHz = !mDisplayHz;	
		}
		else
		{
			mDisplayCalls = !mDisplayCalls;
		}
	}
	else if (mask & MASK_SHIFT)
	{
		if (++mDisplayMode > 3)
			mDisplayMode = 0;
	}
	else if (mask & MASK_CONTROL)
	{
		mDisplayCenter = (ChildAlignment)((mDisplayCenter + 1) % ALIGN_COUNT);
	}
	else if (mGraphRect.pointInRect(x, y))
	{
		gFocusMgr.setMouseCapture(this);
		return TRUE;
	}
	//else
	//{
	//	// pause/unpause
	//	LLFastTimer::sPauseHistory = !LLFastTimer::sPauseHistory;
	//	// reset scroll to bottom when unpausing
	//	if (!LLFastTimer::sPauseHistory)
	//	{
	//		mScrollIndex = 0;
	//	}
	//}
	return LLFloater::handleMouseDown(x, y, mask);
}

BOOL LLFastTimerView::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);
	}
	return LLFloater::handleMouseUp(x, y, mask);;
}

BOOL LLFastTimerView::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		F32 lerp = llclamp(1.f - (F32) (x - mGraphRect.mLeft) / (F32) mGraphRect.getWidth(), 0.f, 1.f);
		mScrollIndex = ll_round( lerp * (F32)(LLFastTimer::NamedTimer::HISTORY_NUM - MAX_VISIBLE_HISTORY));
		mScrollIndex = llclamp(	mScrollIndex, 0, LLFastTimer::getLastFrameIndex());
		return TRUE;
	}
	mHoverTimer = NULL;
	mHoverID = NULL;
	mOverLegend = false; // <FS:LO> Making the ledgend part of fast timers scrollable

	if(LLFastTimer::sPauseHistory && mBarRect.pointInRect(x, y))
	{
		mHoverBarIndex = llmin(LLFastTimer::getCurFrameIndex() - 1, 
								MAX_VISIBLE_HISTORY - ((y - mBarRect.mBottom) * (MAX_VISIBLE_HISTORY + 2) / mBarRect.getHeight()));
		if (mHoverBarIndex == 0)
		{
			return TRUE;
		}
		else if (mHoverBarIndex == -1)
		{
			mHoverBarIndex = 0;
		}

		S32 i = 0;
		for(timer_tree_iterator_t it = begin_timer_tree(LLFastTimer::NamedTimer::getRootNamedTimer());
			it != end_timer_tree();
			++it, ++i)
		{
			// is mouse over bar for this timer?
			if (x > mBarStart[mHoverBarIndex][i] &&
				x < mBarEnd[mHoverBarIndex][i])
			{
				mHoverID = (*it);
				if (mHoverTimer != *it)
				{
					// could be that existing tooltip is for a parent and is thus
					// covering region for this new timer, go ahead and unblock 
					// so we can create a new tooltip
					//LLToolTipMgr::instance().unblockToolTips();
					gViewerWindow->unblockToolTips();
					mHoverTimer = (*it);
				}

				mToolTipRect.set(mBarStart[mHoverBarIndex][i], 
					mBarRect.mBottom + ll_round(((F32)(MAX_VISIBLE_HISTORY - mHoverBarIndex + 1)) * ((F32)mBarRect.getHeight() / ((F32)MAX_VISIBLE_HISTORY + 2.f))),
					mBarEnd[mHoverBarIndex][i],
					mBarRect.mBottom + ll_round((F32)(MAX_VISIBLE_HISTORY - mHoverBarIndex) * ((F32)mBarRect.getHeight() / ((F32)MAX_VISIBLE_HISTORY + 2.f))));
			}

			if ((*it)->getCollapsed())
			{
				it.skipDescendants();
			}
		}
	}
	else if (x < mBarRect.mLeft) 
	{
		LLFastTimer::NamedTimer* timer_id = getLegendID(y);
		if (timer_id)
		{
			mHoverID = timer_id;
		}
		mOverLegend = true; // <FS:LO> Making the ledgend part of fast timers scrollable
	}
	
	return LLFloater::handleHover(x, y, mask);
}


BOOL LLFastTimerView::handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect_screen)
{
	std::string tool_tip;
	if(LLFastTimer::sPauseHistory && mBarRect.pointInRect(x, y))
	{
		// tooltips for timer bars
		if (mHoverTimer)
		{
			tool_tip = mHoverTimer->getToolTip(LLFastTimer::NamedTimer::HISTORY_NUM - mScrollIndex - mHoverBarIndex);
		}
	}
	else
	{
		// tooltips for timer legend
		if (x < mBarRect.mLeft) 
		{
			LLFastTimer::NamedTimer* idp = getLegendID(y);
			if (idp)
			{
				tool_tip = idp->getToolTip();
			}
		}
	}

	if(!tool_tip.empty())
	{
		msg = tool_tip;
		localPointToScreen( 
			0, 0, 
			&(sticky_rect_screen->mLeft), &(sticky_rect_screen->mBottom) );
		localPointToScreen(
			getRect().getWidth(), getRect().getHeight(),
			&(sticky_rect_screen->mRight), &(sticky_rect_screen->mTop) );
		return TRUE;
	}
	else
		return LLFloater::handleToolTip(x, y, msg, sticky_rect_screen);
}

BOOL LLFastTimerView::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	//LLFastTimer::sPauseHistory = TRUE;
	//mScrollIndex = llclamp(	mScrollIndex + clicks,
							//0,
							//llmin(LLFastTimer::getLastFrameIndex(), (S32)LLFastTimer::NamedTimer::HISTORY_NUM - MAX_VISIBLE_HISTORY));
	// <FS:LO> Making the ledgend part of fast timers scrollable
	if(mOverLegend)
	{
		mScrollOffset += clicks;
		S32 count = 0;
		for (timer_tree_iterator_t it = begin_timer_tree(LLFastTimer::NamedTimer::getRootNamedTimer());
			it != timer_tree_iterator_t();
			++it)
		{
			count++;
			LLFastTimer::NamedTimer* idp = (*it);
			if (idp->getCollapsed()) 
			{
				it.skipDescendants();
			}
		}
		mScrollOffset = llclamp(mScrollOffset,0,count-5);
	}
	else
	{
		LLFastTimer::sPauseHistory = TRUE;
		mScrollIndex = llclamp(	mScrollIndex + clicks,
								0,
								llmin(LLFastTimer::getLastFrameIndex(), (S32)LLFastTimer::NamedTimer::HISTORY_NUM - MAX_VISIBLE_HISTORY));
	}
	// </FS:LO>
	return TRUE;
}

static LLTrace::BlockTimerStatHandle FTM_RENDER_TIMER("Timers", true);

static std::map<LLFastTimer::NamedTimer*, LLColor4> sTimerColors;

void LLFastTimerView::draw()
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER_TIMER);
	
	std::string tdesc;

	F64 clock_freq = (F64)LLFastTimer::countsPerSecond();
	F64 iclock_freq = 1000.0 / clock_freq;
	
	S32 margin = 10;
	S32 height = getRect().getHeight();
	S32 width = getRect().getWidth();
	
	LLRect new_rect;
	new_rect.setLeftTopAndSize(getRect().mLeft, getRect().mTop, width, height);
	setRect(new_rect);

	S32 left, top, right, bottom;
	S32 x, y, barw, barh, dx, dy;
	S32 texth;
	LLPointer<LLUIImage> box_imagep = LLUI::getUIImage("Rounded_Square");

	// Draw the window background
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0, LLColor4(0.f, 0.f, 0.f, 0.25f));
	
	S32 xleft = margin;
	S32 ytop = margin;
	
	mAverageCyclesPerTimer = LLFastTimer::sTimerCalls == 0 
		? 0 
		: ll_round(lerp((F32)mAverageCyclesPerTimer, (F32)(LLFastTimer::sTimerCycles / (U64)LLFastTimer::sTimerCalls), 0.1f));
	LLFastTimer::sTimerCycles = 0;
	LLFastTimer::sTimerCalls = 0;

	// Draw some help
	{
		
		x = xleft;
		y = height - ytop;
		texth = (S32)LLFontGL::getFontMonospace()->getLineHeight();

#if TIME_FAST_TIMERS
		tdesc = llformat("Cycles per timer call: %d", mAverageCyclesPerTimer);
		LLFontGL::getFontMonospace()->renderUTF8(tdesc, 0, x, y, LLColor4::white, LLFontGL::LEFT, LLFontGL::TOP);
#else
		char modedesc[][32] = {
			"2 x Average ",
			"Max         ",
			"Recent Max  ",
			"100 ms      "
		};
		char centerdesc[][32] = {
			"Left      ",
			"Centered  ",
			"Ordered   "
		};

		tdesc = llformat("Full bar = %s [Click to pause/reset] [SHIFT-Click to toggle]",modedesc[mDisplayMode]);
		LLFontGL::getFontMonospace()->renderUTF8(tdesc, 0, x, y, LLColor4::white, LLFontGL::LEFT, LLFontGL::TOP);

		x = xleft, y -= (texth + 2);
		tdesc = llformat("Justification = %s [CTRL-Click to toggle]",centerdesc[mDisplayCenter]);
		LLFontGL::getFontMonospace()->renderUTF8(tdesc, 0, x, y, LLColor4::white, LLFontGL::LEFT, LLFontGL::TOP);
		y -= (texth + 2);

		LLFontGL::getFontMonospace()->renderUTF8(std::string("[Right-Click log selected] [ALT-Click toggle counts] [ALT-SHIFT-Click sub hidden]"),
										 0, x, y, LLColor4::white, LLFontGL::LEFT, LLFontGL::TOP);
#endif
		y -= (texth + 2);
	}

	S32 histmax = llmin(LLFastTimer::getLastFrameIndex()+1, MAX_VISIBLE_HISTORY);
		
	// Draw the legend
	xleft = margin;
	ytop = y;

	y -= (texth + 2);

	sTimerColors[&LLFastTimer::NamedTimer::getRootNamedTimer()] = LLColor4::grey;

	F32 hue = 0.f;
	// <ALCH:LL> Move color generation down to be in the next loop.
	/*for (timer_tree_iterator_t it = begin_timer_tree(LLFastTimer::NamedTimer::getRootNamedTimer());
		it != timer_tree_iterator_t();
		++it)
	{
		LLFastTimer::NamedTimer* idp = (*it);

		const F32 HUE_INCREMENT = 0.23f;
		hue = fmodf(hue + HUE_INCREMENT, 1.f);
		// saturation increases with depth
		F32 saturation = clamp_rescale((F32)idp->getDepth(), 0.f, 3.f, 0.f, 1.f);
		// lightness alternates with depth
		F32 lightness = idp->getDepth() % 2 ? 0.5f : 0.6f;

		LLColor4 child_color;
		child_color.setHSL(hue, saturation, lightness);

		sTimerColors[idp] = child_color;
	}*/
	// </ALCH:LL>

	const S32 LEGEND_WIDTH = 220;
	{
		LLLocalClipRect clip(LLRect(margin, y, LEGEND_WIDTH, margin));
		S32 cur_line = 0;
		ft_display_idx.clear();
		std::map<LLFastTimer::NamedTimer*, S32> display_line;
		S32 mScrollOffset_tmp = mScrollOffset; // <FS:LO> Making the ledgend part of fast timers scrollable
		for (timer_tree_iterator_t it = begin_timer_tree(LLFastTimer::NamedTimer::getRootNamedTimer());
			it != timer_tree_iterator_t();
			++it)
		{
			LLFastTimer::NamedTimer* idp = (*it);
			// <ALCH:LL> Move color generation down to be in the next loop.
			const F32 HUE_INCREMENT = 0.23f;
			hue = fmodf(hue + HUE_INCREMENT, 1.f);
			// saturation increases with depth
			F32 saturation = clamp_rescale((F32)idp->getDepth(), 0.f, 3.f, 0.f, 1.f);
			// lightness alternates with depth
			F32 lightness = idp->getDepth() % 2 ? 0.5f : 0.6f;

			LLColor4 child_color;
			child_color.setHSL(hue, saturation, lightness);

			sTimerColors[idp] = child_color;
			// </ALCH:LL>

			// <FS:LO> Making the ledgend part of fast timers scrollable
			if(mScrollOffset_tmp)
			{
				--mScrollOffset_tmp;
				if (idp->getCollapsed()) 
				{
					it.skipDescendants();
				}
				continue;
			}
			// </FS:LO>
			display_line[idp] = cur_line;
			ft_display_idx.push_back(idp);
			cur_line++;

			x = xleft;

			left = x; right = x + texth;
			top = y; bottom = y - texth;
			S32 scale_offset = 0;
			if (idp == mHoverID)
			{
				scale_offset = llfloor(sinf(mHighlightTimer.getElapsedTimeF32() * 6.f) * 2.f);
			}
			gl_rect_2d(left - scale_offset, top + scale_offset, right + scale_offset, bottom - scale_offset, sTimerColors[idp]);

			F32 ms = 0;
			S32 calls = 0;
			if (mHoverBarIndex > 0 && mHoverID)
			{
				S32 hidx = LLFastTimer::NamedTimer::HISTORY_NUM - mScrollIndex - mHoverBarIndex;
				U64 ticks = idp->getHistoricalCount(hidx);
				ms = (F32)((F64)ticks * iclock_freq);
				calls = (S32)idp->getHistoricalCalls(hidx);
			}
			else
			{
				U64 ticks = idp->getCountAverage();
				ms = (F32)((F64)ticks * iclock_freq);
				calls = (S32)idp->getCallAverage();
			}

			if (mDisplayCalls)
			{
				tdesc = llformat("%s (%d)",idp->getName().c_str(),calls);
			}
			else
			{
				tdesc = llformat("%s [%.1f]",idp->getName().c_str(),ms);
			}
			dx = (texth+4) + idp->getDepth()*8;

			LLColor4 color = LLColor4::white;
			if (idp->getDepth() > 0)
			{
				S32 line_start_y = (top + bottom) / 2;
				S32 line_end_y = line_start_y + ((texth + 2) * (cur_line - display_line[idp->getParent()])) - texth;
				gl_line_2d(x + dx - 8, line_start_y, x + dx, line_start_y, color);
				S32 line_x = x + (texth + 4) + ((idp->getDepth() - 1) * 8);
				gl_line_2d(line_x, line_start_y, line_x, line_end_y, color);
				if (idp->getCollapsed() && !idp->getChildren().empty())
				{
					gl_line_2d(line_x+4, line_start_y-3, line_x+4, line_start_y+4, color);
				}
			}

			x += dx;
			BOOL is_child_of_hover_item = (idp == mHoverID);
			LLFastTimer::NamedTimer* next_parent = idp->getParent();
			while(!is_child_of_hover_item && next_parent)
			{
				is_child_of_hover_item = (mHoverID == next_parent);
				next_parent = next_parent->getParent();
			}

			LLFontGL::getFontSansSerifSmall()->renderUTF8(tdesc, 0, 
											x, y, 
											color, 
											LLFontGL::LEFT, LLFontGL::TOP, 
											is_child_of_hover_item ? LLFontGL::BOLD : LLFontGL::NORMAL);

			y -= (texth + 2);

			if (idp->getCollapsed()) 
			{
				it.skipDescendants();
			}
		}
	}

	xleft += LEGEND_WIDTH + 8;
	// ytop = ytop;

	// update rectangle that includes timer bars
	mBarRect.mLeft = xleft;
	mBarRect.mRight = getRect().getWidth();
	mBarRect.mTop = ytop - ((S32)LLFontGL::getFontMonospace()->getLineHeight() + 4);
	mBarRect.mBottom = margin + LINE_GRAPH_HEIGHT;

	y = ytop;
	barh = (ytop - margin - LINE_GRAPH_HEIGHT) / (MAX_VISIBLE_HISTORY + 2);
	dy = barh>>2; // spacing between bars
	if (dy < 1) dy = 1;
	barh -= dy;
	barw = width - xleft - margin;

	// Draw the history bars
	if (LLFastTimer::getLastFrameIndex() >= 0)
	{	
		LLLocalClipRect clip(LLRect(xleft, ytop, getRect().getWidth() - margin, margin));

		U64 totalticks;
		if (!LLFastTimer::sPauseHistory)
		{
			U64 ticks = LLFastTimer::NamedTimer::getRootNamedTimer().getHistoricalCount(mScrollIndex);

			if (LLFastTimer::getCurFrameIndex() >= 10)
			{
				U64 framec = LLFastTimer::getCurFrameIndex();
				U64 avg = (U64)mAvgCountTotal;
				mAvgCountTotal = (avg*framec + ticks) / (framec + 1);
				if (ticks > mMaxCountTotal)
				{
					mMaxCountTotal = ticks;
				}
			}

			if (ticks < mAvgCountTotal/100 || ticks > mAvgCountTotal*100)
			{
				LLFastTimer::sResetHistory = true;
			}

			if (LLFastTimer::getCurFrameIndex() < 10 || LLFastTimer::sResetHistory)
			{
				mAvgCountTotal = ticks;
				mMaxCountTotal = ticks;
				LLFastTimer::sResetHistory = false;
			}
		}

		if (mDisplayMode == 0)
		{
			totalticks = mAvgCountTotal*2;
		}
		else if (mDisplayMode == 1)
		{
			totalticks = mMaxCountTotal;
		}
		else if (mDisplayMode == 2)
		{
			// Calculate the max total ticks for the current history
			totalticks = 0;
			for (S32 j=0; j<histmax; j++)
			{
				U64 ticks = LLFastTimer::NamedTimer::getRootNamedTimer().getHistoricalCount(j);

				if (ticks > totalticks)
					totalticks = ticks;
			}
		}
		else
		{
			totalticks = (U64)(clock_freq * .1); // 100 ms
		}
		
		// Draw MS ticks
		{
			U32 ms = (U32)((F64)totalticks * iclock_freq) ;

			tdesc = llformat("%.1f ms |", (F32)ms*.25f);
			x = xleft + barw/4 - LLFontGL::getFontMonospace()->getWidth(tdesc);
			LLFontGL::getFontMonospace()->renderUTF8(tdesc, 0, x, y, LLColor4::white,
										 LLFontGL::LEFT, LLFontGL::TOP);
			
			tdesc = llformat("%.1f ms |", (F32)ms*.50f);
			x = xleft + barw/2 - LLFontGL::getFontMonospace()->getWidth(tdesc);
			LLFontGL::getFontMonospace()->renderUTF8(tdesc, 0, x, y, LLColor4::white,
										 LLFontGL::LEFT, LLFontGL::TOP);
			
			tdesc = llformat("%.1f ms |", (F32)ms*.75f);
			x = xleft + (barw*3)/4 - LLFontGL::getFontMonospace()->getWidth(tdesc);
			LLFontGL::getFontMonospace()->renderUTF8(tdesc, 0, x, y, LLColor4::white,
										 LLFontGL::LEFT, LLFontGL::TOP);
			
			tdesc = llformat( "%d ms |", ms);
			x = xleft + barw - LLFontGL::getFontMonospace()->getWidth(tdesc);
			LLFontGL::getFontMonospace()->renderUTF8(tdesc, 0, x, y, LLColor4::white,
										 LLFontGL::LEFT, LLFontGL::TOP);
		}

		// Draw borders
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			gGL.color4f(0.5f,0.5f,0.5f,0.5f);

			S32 by = y + 2;
			
			y -= ((S32)LLFontGL::getFontMonospace()->getLineHeight() + 4);

			//heading
			gl_rect_2d(xleft-5, by, getRect().getWidth()-5, y+5, FALSE);

			//tree view
			gl_rect_2d(5, by, xleft-10, 5, FALSE);

			by = y + 5;
			//average bar
			gl_rect_2d(xleft-5, by, getRect().getWidth()-5, by-barh-dy-5, FALSE);
			
			by -= barh*2+dy;
			
			//current frame bar
			gl_rect_2d(xleft-5, by, getRect().getWidth()-5, by-barh-dy-2, FALSE);
			
			by -= barh+dy+1;
			
			//history bars
			gl_rect_2d(xleft-5, by, getRect().getWidth()-5, LINE_GRAPH_HEIGHT-barh-dy-2, FALSE);			
			
			by = LINE_GRAPH_HEIGHT-barh-dy-7;
			
			//line graph
			mGraphRect = LLRect(xleft-5, by, getRect().getWidth()-5, 5);
			
			gl_rect_2d(mGraphRect, FALSE);
		}
		
		mBarStart.clear();
		mBarEnd.clear();

		// Draw bars for each history entry
		// Special: -1 = show running average
		gGL.getTexUnit(0)->bind(box_imagep->getImage());
		for (S32 j=-1; j<histmax && y > LINE_GRAPH_HEIGHT; j++)
		{
			mBarStart.push_back(std::vector<S32>());
			mBarEnd.push_back(std::vector<S32>());
			int sublevel_dx[FTV_MAX_DEPTH];
			int sublevel_left[FTV_MAX_DEPTH];
			int sublevel_right[FTV_MAX_DEPTH];
			S32 tidx;
			if (j >= 0)
			{
				tidx = LLFastTimer::NamedTimer::HISTORY_NUM - j - 1 - mScrollIndex;
			}
			else
			{
				tidx = -1;
			}
			
			x = xleft;
			
			// draw the bars for each stat
			std::vector<S32> xpos;
			std::vector<S32> deltax;
			xpos.push_back(xleft);
			
			LLFastTimer::NamedTimer* prev_id = NULL;

			S32 i = 0;
			for(timer_tree_iterator_t it = begin_timer_tree(LLFastTimer::NamedTimer::getRootNamedTimer());
				it != end_timer_tree();
				++it, ++i)
			{
				LLFastTimer::NamedTimer* idp = (*it);
				F32 frac = tidx == -1
					? (F32)idp->getCountAverage() / (F32)totalticks 
					: (F32)idp->getHistoricalCount(tidx) / (F32)totalticks;
		
				dx = ll_round(frac * (F32)barw);
				S32 prev_delta_x = deltax.empty() ? 0 : deltax.back();
				deltax.push_back(dx);
				
				int level = idp->getDepth() - 1;
				
				while ((S32)xpos.size() > level + 1)
				{
					xpos.pop_back();
				}
				left = xpos.back();
				
				if (level == 0)
				{
					sublevel_left[level] = xleft;
					sublevel_dx[level] = dx;
					sublevel_right[level] = sublevel_left[level] + sublevel_dx[level];
				}
				else if (prev_id && prev_id->getDepth() < idp->getDepth())
				{
					U64 sublevelticks = 0;

					for (LLFastTimer::NamedTimer::child_const_iter it = prev_id->beginChildren();
						it != prev_id->endChildren();
						++it)
					{
						sublevelticks += (tidx == -1)
							? (*it)->getCountAverage() 
							: (*it)->getHistoricalCount(tidx);
					}

					F32 subfrac = (F32)sublevelticks / (F32)totalticks;
					sublevel_dx[level] = (int)(subfrac * (F32)barw + .5f);

					if (mDisplayCenter == ALIGN_CENTER)
					{
						left += (prev_delta_x - sublevel_dx[level])/2;
					}
					else if (mDisplayCenter == ALIGN_RIGHT)
					{
						left += (prev_delta_x - sublevel_dx[level]);
					}

					sublevel_left[level] = left;
					sublevel_right[level] = sublevel_left[level] + sublevel_dx[level];
				}				

				right = left + dx;
				xpos.back() = right;
				xpos.push_back(left);
				
				mBarStart.back().push_back(left);
				mBarEnd.back().push_back(right);

				top = y;
				bottom = y - barh;

				if (right > left)
				{
					//U32 rounded_edges = 0;
					LLColor4 color = sTimerColors[idp];// *ft_display_table[i].color;
					S32 scale_offset = 0;

					BOOL is_child_of_hover_item = (idp == mHoverID);
					LLFastTimer::NamedTimer* next_parent = idp->getParent();
					while(!is_child_of_hover_item && next_parent)
					{
						is_child_of_hover_item = (mHoverID == next_parent);
						next_parent = next_parent->getParent();
					}

					if (idp == mHoverID)
					{
						scale_offset = llfloor(sinf(mHighlightTimer.getElapsedTimeF32() * 6.f) * 3.f);
						//color = lerp(color, LLColor4::black, -0.4f);
					}
					else if (mHoverID != NULL && !is_child_of_hover_item)
					{
						color = lerp(color, LLColor4::grey, 0.8f);
					}

					gGL.color4fv(color.mV);
					F32 start_fragment = llclamp((F32)(left - sublevel_left[level]) / (F32)sublevel_dx[level], 0.f, 1.f);
					F32 end_fragment = llclamp((F32)(right - sublevel_left[level]) / (F32)sublevel_dx[level], 0.f, 1.f);
					LLRect rect(sublevel_left[level], top - level + scale_offset, sublevel_right[level], bottom + level - scale_offset);
					gl_segmented_rect_2d_fragment_tex(rect, box_imagep->getTextureWidth(), box_imagep->getTextureHeight(), 16, start_fragment, end_fragment);

				}

				if ((*it)->getCollapsed())
				{
					it.skipDescendants();
				}
		
				prev_id = idp;
			}
			y -= (barh + dy);
			if (j < 0)
				y -= barh;
		}
		
		//draw line graph history
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			LLLocalClipRect clip(mGraphRect);
			
			//normalize based on last frame's maximum
			static U64 last_max = 0;
			static F32 alpha_interp = 0.f;
			U64 max_ticks = llmax(last_max, (U64) 1);			
			F32 ms = (F32)((F64)max_ticks * iclock_freq);
			
			//display y-axis range
			std::string tdesc;
			 if (mDisplayCalls)
				tdesc = llformat("%d calls", (int)max_ticks);
			else if (mDisplayHz)
				tdesc = llformat("%d Hz", (int)max_ticks);
			else
				tdesc = llformat("%4.2f ms", ms);
							
			x = mGraphRect.mRight - LLFontGL::getFontMonospace()->getWidth(tdesc)-5;
			y = mGraphRect.mTop - ((S32)LLFontGL::getFontMonospace()->getLineHeight());
 
			LLFontGL::getFontMonospace()->renderUTF8(tdesc, 0, x, y, LLColor4::white,
										 LLFontGL::LEFT, LLFontGL::TOP);

			//highlight visible range
			{
				S32 first_frame = LLFastTimer::NamedTimer::HISTORY_NUM - mScrollIndex;
				S32 last_frame = first_frame - MAX_VISIBLE_HISTORY;
				
				F32 frame_delta = ((F32) (mGraphRect.getWidth()))/(LLFastTimer::NamedTimer::HISTORY_NUM-1);
				
				F32 right = (F32) mGraphRect.mLeft + frame_delta*first_frame;
				F32 left = (F32) mGraphRect.mLeft + frame_delta*last_frame;
				
				gGL.color4f(0.5f,0.5f,0.5f,0.3f);
				gl_rect_2d((S32) left, mGraphRect.mTop, (S32) right, mGraphRect.mBottom);
				
				if (mHoverBarIndex >= 0)
				{
					S32 bar_frame = first_frame - mHoverBarIndex;
					F32 bar = (F32) mGraphRect.mLeft + frame_delta*bar_frame;

					gGL.color4f(0.5f,0.5f,0.5f,1);
				
					gGL.begin(LLRender::LINES);
					gGL.vertex2i((S32)bar, mGraphRect.mBottom);
					gGL.vertex2i((S32)bar, mGraphRect.mTop);
					gGL.end();
				}
			}
			
			U64 cur_max = 0;
			for(timer_tree_iterator_t it = begin_timer_tree(LLFastTimer::NamedTimer::getRootNamedTimer());
				it != end_timer_tree();
				++it)
			{
				LLFastTimer::NamedTimer* idp = (*it);
				
				//fatten highlighted timer
				if (mHoverID == idp)
				{
					gGL.flush();
					gGL.setLineWidth(3);
				}
			
				const F32 * col = sTimerColors[idp].mV;// ft_display_table[idx].color->mV;
				
				F32 alpha = 1.f;
				
				if (mHoverID != NULL &&
					idp != mHoverID)
				{	//fade out non-highlighted timers
					if (idp->getParent() != mHoverID)
					{
						alpha = alpha_interp;
					}
				}

				gGL.color4f(col[0], col[1], col[2], alpha);				
				gGL.begin(LLRender::TRIANGLE_STRIP);
				for (U32 j = llmax(0, LLFastTimer::NamedTimer::HISTORY_NUM - LLFastTimer::getLastFrameIndex());
					j < (U32)LLFastTimer::NamedTimer::HISTORY_NUM;
					j++)
				{
					U64 ticks = idp->getHistoricalCount(j);

					if (mDisplayHz)
					{
						F64 tc = (F64) (ticks+1) * iclock_freq;
						tc = 1000.f/tc;
						ticks = llmin((U64) tc, (U64) 1024);
					}
					else if (mDisplayCalls)
					{
						ticks = (S32)idp->getHistoricalCalls(j);
					}
										
					if (alpha == 1.f)
					{ 
						//normalize to highlighted timer
						cur_max = llmax(cur_max, ticks);
					}
					F32 x = mGraphRect.mLeft + ((F32) (mGraphRect.getWidth()))/(LLFastTimer::NamedTimer::HISTORY_NUM-1)*j;
					F32 y = mGraphRect.mBottom + (F32) mGraphRect.getHeight()/max_ticks*ticks;
					gGL.vertex2f(x,y);
					gGL.vertex2f(x,mGraphRect.mBottom);
				}
				gGL.end();
				
				if (mHoverID == idp)
				{
					gGL.flush();
					gGL.setLineWidth(1);
				}

				if (idp->getCollapsed())
				{	
					//skip hidden timers
					it.skipDescendants();
				}
			}
			
			//interpolate towards new maximum
			last_max = (U64) lerp((F32)last_max, (F32) cur_max, LLSmoothInterpolation::getInterpolant(0.1f));
			if (last_max - cur_max <= 1 ||  cur_max - last_max  <= 1)
			{
				last_max = cur_max;
			}
			F32 alpha_target = last_max > cur_max ?
								llmin((F32) last_max/ (F32) cur_max - 1.f,1.f) :
								llmin((F32) cur_max/ (F32) last_max - 1.f,1.f);
			alpha_interp = lerp(alpha_interp, alpha_target, LLSmoothInterpolation::getInterpolant(0.1f));

			if (mHoverID != NULL)
			{
				x = (mGraphRect.mRight + mGraphRect.mLeft)/2;
				y = mGraphRect.mBottom + 8;

				LLFontGL::getFontMonospace()->renderUTF8(
					mHoverID->getName(), 
					0, 
					x, y, 
					LLColor4::white,
					LLFontGL::LEFT, LLFontGL::BOTTOM);
			}					
		}
	}

	// Output stats for clicked bar to log
	if (mPrintStats >= 0)
	{
		std::string legend_stat;
		bool first = true;
		for(timer_tree_iterator_t it = begin_timer_tree(LLFastTimer::NamedTimer::getRootNamedTimer());
			it != end_timer_tree();
			++it)
		{
			LLFastTimer::NamedTimer* idp = (*it);

			if (!first)
			{
				legend_stat += ", ";
			}
			first = false;
			legend_stat += idp->getName();

			if (idp->getCollapsed())
			{
				it.skipDescendants();
			}
		}
		LL_INFOS() << legend_stat << LL_ENDL;

		std::string timer_stat;
		first = true;
		for(timer_tree_iterator_t it = begin_timer_tree(LLFastTimer::NamedTimer::getRootNamedTimer());
			it != end_timer_tree();
			++it)
		{
			LLFastTimer::NamedTimer* idp = (*it);

			if (!first)
			{
				timer_stat += ", ";
			}
			first = false;

			U64 ticks;
			if (mPrintStats > 0)
			{
				ticks = idp->getHistoricalCount(mPrintStats);
			}
			else
			{
				ticks = idp->getCountAverage();
			}
			F32 ms = (F32)((F64)ticks * iclock_freq);

			timer_stat += llformat("%.1f",ms);

			if (idp->getCollapsed())
			{
				it.skipDescendants();
			}
		}
		LL_INFOS() << timer_stat << LL_ENDL;
		mPrintStats = -1;
	}
		
	mHoverID = NULL;
	mHoverBarIndex = -1;

	LLView::draw();
}

F64 LLFastTimerView::getTime(const std::string& name)
{
	const LLFastTimer::NamedTimer* timerp = LLFastTimer::getTimerByName(name);
	if (timerp)
	{
		return (F64)timerp->getCountAverage() / (F64)LLFastTimer::countsPerSecond();
	}
	return 0.0;
}

void saveChart(const std::string& label, const char* suffix, LLImageRaw* scratch)
{
	//read result back into raw image
	glReadPixels(0, 0, 1024, 512, GL_RGB, GL_UNSIGNED_BYTE, scratch->getData());

	//write results to disk
	LLPointer<LLImagePNG> result = new LLImagePNG();
	result->encode(scratch, 0.f);

	std::string ext = result->getExtension();
	std::string filename = llformat("%s_%s.%s", label.c_str(), suffix, ext.c_str());
	
	std::string out_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, filename);
	result->save(out_file);
}

//static
void LLFastTimerView::exportCharts(const std::string& base, const std::string& target)
{
	//allocate render target for drawing charts 
	LLRenderTarget buffer;
	buffer.allocate(1024,512, GL_RGB, FALSE, FALSE);
	

	LLSD cur;

	LLSD base_data;

	{ //read base log into memory
		S32 i = 0;
		std::ifstream is(base.c_str());
		while (!is.eof() && LLSDSerialize::fromXML(cur, is) > 0)
		{
			base_data[i++] = cur;
		}
		is.close();
	}

	LLSD cur_data;
	std::set<std::string> chart_names;

	{ //read current log into memory
		S32 i = 0;
		std::ifstream is(target.c_str());
		while (!is.eof() && LLSDSerialize::fromXML(cur, is) > 0)
		{
			cur_data[i++] = cur;

			for (LLSD::map_iterator iter = cur.beginMap(); iter != cur.endMap(); ++iter)
			{
				std::string label = iter->first;
				chart_names.insert(label);
			}
		}
		is.close();
	}

	//get time domain
	LLSD::Real cur_total_time = 0.0;

	for (U32 i = 0; i < (U32)cur_data.size(); ++i)
	{
		cur_total_time += cur_data[i]["Total"]["Time"].asReal();
	}

	LLSD::Real base_total_time = 0.0;
	for (U32 i = 0; i < (U32)base_data.size(); ++i)
	{
		base_total_time += base_data[i]["Total"]["Time"].asReal();
	}

	//allocate raw scratch space
	LLPointer<LLImageRaw> scratch = new LLImageRaw(1024, 512, 3);

	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.loadIdentity();
	gGL.ortho(-0.05f, 1.05f, -0.05f, 1.05f, -1.0f, 1.0f);

	//render charts
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	
	buffer.bindTarget();

	for (std::set<std::string>::iterator iter = chart_names.begin(); iter != chart_names.end(); ++iter)
	{
		std::string label = *iter;
	
		LLSD::Real max_time = 0.0;
		LLSD::Integer max_calls = 0;
		LLSD::Real max_execution = 0.0;

		std::vector<LLSD::Real> cur_execution;
		std::vector<LLSD::Real> cur_times;
		std::vector<LLSD::Integer> cur_calls;

		std::vector<LLSD::Real> base_execution;
		std::vector<LLSD::Real> base_times;
		std::vector<LLSD::Integer> base_calls;

		for (U32 i = 0; i < (U32)cur_data.size(); ++i)
		{
			LLSD::Real time = cur_data[i][label]["Time"].asReal();
			LLSD::Integer calls = cur_data[i][label]["Calls"].asInteger();

			LLSD::Real execution = 0.0;
			if (calls > 0)
			{
				execution = time/calls;
				cur_execution.push_back(execution);
				cur_times.push_back(time);
			}

			cur_calls.push_back(calls);
		}

		for (U32 i = 0; i < (U32)base_data.size(); ++i)
		{
			LLSD::Real time = base_data[i][label]["Time"].asReal();
			LLSD::Integer calls = base_data[i][label]["Calls"].asInteger();

			LLSD::Real execution = 0.0;
			if (calls > 0)
			{
				execution = time/calls;
				base_execution.push_back(execution);
				base_times.push_back(time);
			}

			base_calls.push_back(calls);
		}

		std::sort(base_calls.begin(), base_calls.end());
		std::sort(base_times.begin(), base_times.end());
		std::sort(base_execution.begin(), base_execution.end());

		std::sort(cur_calls.begin(), cur_calls.end());
		std::sort(cur_times.begin(), cur_times.end());
		std::sort(cur_execution.begin(), cur_execution.end());

		//remove outliers
		const U32 OUTLIER_CUTOFF = 512;
		if (base_times.size() > OUTLIER_CUTOFF)
		{ 
			ll_remove_outliers(base_times, 1.f);
		}

		if (base_execution.size() > OUTLIER_CUTOFF)
		{ 
			ll_remove_outliers(base_execution, 1.f);
		}

		if (cur_times.size() > OUTLIER_CUTOFF)
		{ 
			ll_remove_outliers(cur_times, 1.f);
		}

		if (cur_execution.size() > OUTLIER_CUTOFF)
		{ 
			ll_remove_outliers(cur_execution, 1.f);
		}


		max_time = llmax(base_times.empty() ? 0.0 : *base_times.rbegin(), cur_times.empty() ? 0.0 : *cur_times.rbegin());
		max_calls = llmax(base_calls.empty() ? 0 : *base_calls.rbegin(), cur_calls.empty() ? 0 : *cur_calls.rbegin());
		max_execution = llmax(base_execution.empty() ? 0.0 : *base_execution.rbegin(), cur_execution.empty() ? 0.0 : *cur_execution.rbegin());


		LLVector3 last_p;

		//====================================
		// basic
		//====================================
		buffer.clear();

		last_p.clear();

		LLGLDisable<GL_CULL_FACE> cull;

		LLVector3 base_col(0, 0.7f, 0.f);
		LLVector3 cur_col(1.f, 0.f, 0.f);

		gGL.setSceneBlendType(LLRender::BT_ADD);

		gGL.color3fv(base_col.mV);
		for (U32 i = 0; i < base_times.size(); ++i)
		{
			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.vertex3fv(last_p.mV);
			gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
			last_p.set((F32)i/(F32) base_times.size(), base_times[i]/max_time, 0.f);
			gGL.vertex3fv(last_p.mV);
			gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
			gGL.end();
		}
		
		gGL.flush();

		
		last_p.clear();
		{
			LLGLEnable<GL_BLEND> blend;
						
			gGL.color3fv(cur_col.mV);
			for (U32 i = 0; i < cur_times.size(); ++i)
			{
				gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
				gGL.vertex3fv(last_p.mV);
				last_p.set((F32) i / (F32) cur_times.size(), cur_times[i]/max_time, 0.f);
				gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
				gGL.vertex3fv(last_p.mV);
				gGL.end();
			}
			
			gGL.flush();
		}

		saveChart(label, "time", scratch);
		
		//======================================
		// calls
		//======================================
		buffer.clear();

		last_p.clear();

		gGL.color3fv(base_col.mV);
		for (U32 i = 0; i < base_calls.size(); ++i)
		{
			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.vertex3fv(last_p.mV);
			gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
			last_p.set((F32) i / (F32) base_calls.size(), (F32)base_calls[i]/max_calls, 0.f);
			gGL.vertex3fv(last_p.mV);
			gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
			gGL.end();
		}
		
		gGL.flush();

		{
			LLGLEnable<GL_BLEND> blend;
			gGL.color3fv(cur_col.mV);
			last_p.clear();

			for (U32 i = 0; i < cur_calls.size(); ++i)
			{
				gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
				gGL.vertex3fv(last_p.mV);
				last_p.set((F32) i / (F32) cur_calls.size(), (F32) cur_calls[i]/max_calls, 0.f);
				gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
				gGL.vertex3fv(last_p.mV);
				gGL.end();
				
			}
			
			gGL.flush();
		}

		saveChart(label, "calls", scratch);

		//======================================
		// execution
		//======================================
		buffer.clear();


		gGL.color3fv(base_col.mV);
		U32 count = 0;
		U32 total_count = base_execution.size();

		last_p.clear();

		for (std::vector<LLSD::Real>::iterator iter = base_execution.begin(); iter != base_execution.end(); ++iter)
		{
			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.vertex3fv(last_p.mV);
			gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
			last_p.set((F32)count/(F32)total_count, *iter/max_execution, 0.f);
			gGL.vertex3fv(last_p.mV);
			gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
			gGL.end();
			count++;
		}

		last_p.clear();
				
		{
			LLGLEnable<GL_BLEND> blend;
			gGL.color3fv(cur_col.mV);
			count = 0;
			total_count = cur_execution.size();

			for (std::vector<LLSD::Real>::iterator iter = cur_execution.begin(); iter != cur_execution.end(); ++iter)
			{
				gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
				gGL.vertex3fv(last_p.mV);
				last_p.set((F32)count/(F32)total_count, *iter/max_execution, 0.f);			
				gGL.vertex3f(last_p.mV[0], 0.f, 0.f);
				gGL.vertex3fv(last_p.mV);
				gGL.end();
				count++;
			}

			gGL.flush();
		}

		saveChart(label, "execution", scratch);
	}

	buffer.flush();

	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();
}

//static
LLSD LLFastTimerView::analyzePerformanceLogDefault(std::istream& is)
{
	LLSD ret;

	LLSD cur;

	LLSD::Real total_time = 0.0;
	LLSD::Integer total_frames = 0;

	typedef std::map<std::string,LLViewerStats::StatsAccumulator> stats_map_t;
	stats_map_t time_stats;
	stats_map_t sample_stats;

	while (!is.eof() && LLSDSerialize::fromXML(cur, is) > 0)
	{
		for (LLSD::map_iterator iter = cur.beginMap(); iter != cur.endMap(); ++iter)
		{
			std::string label = iter->first;

			F64 time = iter->second["Time"].asReal();

			// Skip the total figure
			if(label.compare("Total") != 0)
			{
				total_time += time;
			}			

			if (time > 0.0)
			{
				LLSD::Integer samples = iter->second["Calls"].asInteger();

				time_stats[label].push(time);
				sample_stats[label].push(samples);
			}
		}
		total_frames++;
	}

	for(stats_map_t::iterator it = time_stats.begin(); it != time_stats.end(); ++it)
	{
		std::string label = it->first;
		ret[label]["TotalTime"] = time_stats[label].mSum;
		ret[label]["MeanTime"] = time_stats[label].getMean();
		ret[label]["MaxTime"] = time_stats[label].getMaxValue();
		ret[label]["MinTime"] = time_stats[label].getMinValue();
		ret[label]["StdDevTime"] = time_stats[label].getStdDev();
		
		ret[label]["Samples"] = sample_stats[label].mSum;
		ret[label]["MaxSamples"] = sample_stats[label].getMaxValue();
		ret[label]["MinSamples"] = sample_stats[label].getMinValue();
		ret[label]["StdDevSamples"] = sample_stats[label].getStdDev();

		ret[label]["Frames"] = (LLSD::Integer)time_stats[label].getCount();
	}
		
	ret["SessionTime"] = total_time;
	ret["FrameCount"] = total_frames;

	return ret;

}

//static
void LLFastTimerView::doAnalysisDefault(std::string baseline, std::string target, std::string output)
{
	// Open baseline and current target, exit if one is inexistent
	std::ifstream base_is(baseline.c_str());
	std::ifstream target_is(target.c_str());
	if (!base_is.is_open() || !target_is.is_open())
	{
		LL_WARNS() << "'-analyzeperformance' error : baseline or current target file inexistent" << LL_ENDL;
		base_is.close();
		target_is.close();
		return;
	}

	//analyze baseline
	LLSD base = analyzePerformanceLogDefault(base_is);
	base_is.close();

	//analyze current
	LLSD current = analyzePerformanceLogDefault(target_is);
	target_is.close();

	//output comparision
	std::ofstream os(output.c_str());

	LLSD::Real session_time = current["SessionTime"].asReal();
	os <<
		"Label, "
		"% Change, "
		"% of Session, "
		"Cur Min, "
		"Cur Max, "
		"Cur Mean/sample, "
		"Cur Mean/frame, "
		"Cur StdDev/frame, "
		"Cur Total, "
		"Cur Frames, "
		"Cur Samples, "
		"Base Min, "
		"Base Max, "
		"Base Mean/sample, "
		"Base Mean/frame, "
		"Base StdDev/frame, "
		"Base Total, "
		"Base Frames, "
		"Base Samples\n"; 

	for (LLSD::map_iterator iter = base.beginMap();  iter != base.endMap(); ++iter)
	{
		LLSD::String label = iter->first;

		if (current[label]["Samples"].asInteger() == 0 ||
			base[label]["Samples"].asInteger() == 0)
		{
			//cannot compare
			continue;
		}	
		LLSD::Real a = base[label]["TotalTime"].asReal() / base[label]["Samples"].asReal();
		LLSD::Real b = current[label]["TotalTime"].asReal() / current[label]["Samples"].asReal();
			
		LLSD::Real diff = b-a;

		LLSD::Real perc = diff/a * 100;

		os << llformat("%s, %.2f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %d, %d, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %d, %d\n",
			label.c_str(), 
			(F32) perc, 
			(F32) (current[label]["TotalTime"].asReal()/session_time * 100.0), 

			(F32) current[label]["MinTime"].asReal(), 
			(F32) current[label]["MaxTime"].asReal(), 
			(F32) b, 
			(F32) current[label]["MeanTime"].asReal(), 
			(F32) current[label]["StdDevTime"].asReal(),
			(F32) current[label]["TotalTime"].asReal(), 
			current[label]["Frames"].asInteger(),
			current[label]["Samples"].asInteger(),
			(F32) base[label]["MinTime"].asReal(), 
			(F32) base[label]["MaxTime"].asReal(), 
			(F32) a, 
			(F32) base[label]["MeanTime"].asReal(), 
			(F32) base[label]["StdDevTime"].asReal(),
			(F32) base[label]["TotalTime"].asReal(), 
			base[label]["Frames"].asInteger(),
			base[label]["Samples"].asInteger());			
	}

	exportCharts(baseline, target);

	os.flush();
	os.close();
}

//static
void LLFastTimerView::outputAllMetrics()
{
	/*if (LLMetricPerformanceTesterBasic::hasMetricPerformanceTesters())
	{
		for (LLMetricPerformanceTesterBasic::name_tester_map_t::iterator iter = LLMetricPerformanceTesterBasic::sTesterMap.begin(); 
			iter != LLMetricPerformanceTesterBasic::sTesterMap.end(); ++iter)
		{
			LLMetricPerformanceTesterBasic* tester = ((LLMetricPerformanceTesterBasic*)iter->second);	
			tester->outputTestResults();
		}
	}*/
}

//static
void LLFastTimerView::doAnalysis(std::string baseline, std::string target, std::string output)
{
	if(LLFastTimer::sLog)
	{
		doAnalysisDefault(baseline, target, output) ;
		return ;
	}

	/*if(LLFastTimer::sMetricLog)
	{
		LLMetricPerformanceTesterBasic::doAnalysisMetrics(baseline, target, output) ;
		return ;
	}*/
}
void	LLFastTimerView::onClose(bool app_quitting)
{
	if(app_quitting)
	{
		LLFloater::close(app_quitting);
	}
	else
	{
		setVisible(false);
	}
}

