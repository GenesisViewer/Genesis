#include "llfavoritestoolbar.h"
#include "llrect.h"
#include "lluictrlfactory.h"
#include "lltooldraganddrop.h"
#include "llfavoritesbar.h"
LLFavoritesBar *gFavoritesBar = NULL;
S32 FAVORITES_BAR_HEIGHT = 0;
extern S32 MENU_BAR_HEIGHT;


LLFavoritesBar::LLFavoritesBar(const std::string& name, const LLRect& rect)
:	LLPanel(name, LLRect(), FALSE)
{
    
	LLUICtrlFactory::getInstance()->buildPanel(this,"panel_favorites_bar.xml");
    
    //LLFavoritesBarCtrl favoriteBarCtrl  = new LLFavoritesBarCtrl(LLFavoritesBarCtrl::Params());

}



