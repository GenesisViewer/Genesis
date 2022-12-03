#include "llviewerprecompiledheaders.h"
#include "genxtexturecache.h"
#include "llsqlmgr.h"
#include "lltimer.h"
#include "llimage.h"
GenxTextureCache::GenxTextureCache()
{
	
}
void GenxTextureCache::writeTextureCache (LLUUID textureId, S32 size, U8* buffer) {
    int rc;
    char *sql;
    sqlite3_stmt *stmt;
    char *zErrMsg = 0;
    sqlite3 *db = LLSqlMgr::instance().getTextureCacheDB();
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    sql = "INSERT INTO TEXTURE_CACHE_ENTRY(ID,SIZE,LAST_USED) VALUES(?,?,?)";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1,  textureId.asString().c_str(), strlen(textureId.asString().c_str()), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2,  size);
    sqlite3_bind_int(stmt, 3,  LLTimer::getTotalSeconds());
   
   
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    S32 left = size;
    int part = 0;
    
    sql = "INSERT INTO TEXTURE_CACHE_FILES (ID,PART,DATAS) VALUES (?,?,?)";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    while (left >0) {
        int temp_size = llmin (10000, left);
        U8* aData = (U8*) ALLOCATE_MEM(LLImageBase::getPrivatePool(), temp_size);
        memcpy(aData,buffer + (part * 10000), temp_size );
        
        sqlite3_bind_text(stmt,1,textureId.asString().c_str(), strlen(textureId.asString().c_str()),SQLITE_STATIC) ;
        sqlite3_bind_int(stmt, 2,  part);
        sqlite3_bind_blob(stmt,3 ,(const U8*)aData,temp_size,SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        sqlite3_reset(stmt);
        left = left-temp_size;
        part++;
        FREE_MEM(LLImageBase::getPrivatePool(), aData);
        aData = NULL;
    }
    sqlite3_finalize(stmt);  
    sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
}
void GenxTextureCache::readTextureCache(LLUUID textureId, LLPointer<LLImageFormatted> formattedImage,std::string url) {
    
    if (url.compare(0, 7, "file://") == 0)
	{
        std::string filename = url.substr(7, std::string::npos);
        //get the file size
        S32 size = LLAPRFile::size(filename);
        U8* readData = (U8*)ALLOCATE_MEM(LLImageBase::getPrivatePool(), size);
		S32 bytes_read = LLAPRFile::readEx(filename, readData, 0, size);
		if (bytes_read != size)
		{
 			LL_WARNS() << "Error reading file from local cache: " << filename
 					<< " Bytes: " << size << " Offset: " << 0
 					<< " / " << size << LL_ENDL;
			
			FREE_MEM(LLImageBase::getPrivatePool(), readData);
        } else {
            //formattedImage->allocateData(size);
            formattedImage->appendData(readData,size);
        }    
			
    } else {
        char *sql;
        sqlite3_stmt *stmt;
        sqlite3 *db = LLSqlMgr::instance().getTextureCacheDB();
        S32 size = 0;
        sql = "SELECT SIZE FROM TEXTURE_CACHE_ENTRY WHERE ID =?";
        sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        sqlite3_bind_text(stmt, 1,  textureId.asString().c_str(), strlen(textureId.asString().c_str()), SQLITE_STATIC);
        if ( sqlite3_step(stmt) == SQLITE_ROW) {
            size = sqlite3_column_int(stmt, 0);

        }
        sqlite3_finalize(stmt);
        if (size >0) {
            sql = "SELECT DATAS FROM TEXTURE_CACHE_FILES WHERE ID =? ORDER BY PART";
            sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            sqlite3_bind_text(stmt, 1,  textureId.asString().c_str(), strlen(textureId.asString().c_str()), SQLITE_STATIC);
            while ( sqlite3_step(stmt) == SQLITE_ROW) {
                int nData = sqlite3_column_bytes(stmt, 0);
                U8 *aData = (U8*) ALLOCATE_MEM(LLImageBase::getPrivatePool(), nData);
                const U8 *pBuffer = reinterpret_cast<const U8*>( sqlite3_column_blob(stmt, 0) );
                memcpy(aData,pBuffer,nData);
                //formattedImage->allocateData(size);
                formattedImage->appendData(aData,nData);
                
                
            }
            sqlite3_finalize(stmt);
            //update last used for LRU system
            sql = "UPDATE TEXTURE_CACHE_ENTRY SET LAST_USED = ?  WHERE ID =?";
            sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1,  LLTimer::getTotalSeconds());
            sqlite3_bind_text(stmt, 2,  textureId.asString().c_str(), strlen(textureId.asString().c_str()), SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        

    }
}