#include "llviewerprecompiledheaders.h"
#include "genxlogfilemgr.h"

GenxLogFileMgr::GenxLogFileMgr()
{
}

void GenxLogFileMgr::initLogs()
{
   

    char *sql;
    char *zErrMsg = 0;
    std::string logdbpath = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "genesislog.db");
    sqlite3_open(logdbpath.c_str(), &db);
    

    sql = "CREATE TABLE IF NOT EXISTS LOG_FILES (" \
    "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP," \
    "logfile TEXT" \
    ")";

    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg); 



}

std::string GenxLogFileMgr::getLastLogFile() {
    char *sql;
    
    sqlite3_stmt *stmt;
    

    std::string result = "";

    sql = "SELECT logfile FROM LOG_FILES ORDER BY timestamp desc limit 1";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
   
    while ( sqlite3_step(stmt) == SQLITE_ROW) {
       const unsigned char *logfile = sqlite3_column_text(stmt, 0);
       
       result = std::string(reinterpret_cast<const char*>(logfile));
    }
    return result;
}

std::string GenxLogFileMgr::addLogFile() {
    char *zErrMsg = 0;
    char *sql = "INSERT INTO LOG_FILES (logfile) VALUES ('Genesis-' || strftime('%Y-%m-%d-%H-%M-%S','now','localtime') || '.log')";
    
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg); 

    return getLastLogFile();
}

void GenxLogFileMgr::cleanLogFiles() {
    char *sql;
    char *zErrMsg = 0;
    sqlite3_stmt *stmt;
    

    std::string result = "";

    sql = "SELECT logfile FROM LOG_FILES ORDER BY timestamp desc";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
    S32 index=1;
    while ( sqlite3_step(stmt) == SQLITE_ROW) {
       const unsigned char *logfile = sqlite3_column_text(stmt, 0);
       
       std::string filename = std::string(reinterpret_cast<const char*>(logfile));
       if ( index > 5)
            LLAPRFile::remove(gDirUtilp->getExpandedFilename(LL_PATH_LOGS, filename));
       index++;
    }
    
    sql = "DELETE FROM LOG_FILES where timestamp not in (select timestamp from LOG_FILES order by timestamp desc limit 5)";
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg); 
}