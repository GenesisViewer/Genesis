
#include "llviewerprecompiledheaders.h"

#include "genxdroptarget.h"


#include "llbutton.h"
#include "llinventorymodel.h"
#include "lltextbox.h"
#include "lltooldraganddrop.h"
#include "lltrans.h"
#include "llviewborder.h"

static LLRegisterWidget<GenxDropTarget> r("genx_drop_target");



GenxDropTarget::GenxDropTarget(const GenxDropTarget::Params& p)
{
	setToolTip(std::string(p.tool_tip));
	mCtrl = new LLUICtrl(LLUICtrl::getDefaultParams());
	mText = new LLTextBox("drop_text", LLRect(), p.label);
	addChild(mText);

	//this->setText("drop_text");

	setControlName(p.control_name, NULL);
	mText->setFollows(FOLLOWS_NONE);
	mText->setMouseOpaque(false);
	mText->setVPad(1);
	mText->setHAlign(LLFontGL::HCENTER);

	mBorder = new LLViewBorder("drop_border", LLRect(), LLViewBorder::BEVEL_IN);
	addChild(mBorder);

	mBorder->setMouseOpaque(false);
	if (!p.border_visible) mBorder->setBorderWidth(0);

	if (p.show_reset)
	{
		addChild(mReset = new LLButton("reset", LLRect(), "icn_clear_lineeditor.tga", "icn_clear_lineeditor.tga", "", boost::bind(&GenxDropTarget::setValue, this, _2)));
	}

	// Now set the rects of the children
	p.fill_parent ? fillParent(getParent()) : setChildRects(p.rect);
}

GenxDropTarget::~GenxDropTarget()
{
}

// static
LLView* GenxDropTarget::fromXML(LLXMLNodePtr node, LLView* parent, LLUICtrlFactory* factory)
{
	GenxDropTarget* target = new GenxDropTarget;
	target->initFromXML(node, parent);
	return target;
}

// virtual
void GenxDropTarget::initFromXML(LLXMLNodePtr node, LLView* parent)
{
	LLView::initFromXML(node, parent);

	const LLRect& rect = getRect();
	if (node->hasAttribute("show_reset"))
	{
		bool show;
		node->getAttribute_bool("show_reset", show);
		if (!show)
		{
			delete mReset;
			mReset = NULL;
		}
	}
	setChildRects(LLRect(0, rect.getHeight(), rect.getWidth(), 0));

	if (node->hasAttribute("name")) // Views can't have names, but drop targets can
	{
		std::string name;
		node->getAttributeString("name", name);
		setName(name);
	}

	if (node->hasAttribute("label"))
	{
		std::string label;
		node->getAttributeString("label", label);
		mText->setText(label);
		mLabel=label;
	}

	if (node->hasAttribute("fill_parent"))
	{
		bool fill;
		node->getAttribute_bool("fill_parent", fill);
		if (fill) fillParent(parent);
	}

	if (node->hasAttribute("border_visible"))
	{
		bool border_visible;
		node->getAttribute_bool("border_visible", border_visible);
		if (!border_visible) mBorder->setBorderWidth(0);
	}
}



void GenxDropTarget::setChildRects(LLRect rect)
{
	mBorder->setRect(rect);
	if (mReset)
	{
		// Reset button takes rightmost part of the text area.
		S32 height(rect.getHeight());
		rect.mRight -= height;
		mText->setRect(rect);
		rect.mLeft = rect.mRight;
		rect.mRight += height;
		mReset->setRect(rect);
	}
	else
	{
		mText->setRect(rect);
	}
}

void GenxDropTarget::fillParent(const LLView* parent)
{
	if (!parent) return; // No parent to fill

	const std::string& tool_tip = getToolTip();
	if (!tool_tip.empty()) // Don't tool_tip the entire parent
	{
		mText->setToolTip(tool_tip);
		setToolTip(LLStringExplicit(""));
	}

	// The following block enlarges the target, but maintains the desired size for the text and border
	setChildRects(getRect()); // Children maintain the old rectangle
	const LLRect& parent_rect = parent->getRect();
	setRect(LLRect(0, parent_rect.getHeight(), parent_rect.getWidth(), 0));
}
void GenxDropTarget::setValue(const LLSD& value)
{
	const LLUUID& id(value.asUUID());
	setItem(id.isNull() ? NULL : gInventory.getItem(id));
	
}

void GenxDropTarget::setItem(LLInventoryItem* item)
{
	if (mReset) mReset->setVisible(!!item);
	if (item) 
		mText->setText(item->getName());
	else	
		mText->setText(mLabel);
	this->mItem=item;	
}


void GenxDropTarget::doDrop(EDragAndDropType cargo_type, void* cargo_data)
{
	LL_INFOS() << "LLDropTarget::doDrop()" << LL_ENDL;
}

BOOL GenxDropTarget::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop, EDragAndDropType cargo_type, void* cargo_data, EAcceptance* accept, std::string& tooltip_msg)
{
	LLViewerInventoryItem* inv_item = static_cast<LLViewerInventoryItem*>(cargo_data);
	if (cargo_type >= DAD_TEXTURE && cargo_type <= DAD_LINK &&
		inv_item && inv_item->getActualType() != LLAssetType::AT_LINK_FOLDER && inv_item->getType() != LLAssetType::AT_CATEGORY &&
		(
			LLAssetType::lookupCanLink(inv_item->getType()) ||
			(inv_item->getType() == LLAssetType::AT_LINK && !gInventory.getObject(inv_item->getLinkedUUID()))
		))
	{
	
		*accept = ACCEPT_YES_COPY_SINGLE;
		if (drop) {
			setItem(inv_item);
			mCtrl->onCommit();
		}
	} else {
		*accept = ACCEPT_NO;
	}
	return true;
}

boost::signals2::connection GenxDropTarget::setCommitCallback( const LLUICtrl::commit_signal_t::slot_type& cb ) 
{ 
	// if (!mCommitSignal) mCommitSignal = new LLUICtrl::commit_signal_t();
	// return mCommitSignal->connect(cb); 
	return mCtrl->setCommitCallback(cb);
}