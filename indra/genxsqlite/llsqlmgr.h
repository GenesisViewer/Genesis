
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
	char init(std::string db_path);
    sqlite3 *getDB();
    void close();
    bool isInit() {return ready;}
private:
    sqlite3 *db;
    bool ready = FALSE;
};

#endif /* LLSQLMGR_H */
