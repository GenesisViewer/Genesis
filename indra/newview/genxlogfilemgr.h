#include "llsingleton.h"    
#include "sqlite3.h"
#ifndef GENX_LOGFILEMGR_H
#define GENX_LOGFILEMGR_H



class GenxLogFileMgr: public LLSingleton<GenxLogFileMgr>
{
 public:
	GenxLogFileMgr();
    void initLogs();
    std::string getLastLogFile();
    std::string addLogFile();
    void cleanLogFiles();
private:
    sqlite3 *db ;
    
};

#endif // GENX_LOGFILEMGR_H