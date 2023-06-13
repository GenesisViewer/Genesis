#pragma once

#include "aostate.h"
#include "llfloater.h"
#include <stdio.h>
#include "aosystem.h"
class GenxFloaterAO final : public LLFloater, public LLFloaterSingleton<GenxFloaterAO>
{
	friend class AOSystem;
public:

	GenxFloaterAO(const LLSD&);
	BOOL postBuild() override;
	void onOpen() override;
	virtual ~GenxFloaterAO();
	void GenxFloaterAO::onClickNewAOSet();
    void refreshAOSets();
    void onSelectAOSet();
    void onDeleteAOSet();
    void onRenameAOSet();
    void onRenameAOSetOK();
    void onRenameAOSetCancel();
    void onClickNewCard() const;
    void onInventoryItemDropped();
    void onClickReloadCard() const;
    void onClickOpenCard() const;
    void onComboBoxCommit(LLUICtrl* ctrl) const;
    void onClickCycleStand(bool next) const;
    void onClickStandRandomize();
    void onClickNoStandInMouselook();
    void onChangeStandTime();
private:
    std::array<class LLComboBox*, STATE_AGENT_END> mCombos;  
    std::map<std::string, std::string> mapCombosToControlNames;
protected:

	LLComboBox* getComboFromState(const U8& state) const { return mCombos[state]; }      
};
