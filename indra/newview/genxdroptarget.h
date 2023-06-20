
#ifndef GENXDROPTARGET_H
#define GENXDROPTARGET_H

#include "stdtypes.h"
#include "llview.h"
#include "lluictrl.h"
#include "llinventorymodel.h"
class GenxDropTarget : public LLView
{
public:
	struct Params : public LLInitParam::Block<Params, LLView::Params>
	{
		Optional<bool> border_visible; // Whether or not to display the border
		Optional<std::string> control_name; // Control to change on item drop (Per Account only)
		Optional<std::string> label; // Label for the LLTextBox, used when label doesn't dynamically change on drop
		Optional<bool> show_reset; // Whether or not to show the reset button
		Optional<bool> fill_parent; // Whether or not to fill the direct parent, to have a larger drop target.  If true, the next sibling must explicitly define its rect without deltas.
		Params()
		:	border_visible("border_visible", true)
		,	control_name("control_name", "")
		,	label("label", "")
		,	show_reset("show_reset", true)
		,	fill_parent("fill_parent", false)
		{
			changeDefault(mouse_opaque, false);
			changeDefault(follows.flags, FOLLOWS_ALL);
		}
	};

	GenxDropTarget(const Params& p = Params());
	~GenxDropTarget();

	virtual void doDrop(EDragAndDropType cargo_type, void* cargo_data);

	//
	// LLView functionality
	virtual BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop, EDragAndDropType cargo_type, void* cargo_data, EAcceptance* accept, std::string& tooltip_msg);
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent, class LLUICtrlFactory* factory);
	virtual void initFromXML(LLXMLNodePtr node, LLView* parent);
	

	void setChildRects(LLRect rect);
	void fillParent(const LLView* parent);
	virtual void setValue(const LLSD& value);
	boost::signals2::connection setCommitCallback( const LLUICtrl::commit_signal_t::slot_type& cb ) ;
	LLInventoryItem* getItem() {return mItem;}
	
protected:
	virtual void setItem(LLInventoryItem* item);
	

	
	LLInventoryItem* mItem;
	class LLViewBorder* mBorder;
	class LLTextBox* mText;
	class LLButton* mReset;
	class LLUICtrl* mCtrl;
	//LLUICtrl::commit_signal_t*		mCommitSignal;
private: 
	std::string mLabel;	
};

#endif // GENXDROPTARGET_H
