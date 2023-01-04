#include "llviewerprecompiledheaders.h"
#include "genxcontactset.h"
#include <stdio.h>
#include "llsqlmgr.h"
#include "v4color.h"



GenxContactSetMgr::GenxContactSetMgr() 
{
}
GenxContactSetMgr::~GenxContactSetMgr()
{
}
void GenxContactSetMgr::init() {
  
}


ContactSet GenxContactSetMgr::getContactSet(std::string csId) {
   
   char *sql;
   sqlite3_stmt *stmt;
   ContactSet contactSet;
   
   
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "SELECT R,G,B,A,IFNULL(ALIAS,ID)  FROM CONTACTS_SET where ID=?";
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
   sqlite3_bind_text(stmt, 1,  csId.c_str(), strlen(csId.c_str()), 0);
   while ( sqlite3_step(stmt) == SQLITE_ROW) {
      
    
      double r = sqlite3_column_double(stmt, 0);
      double g = sqlite3_column_double(stmt, 1);
      double b = sqlite3_column_double(stmt, 2);
      double a = sqlite3_column_double(stmt, 3);
      LLColor4  colorContactSet = LLColor4(F32(r),F32(g),F32(b),F32(a));
      const unsigned char *name = sqlite3_column_text(stmt, 4);
      std::string contactSetName = std::string(reinterpret_cast<const char*>(name));
      contactSet = ContactSet();
      contactSet.setId(csId);
      contactSet.setName(contactSetName);
      contactSet.setColor(colorContactSet);
   }
   sqlite3_finalize(stmt);
   return contactSet;
}

std::vector<ContactSet> GenxContactSetMgr::getContactSets(){
   char *sql;
   sqlite3_stmt *stmt;
   std::vector<ContactSet> contactsets;
   
   
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "SELECT R,G,B,A,IFNULL(ALIAS,ID),ID  FROM CONTACTS_SET order by 5 COLLATE NOCASE ASC";
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
   
   while ( sqlite3_step(stmt) == SQLITE_ROW) {
      
    
      double r = sqlite3_column_double(stmt, 0);
      double g = sqlite3_column_double(stmt, 1);
      double b = sqlite3_column_double(stmt, 2);
      double a = sqlite3_column_double(stmt, 3);
      LLColor4  colorContactSet = LLColor4(F32(r),F32(g),F32(b),F32(a));
      const unsigned char *name = sqlite3_column_text(stmt, 4);
      const unsigned char *id = sqlite3_column_text(stmt, 5);
      std::string contactSetName = std::string(reinterpret_cast<const char*>(name));
      ContactSet contactSet = ContactSet();
      std::string csId = reinterpret_cast<const char*>(id);
      contactSet.setId(csId);
      contactSet.setName(contactSetName);
      contactSet.setColor(colorContactSet);
      contactsets.push_back(contactSet);
   }
   sqlite3_finalize(stmt);
   return contactsets;
}
ContactSet GenxContactSetMgr::getAvatarContactSet(std::string avatarId) {
    char *sql;
    sqlite3_stmt *stmt;
    
    if (cache.find(avatarId) == cache.end()) {
        ContactSet contactSet = ContactSet();
        sqlite3 *db = LLSqlMgr::instance().getDB();
        sql = "SELECT cs.ID,R,G,B,A,IFNULL(cs.ALIAS,cs.ID) FROM CONTACT_SET_AVATARS csa,CONTACTS_SET cs where csa.CONTACT_SET_ID=cs.ID and csa.AVATAR_ID=?";
        sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        
        sqlite3_bind_text(stmt, 1,  avatarId.c_str(), strlen(avatarId.c_str()), 0);
        while ( sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *id = sqlite3_column_text(stmt, 0);
            std::string csId = reinterpret_cast<const char*>(id);
            
            double r = sqlite3_column_double(stmt, 1);
            double g = sqlite3_column_double(stmt, 2);
            double b = sqlite3_column_double(stmt, 3);
            double a = sqlite3_column_double(stmt, 4);
            const unsigned char *name = sqlite3_column_text(stmt, 5);
            std::string contactSetName = std::string(reinterpret_cast<const char*>(name));
            contactSet.setId(csId);
            contactSet.setColor(LLColor4(F32(r),F32(g),F32(b),F32(a)));
            contactSet.setName(contactSetName);
            cache[avatarId] = contactSet;
            

        }
        sqlite3_finalize(stmt);
        return contactSet;
    } else {
        return cache[avatarId];
    }
}
void GenxContactSetMgr::resetAvatarContactSet(std::string avatarId) {
   char *sql;
   sqlite3_stmt *stmt;
   
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "DELETE FROM CONTACT_SET_AVATARS WHERE AVATAR_ID=?";
     
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
   sqlite3_bind_text(stmt, 1,  avatarId.c_str(), strlen(avatarId.c_str()), 0);
   
   
   sqlite3_step(stmt);
   sqlite3_finalize(stmt);
   cache[avatarId] = ContactSet();
}

void GenxContactSetMgr::updateAvatarContactSet(std::string avatarId, std::string contactSetId) {
   
   char *sql;
   sqlite3_stmt *stmt;
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "INSERT INTO CONTACT_SET_AVATARS(CONTACT_SET_ID,AVATAR_ID)"  \
         "VALUES(?,?)"  \
         "ON CONFLICT(AVATAR_ID) DO UPDATE SET"  \
         "   CONTACT_SET_ID=excluded.CONTACT_SET_ID";
        



   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   sqlite3_bind_text(stmt, 1,  contactSetId.c_str(), strlen(contactSetId.c_str()), 0);
   sqlite3_bind_text(stmt, 2,  avatarId.c_str(), strlen(avatarId.c_str()), 0);
   
   
   sqlite3_step(stmt);
   sqlite3_finalize(stmt);
   cache.erase(avatarId);
   


}

void GenxContactSetMgr::updateContactSet(ContactSet contactSet) {
   char *sql;
   sqlite3_stmt *stmt;
   const auto rgb_color = contactSet.getColor().getValue();
   F32 r = rgb_color[0];
   F32 g = rgb_color[1];
   F32 b = rgb_color[2];
   std::string csId = contactSet.getId();
   std::string csAlias = contactSet.getName();
   sqlite3 *db = LLSqlMgr::instance().getDB();
   sql = "UPDATE CONTACTS_SET SET R=?,G=?,B=?,A=1.0,ALIAS=? WHERE ID = ?";
   sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   sqlite3_bind_double(stmt, 1,  r);
   sqlite3_bind_double(stmt, 2,  g);
   sqlite3_bind_double(stmt, 3,  b);
   
   sqlite3_bind_text(stmt, 4,  csAlias.c_str(), strlen(csAlias.c_str()), 0);
   sqlite3_bind_text(stmt, 5,  csId.c_str(), strlen(csId.c_str()), 0);
   sqlite3_step(stmt);
   
   sqlite3_finalize(stmt);

   //update cache
   cache.clear();
}
std::string GenxContactSetMgr::insertContactSet(std::string csId, std::string csAlias) {
     
   char *sql;
   sqlite3_stmt *stmt;
   std::string result;
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
   
   sqlite3_step(stmt);
  
   sqlite3_finalize(stmt);
   return csId;
}
void GenxContactSetMgr::deleteContactSet(std::string csId) {
     
   char *sql;
   sqlite3_stmt *stmt;
   LL_DEBUGS() << "Deleting Contact Set from Genesis DB" << LL_ENDL;
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
   cache.clear();
}