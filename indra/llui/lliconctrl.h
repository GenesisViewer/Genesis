/** 
 * @file lliconctrl.h
 * @brief LLIconCtrl base class
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

#ifndef LL_LLICONCTRL_H
#define LL_LLICONCTRL_H

#include "lluuid.h"
#include "v4color.h"
#include "lluictrl.h"
#include "stdenums.h"
#include "llimagegl.h"

class LLTextBox;
class LLUICtrlFactory;

//
// Classes
//
class LLIconCtrl
: public LLUICtrl
{
public:
	struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
	{
		Optional<LLUIImage*>	image;
		Optional<LLUIColor>		color;
//		Optional<bool>			use_draw_context_alpha;
		Optional<S32>			min_width,
								min_height;
		Ignored					scale_image;

		Params();
	};
protected:
	LLIconCtrl(const Params&);
	friend class LLUICtrlFactory;

public:
	LLIconCtrl(const std::string& name, const LLRect &rect, const std::string &image_name, const S32& min_width = 0, const S32& min_height = 0);
	virtual ~LLIconCtrl();

	// llview overrides
	virtual void	draw();

	// lluictrl overrides
	virtual void	setValue(const LLSD& value );

	/*virtual*/ void	setAlpha(F32 alpha);

	std::string	getImageName() const;

	void			setColor(const LLColor4& color) { mColor = color; }
	void			setImage(LLPointer<LLUIImage> image) { mImagep = image; }
	const LLPointer<LLUIImage> getImage() { return mImagep; }

	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	static LLView* fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

protected:
	S32 mPriority;

	//the output size of the icon image if set.
	S32 mMinWidth,
		mMinHeight;

	// If set to true (default), use the draw context transparency.
	// If false, will use transparency returned by getCurrentTransparency(). See STORM-698.
	//bool mUseDrawContextAlpha;

private:
	LLColor4		mColor;
	LLPointer<LLUIImage>	mImagep;
};

#endif
