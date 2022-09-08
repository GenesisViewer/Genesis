
#include <stdio.h>
#include "llsingleton.h"
#include "sqlite3.h"

#ifndef LLSQLMGR_H
#define LLSQLMGR_H

class LLSqlMgr : public LLSingleton<LLSqlMgr>
{
public:
	LLSqlMgr();
	~LLSqlMgr();

    // viewer auth version
	char initAgentDB(std::string db_path);
    char initALLAgentsDB(std::string db_path);
    sqlite3 *getDB();
    sqlite3 *getAllAgentsDB();
    void close();
    bool isInit() {return ready;}
private:
    //db for a connected agent
    sqlite3 *db;
    //db for all agents
    sqlite3 *commondb;
    
    bool ready = FALSE;
};

#endif /* LLSQLMGR_H */
