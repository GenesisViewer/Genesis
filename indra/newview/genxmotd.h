#include "llsingleton.h"    
#ifndef GENX_MOTD_H
#define GENX_MOTD_H



class GenxMOTD: public LLSingleton<GenxMOTD>
{
 public:
	GenxMOTD();
	std::string getMOTD();
private:
    BOOL mShowGenxMOTD = FALSE;    
};

#endif // GENX_MOTD_H