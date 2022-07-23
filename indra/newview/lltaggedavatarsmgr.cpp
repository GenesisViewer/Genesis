#include "llviewerprecompiledheaders.h"
#include "lltaggedavatarsmgr.h"
#include <stdio.h>
#include "llsqlmgr.h"
#include "v4color.h"

LLTaggedAvatarsMgr::LLTaggedAvatarsMgr() 
{
	
}
LLTaggedAvatarsMgr::~LLTaggedAvatarsMgr()
{
	
}


char LLTaggedAvatarsMgr::init() {
  return '0';
}


void LLTaggedAvatarsMgr::updateContactSet(std::string avatarId, std::string contactSet, std::string avatarName) {
   char *errMsg = 0;     
   int rc;   
   char *sql;
   sqlite3_stmt *stmt;
   LL_INFOS() << "updating Tagged avatars from Genesis DB" << LL_ENDL;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "INSERT INTO CONTACT_SET_AVATARS(CONTACT_SET_ID,AVATAR_ID,AVATAR_NAME)"  \
         "VALUES(?,?,?)"  \
         "ON CONFLICT(AVATAR_ID) DO UPDATE SET"  \
         "   CONTACT_SET_ID=excluded.CONTACT_SET_ID,"  \
         "   AVATAR_NAME=excluded.AVATAR_NAME";
        



   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   sqlite3_bind_text(stmt, 1,  contactSet.c_str(), strlen(contactSet.c_str()), 0);
   sqlite3_bind_text(stmt, 2,  avatarId.c_str(), strlen(avatarId.c_str()), 0);
   sqlite3_bind_text(stmt, 3,  avatarName.c_str(), strlen(avatarName.c_str()), 0);
   
   sqlite3_step(stmt);
   sqlite3_finalize(stmt);

}
void LLTaggedAvatarsMgr::deleteContactSet(std::string csId) {
    char *errMsg = 0;     
   int rc;   
   char *sql;
   sqlite3_stmt *stmt;
   LL_INFOS() << "Deleting Contact Set from Genesis DB" << LL_ENDL;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "DELETE FROM CONTACT_SET_AVATARS WHERE CONTACT_SET_ID=?";
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
   sqlite3_bind_text(stmt, 1,  csId.c_str(), strlen(csId.c_str()), 0);
   sqlite3_step(stmt);
   sqlite3_finalize(stmt);

   sql = "DELETE FROM CONTACTS_SET WHERE ID=?";
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
   sqlite3_bind_text(stmt, 1,  csId.c_str(), strlen(csId.c_str()), 0);
   sqlite3_step(stmt);
   sqlite3_finalize(stmt);

}
void LLTaggedAvatarsMgr::deleteAvatarContactSet(std::string avatarId) {
   char *errMsg = 0;     
   int rc;   
   char *sql;
   sqlite3_stmt *stmt;
   LL_INFOS() << "Deleting Tagged avatars from Genesis DB" << LL_ENDL;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "DELETE FROM CONTACT_SET_AVATARS WHERE AVATAR_ID=?";
     
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
   sqlite3_bind_text(stmt, 1,  avatarId.c_str(), strlen(avatarId.c_str()), 0);
   
   
   sqlite3_step(stmt);
   sqlite3_finalize(stmt);
   
}
std::string LLTaggedAvatarsMgr::getAvatarContactSetId(std::string avatarId) {
   char *errMsg = 0;     
   int rc;   
   char *sql;
   sqlite3_stmt *stmt;
   std::string contactSetId;
   LL_INFOS() << "Getting contact_set from Genesis DB" << LL_ENDL;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "SELECT csa.CONTACT_SET_ID FROM CONTACT_SET_AVATARS csa WHERE AVATAR_ID=?";
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
   sqlite3_bind_text(stmt, 1,  avatarId.c_str(), strlen(avatarId.c_str()), 0);
   while ( sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *id = sqlite3_column_text(stmt, 0);
      contactSetId = std::string(reinterpret_cast<const char*>(id));
   }
   sqlite3_finalize(stmt);
   return contactSetId;
}
std::string LLTaggedAvatarsMgr::getAvatarContactSetName(std::string avatarId) {
   char *errMsg = 0;     
   int rc;   
   char *sql;
   sqlite3_stmt *stmt;
   std::string contactSetId;
   LL_INFOS() << "Getting contact_set from Genesis DB" << LL_ENDL;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "SELECT IFNULL(cs.ALIAS,cs.ID) FROM CONTACT_SET_AVATARS csa , CONTACTS_SET cs WHERE cs.ID=csa.CONTACT_SET_ID AND AVATAR_ID=?";
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
   sqlite3_bind_text(stmt, 1,  avatarId.c_str(), strlen(avatarId.c_str()), 0);
   while ( sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *id = sqlite3_column_text(stmt, 0);
      contactSetId = std::string(reinterpret_cast<const char*>(id));
   }
   sqlite3_finalize(stmt);
   return contactSetId;
}
void LLTaggedAvatarsMgr::updateColorContactSet(std::string csId, LLColor4 color) {
   char *errMsg = 0;     
   int rc;   
   char *sql;
   sqlite3_stmt *stmt;
   const auto rgb_color = color.getValue();
   F32 r = rgb_color[0];
   F32 g = rgb_color[1];
   F32 b = rgb_color[2];
   LL_INFOS() << "setting Color contact set  from Genesis DB " << csId << LL_ENDL;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "UPDATE CONTACTS_SET SET R=?,G=?,B=?,A=1.0 WHERE ID = ?";
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   sqlite3_bind_double(stmt, 1,  r);
   sqlite3_bind_double(stmt, 2,  g);
   sqlite3_bind_double(stmt, 3,  b);
   sqlite3_bind_text(stmt, 4,  csId.c_str(), strlen(csId.c_str()), 0);
   sqlite3_step(stmt);
   
   sqlite3_finalize(stmt);
   
}
LLColor4 LLTaggedAvatarsMgr::getColorContactSet(std::string csId) {
   char *errMsg = 0;     
   int rc;   
   char *sql;
   sqlite3_stmt *stmt;
   LLColor4 colorContactSet;
   LL_INFOS() << "getting Color contact set  from Genesis DB " << csId << LL_ENDL;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "SELECT R,G,B,A FROM CONTACTS_SET where ID=?";
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
   sqlite3_bind_text(stmt, 1,  csId.c_str(), strlen(csId.c_str()), 0);
   while ( sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *id = sqlite3_column_text(stmt, 0);
    
      double r = sqlite3_column_double(stmt, 0);
      double g = sqlite3_column_double(stmt, 1);
      double b = sqlite3_column_double(stmt, 2);
      double a = sqlite3_column_double(stmt, 3);
      colorContactSet = LLColor4(F32(r),F32(g),F32(b),F32(a));
      
   }
   sqlite3_finalize(stmt);
   return colorContactSet;
}
std::string LLTaggedAvatarsMgr::getContactSetName(std::string csId) {
   char *errMsg = 0;     
   int rc;   
   char *sql;
   std::string result;
   sqlite3_stmt *stmt;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "SELECT IFNULL(ALIAS,ID) from CONTACTS_SET where ID=?";
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   sqlite3_bind_text(stmt, 1,  csId.c_str(), strlen(csId.c_str()), 0);
   while ( sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *name = sqlite3_column_text(stmt, 0);
      result = std::string(reinterpret_cast<const char*>(name));
   }
   return result;
}
LLColor4 LLTaggedAvatarsMgr::getAvatarColorContactSet(std::string avatarId) {
   char *errMsg = 0;     
   int rc;   
   char *sql;
   sqlite3_stmt *stmt;
   LLColor4 colorContactSet;
   LL_INFOS() << "Deleting Tagged avatars from Genesis DB" << LL_ENDL;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "SELECT csa.AVATAR_ID,R,G,B,A FROM CONTACT_SET_AVATARS csa,CONTACTS_SET cs where csa.CONTACT_SET_ID=cs.ID and csa.AVATAR_ID=?";
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
   sqlite3_bind_text(stmt, 1,  avatarId.c_str(), strlen(avatarId.c_str()), 0);
   while ( sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *id = sqlite3_column_text(stmt, 0);
    
      double r = sqlite3_column_double(stmt, 1);
      double g = sqlite3_column_double(stmt, 2);
      double b = sqlite3_column_double(stmt, 3);
      double a = sqlite3_column_double(stmt, 4);
      colorContactSet = LLColor4(F32(r),F32(g),F32(b),F32(a));

   }
   sqlite3_finalize(stmt);
   return colorContactSet;
}
std::map<std::string, std::string> LLTaggedAvatarsMgr::getContactSets() {
   std::map<std::string, std::string> contact_sets;
   char *errMsg = 0;     
   int rc;   
   sqlite3_stmt *result;
   LL_INFOS() << "Loading Contact set from Genesis DB" << LL_ENDL;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   rc = sqlite3_prepare_v2(db, "SELECT ID, IFNULL(ALIAS,ID) from CONTACTS_SET order by 2", -1, &result, NULL);
  
   if (rc != SQLITE_OK) {
    
      LL_WARNS() << "Cannot read CONTACTS_SET table" << sqlite3_errmsg(db) << LL_ENDL;
     
   }

   while ( sqlite3_step(result) == SQLITE_ROW) {
      const unsigned char *id = sqlite3_column_text(result, 0);
      const unsigned char *name = sqlite3_column_text(result, 1);
      
      contact_sets[std::string(reinterpret_cast<const char*>(id))] = std::string(reinterpret_cast<const char*>(name));
      
      
   }
   sqlite3_finalize(result);
   return contact_sets;
}
void LLTaggedAvatarsMgr::updateContactSetName(std::string csId, std::string csAlias) {
   char *errMsg = 0;     
   int rc;   
   char *sql;
   sqlite3_stmt *stmt;
   std::string result;
   LL_INFOS() << "updating  contact set name" << LL_ENDL;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "UPDATE CONTACTS_SET SET ALIAS = ? WHERE ID= ?";

   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   sqlite3_bind_text(stmt, 1,  csAlias.c_str(), strlen(csAlias.c_str()), 0);
   sqlite3_bind_text(stmt, 2,  csId.c_str(), strlen(csId.c_str()), 0);
   
   
   
   rc=sqlite3_step(stmt);
  
   if (rc == SQLITE_DONE) {
      //enoyer un signal
   }
   sqlite3_finalize(stmt);
   
}
std::string LLTaggedAvatarsMgr::insertContactSet(std::string csId, std::string csAlias) {
   char *errMsg = 0;     
   int rc;   
   char *sql;
   sqlite3_stmt *stmt;
   std::string result;
   LL_INFOS() << "inserting new contact set" << LL_ENDL;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "INSERT INTO CONTACTS_SET(ID,ALIAS,R,G,B,A)"  \
         "VALUES(?,?,?,?,?,?)";
        

   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   sqlite3_bind_text(stmt, 1,  csId.c_str(), strlen(csId.c_str()), 0);
   sqlite3_bind_text(stmt, 2,  csAlias.c_str(), strlen(csAlias.c_str()), 0);
   sqlite3_bind_double(stmt, 3,0.0 );
   sqlite3_bind_double(stmt, 4,0.0 );
   sqlite3_bind_double(stmt, 5,0.0 );
   sqlite3_bind_double(stmt, 6,1.0 );
   
   rc=sqlite3_step(stmt);
  
   if (rc == SQLITE_DONE) {
      //enoyer un signal
   }
   sqlite3_finalize(stmt);
   return result;
}