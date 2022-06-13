/** 
 * @file llvocache.cpp
 * @brief Cache of objects on the viewer.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llvocache.h"

#include "llerror.h"
#include "llregionhandle.h"
#include "llviewercontrol.h"

BOOL check_read(LLAPRFile* apr_file, void* src, S32 n_bytes) 
{
	return apr_file->read(src, n_bytes) == n_bytes ;
}

BOOL check_write(LLAPRFile* apr_file, void* src, S32 n_bytes) 
{
	return apr_file->write(src, n_bytes) == n_bytes ;
}
//---------------------------------------------------------------------------
// LLVOCacheEntry
//---------------------------------------------------------------------------

LLVOCacheEntry::LLVOCacheEntry(U32 local_id, U32 crc, LLDataPackerBinaryBuffer &dp)
	:
	mLocalID(local_id),
	mCRC(crc),
	mHitCount(0),
	mDupeCount(0),
	mCRCChangeCount(0)
{
	mBuffer = new U8[dp.getBufferSize()];
	mDP.assignBuffer(mBuffer, dp.getBufferSize());
	mDP = dp; //memcpy
}

LLVOCacheEntry::LLVOCacheEntry()
	:
	mLocalID(0),
	mCRC(0),
	mHitCount(0),
	mDupeCount(0),
	mCRCChangeCount(0),
	mBuffer(NULL)
{
	mDP.assignBuffer(mBuffer, 0);
}

LLVOCacheEntry::LLVOCacheEntry(LLAPRFile* apr_file)
	: mBuffer(NULL)
{
	S32 size = -1;
	BOOL success;

	mDP.assignBuffer(mBuffer, 0);
	success = check_read(apr_file, &mLocalID, sizeof(U32));
	if(success)
	{
		success = check_read(apr_file, &mCRC, sizeof(U32));
	}
	if(success)
	{
		success = check_read(apr_file, &mHitCount, sizeof(S32));
	}
	if(success)
	{
		success = check_read(apr_file, &mDupeCount, sizeof(S32));
	}
	if(success)
	{
		success = check_read(apr_file, &mCRCChangeCount, sizeof(S32));
	}
	if(success)
	{
		success = check_read(apr_file, &size, sizeof(S32));

	// Corruption in the cache entries
	if ((size > 10000) || (size < 1))
	{
		// We've got a bogus size, skip reading it.
		// We won't bother seeking, because the rest of this file
		// is likely bogus, and will be tossed anyway.
			LL_WARNS() << "Bogus cache entry, size " << size << ", aborting!" << LL_ENDL;
			success = FALSE;
		}
	}
	if(success && size > 0)
	{
		mBuffer = new U8[size];
		success = check_read(apr_file, mBuffer, size);

		if(success)
		{
			mDP.assignBuffer(mBuffer, size);
		}
		else
		{
			delete[] mBuffer ;
			mBuffer = NULL ;
		}
	}

	if(!success)
	{
		mLocalID = 0;
		mCRC = 0;
		mHitCount = 0;
		mDupeCount = 0;
		mCRCChangeCount = 0;
		mBuffer = NULL;
	}
}

LLVOCacheEntry::~LLVOCacheEntry()
{
	mDP.freeBuffer();
}


// New CRC means the object has changed.
void LLVOCacheEntry::assignCRC(U32 crc, LLDataPackerBinaryBuffer &dp)
{
	if (  (mCRC != crc)
		||(mDP.getBufferSize() == 0))
	{
		mCRC = crc;
		mHitCount = 0;
		mCRCChangeCount++;

		mDP.freeBuffer();
		mBuffer = new U8[dp.getBufferSize()];
		mDP.assignBuffer(mBuffer, dp.getBufferSize());
		mDP = dp;
	}
}

LLDataPackerBinaryBuffer *LLVOCacheEntry::getDP(U32 crc)
{
	if (  (mCRC != crc)
		||(mDP.getBufferSize() == 0))
	{
		//LL_INFOS() << "Not getting cache entry, invalid!" << LL_ENDL;
		return NULL;
	}
	mHitCount++;
	return &mDP;
}


void LLVOCacheEntry::recordHit()
{
	mHitCount++;
}


void LLVOCacheEntry::dump() const
{
	LL_INFOS() << "local " << mLocalID
		<< " crc " << mCRC
		<< " hits " << mHitCount
		<< " dupes " << mDupeCount
		<< " change " << mCRCChangeCount
		<< LL_ENDL;
}

BOOL LLVOCacheEntry::writeToFile(LLAPRFile* apr_file) const
{
	BOOL success;
	success = check_write(apr_file, (void*)&mLocalID, sizeof(U32));
	if(success)
	{
		success = check_write(apr_file, (void*)&mCRC, sizeof(U32));
	}
	if(success)
	{
		success = check_write(apr_file, (void*)&mHitCount, sizeof(S32));
	}
	if(success)
	{
		success = check_write(apr_file, (void*)&mDupeCount, sizeof(S32));
	}
	if(success)
	{
		success = check_write(apr_file, (void*)&mCRCChangeCount, sizeof(S32));
	}
	if(success)
	{
		S32 size = mDP.getBufferSize();
		success = check_write(apr_file, (void*)&size, sizeof(S32));
	
		if(success)
		{
			success = check_write(apr_file, (void*)mBuffer, size);
		}
	}

	return success ;
}

//-------------------------------------------------------------------
//LLVOCache
//-------------------------------------------------------------------
// Format string used to construct filename for the object cache
static const char OBJECT_CACHE_FILENAME[] = "objects_%d_%d.slc";

const U32 MAX_NUM_OBJECT_ENTRIES = 128 ;
const U32 MIN_ENTRIES_TO_PURGE = 16 ;
const U32 INVALID_TIME = 0 ;
const char* object_cache_dirname = "objectcache";
const char* header_filename = "object.cache";

LLVOCache* LLVOCache::sInstance = NULL;

//static 
LLVOCache* LLVOCache::getInstance() 
{	
	if(!sInstance)
	{
		sInstance = new LLVOCache() ;
	}
	return sInstance ;
}

//static 
BOOL LLVOCache::hasInstance() 
{
	return sInstance != NULL ;
}

//static 
void LLVOCache::destroyClass() 
{
	if(sInstance)
	{
		delete sInstance ;
		sInstance = NULL ;
	}
}

LLVOCache::LLVOCache():
	mInitialized(FALSE),
	mReadOnly(TRUE),
	mNumEntries(0),
	mCacheSize(1)
{
	mEnabled = gSavedSettings.getBOOL("ObjectCacheEnabled");
}

LLVOCache::~LLVOCache()
{
	if(mEnabled)
	{
		writeCacheHeader();
		clearCacheInMemory();
	}
}

void LLVOCache::setDirNames(ELLPath location)
{
	mHeaderFileName = gDirUtilp->getExpandedFilename(location, object_cache_dirname, header_filename);
	mObjectCacheDirName = gDirUtilp->getExpandedFilename(location, object_cache_dirname);
}

void LLVOCache::initCache(ELLPath location, U32 size, U32 cache_version)
{
	if(!mEnabled)
	{
		LL_WARNS() << "Not initializing cache: Cache is currently disabled." << LL_ENDL;
		return ;
	}

	if(mInitialized)
	{
		LL_WARNS() << "Cache already initialized." << LL_ENDL;
		return ;
	}
	mInitialized = TRUE ;

	setDirNames(location);
	if (!mReadOnly)
	{
		LLFile::mkdir(mObjectCacheDirName);
	}
	mCacheSize = llclamp(size, MIN_ENTRIES_TO_PURGE, MAX_NUM_OBJECT_ENTRIES);
	mMetaInfo.mVersion = cache_version;
	readCacheHeader();	

	if(mMetaInfo.mVersion != cache_version) 
	{
		mMetaInfo.mVersion = cache_version ;
		if(mReadOnly) //disable cache
		{
			clearCacheInMemory();
		}
		else //delete the current cache if the format does not match.
		{			
			removeCache();
		}
	}	
}
	
void LLVOCache::removeCache(ELLPath location) 
{
	if(mReadOnly)
	{
		LL_WARNS() << "Not removing cache at " << location << ": Cache is currently in read-only mode." << LL_ENDL;
		return ;
	}

	LL_INFOS() << "about to remove the object cache due to settings." << LL_ENDL ;

	std::string mask = "*";
	std::string cache_dir = gDirUtilp->getExpandedFilename(location, object_cache_dirname);
	LL_INFOS() << "Removing cache at " << cache_dir << LL_ENDL;
	gDirUtilp->deleteFilesInDir(cache_dir, mask); //delete all files
	LLFile::rmdir(cache_dir);

	clearCacheInMemory();
	mInitialized = FALSE ;
}

void LLVOCache::removeCache() 
{
	llassert_always(mInitialized) ;
	if(mReadOnly)
	{
		LL_WARNS() << "Not clearing object cache: Cache is currently in read-only mode." << LL_ENDL;
		return ;
	}

	LL_INFOS() << "about to remove the object cache due to some error." << LL_ENDL ;

	std::string mask = "*";
	LL_INFOS() << "Removing cache at " << mObjectCacheDirName << LL_ENDL;
	gDirUtilp->deleteFilesInDir(mObjectCacheDirName, mask); 

	clearCacheInMemory() ;
	writeCacheHeader();
}

void LLVOCache::removeEntry(HeaderEntryInfo* entry) 
{
	llassert_always(mInitialized) ;
	if(mReadOnly)
	{
		return ;
	}
	if(!entry)
	{
		return ;
	}

	header_entry_queue_t::iterator iter = mHeaderEntryQueue.find(entry) ;
	if(iter != mHeaderEntryQueue.end())
	{		
		mHandleEntryMap.erase(entry->mHandle) ;		
		mHeaderEntryQueue.erase(iter) ;
		removeFromCache(entry) ;
		delete entry ;

		mNumEntries = mHandleEntryMap.size() ;
	}
}

void LLVOCache::removeEntry(U64 handle) 
{
	handle_entry_map_t::iterator iter = mHandleEntryMap.find(handle) ;
	if(iter == mHandleEntryMap.end()) //no cache
	{
		return ;
	}
	HeaderEntryInfo* entry = iter->second ;
	removeEntry(entry) ;
}

void LLVOCache::clearCacheInMemory()
{
	if(!mHeaderEntryQueue.empty()) 
	{
		for(header_entry_queue_t::iterator iter = mHeaderEntryQueue.begin(); iter != mHeaderEntryQueue.end(); ++iter)
		{
			delete *iter ;
		}
		mHeaderEntryQueue.clear();
		mHandleEntryMap.clear();
		mNumEntries = 0 ;
	}

}

void LLVOCache::getObjectCacheFilename(U64 handle, std::string& filename) 
{
	U32 region_x, region_y;

	grid_from_region_handle(handle, &region_x, &region_y);
	filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, object_cache_dirname,
			   llformat(OBJECT_CACHE_FILENAME, region_x, region_y));

	return ;
}

void LLVOCache::removeFromCache(HeaderEntryInfo* entry)
{
	if(mReadOnly)
	{
		LL_WARNS() << "Not removing cache for handle " << entry->mHandle << ": Cache is currently in read-only mode." << LL_ENDL;
		return ;
	}

	std::string filename;
	getObjectCacheFilename(entry->mHandle, filename);
	LLAPRFile::remove(filename);
	entry->mTime = INVALID_TIME ;
	updateEntry(entry) ; //update the head file.
}

void LLVOCache::readCacheHeader()
{
	if(!mEnabled)
	{
		LL_WARNS() << "Not reading cache header: Cache is currently disabled." << LL_ENDL;
		return;
	}

	//clear stale info.
	clearCacheInMemory();	

	bool success = true ;
	if (LLAPRFile::isExist(mHeaderFileName))
	{
		LLAPRFile apr_file(mHeaderFileName, APR_READ|APR_BINARY);		
		
		//read the meta element
		success = check_read(&apr_file, &mMetaInfo, sizeof(HeaderMetaInfo)) ;
		
		if(success)
		{
			HeaderEntryInfo* entry = NULL ;
			mNumEntries = 0 ;
			U32 num_read = 0 ;
			while(num_read++ < MAX_NUM_OBJECT_ENTRIES)
			{
				if(!entry)
				{
					entry = new HeaderEntryInfo() ;
				}
				success = check_read(&apr_file, entry, sizeof(HeaderEntryInfo));
								
				if(!success) //failed
				{
					LL_WARNS() << "Error reading cache header entry. (entry_index=" << mNumEntries << ")" << LL_ENDL;
					delete entry ;
					entry = NULL ;
					break ;
				}
				else if(entry->mTime == INVALID_TIME)
				{
					continue ; //an empty entry
				}

				entry->mIndex = mNumEntries++ ;
				mHeaderEntryQueue.insert(entry) ;
				mHandleEntryMap[entry->mHandle] = entry ;
				entry = NULL ;
			}
			if(entry)
			{
				delete entry ;
			}
		}

		//---------
		//debug code
		//----------
		//std::string name ;
		//for(header_entry_queue_t::iterator iter = mHeaderEntryQueue.begin() ; success && iter != mHeaderEntryQueue.end(); ++iter)
		//{
		//	getObjectCacheFilename((*iter)->mHandle, name) ;
		//	LL_INFOS() << name << LL_ENDL ;
		//}
		//-----------
	}
	else
	{
		writeCacheHeader() ;
	}

	if(!success)
	{
		removeCache() ; //failed to read header, clear the cache
	}
	else if(mNumEntries >= mCacheSize)
	{
		purgeEntries(mCacheSize) ;
	}

	return ;
}

void LLVOCache::writeCacheHeader()
{
	if (!mEnabled)
	{
		LL_WARNS() << "Not writing cache header: Cache is currently disabled." << LL_ENDL;
		return;
	}

	if(mReadOnly)
	{
		LL_WARNS() << "Not writing cache header: Cache is currently in read-only mode." << LL_ENDL;
		return;
	}

	bool success = true ;
	{
		LLAPRFile apr_file(mHeaderFileName, APR_CREATE|APR_WRITE|APR_BINARY);

		//write the meta element
		success = check_write(&apr_file, &mMetaInfo, sizeof(HeaderMetaInfo)) ;


		mNumEntries = 0 ;	
		for(header_entry_queue_t::iterator iter = mHeaderEntryQueue.begin() ; success && iter != mHeaderEntryQueue.end(); ++iter)
		{
			(*iter)->mIndex = mNumEntries++ ;
			success = check_write(&apr_file, (void*)*iter, sizeof(HeaderEntryInfo));
		}
	
		mNumEntries = mHeaderEntryQueue.size() ;
		if(success && mNumEntries < MAX_NUM_OBJECT_ENTRIES)
		{
			HeaderEntryInfo* entry = new HeaderEntryInfo() ;
			entry->mTime = INVALID_TIME ;
			for(S32 i = mNumEntries ; success && i < MAX_NUM_OBJECT_ENTRIES ; i++)
			{
				//fill the cache with the default entry.
				success = check_write(&apr_file, entry, sizeof(HeaderEntryInfo)) ;			

			}
			delete entry ;
		}
	}

	if(!success)
	{
		clearCacheInMemory() ;
		mReadOnly = TRUE ; //disable the cache.
	}
	return ;
}

BOOL LLVOCache::updateEntry(const HeaderEntryInfo* entry)
{
	LLAPRFile apr_file(mHeaderFileName, APR_WRITE|APR_BINARY);
	apr_file.seek(APR_SET, entry->mIndex * sizeof(HeaderEntryInfo) + sizeof(HeaderMetaInfo)) ;

	return check_write(&apr_file, (void*)entry, sizeof(HeaderEntryInfo)) ;
}

void LLVOCache::readFromCache(U64 handle, const LLUUID& id, LLVOCacheEntry::vocache_entry_map_t& cache_entry_map) 
{
	if(!mEnabled)
	{
		LL_WARNS() << "Not reading cache for handle " << handle << "): Cache is currently disabled." << LL_ENDL;
		return ;
	}
	llassert_always(mInitialized);

	handle_entry_map_t::iterator iter = mHandleEntryMap.find(handle) ;
	if(iter == mHandleEntryMap.end()) //no cache
	{
		LL_WARNS() << "No handle map entry for " << handle << LL_ENDL;
		return ;
	}

	bool success = true ;
	{
		std::string filename;
		getObjectCacheFilename(handle, filename);
		LLAPRFile apr_file(filename, APR_READ|APR_BINARY);
	
		LLUUID cache_id ;
		success = check_read(&apr_file, cache_id.mData, UUID_BYTES) ;
	
		if(success)
		{		
			if(cache_id != id)
			{
				LL_INFOS() << "Cache ID doesn't match for this region, discarding"<< LL_ENDL;
				success = false ;
			}

			if(success)
			{
				S32 num_entries;
				success = check_read(&apr_file, &num_entries, sizeof(S32)) ;
	
				if(success)
				{
					for (S32 i = 0; i < num_entries; i++)
					{
						LLVOCacheEntry* entry = new LLVOCacheEntry(&apr_file);
						if (!entry->getLocalID())
						{
							LL_WARNS() << "Aborting cache file load for " << filename << ", cache file corruption!" << LL_ENDL;
							delete entry ;
							success = false ;
							break ;
						}
						cache_entry_map[entry->getLocalID()] = entry;
					}
				}
			}
		}		
	}
	
	if(!success)
	{
		if(cache_entry_map.empty())
		{
			removeEntry(iter->second) ;
		}
	}

	return ;
}
	
void LLVOCache::purgeEntries(U32 size)
{
	while(mHeaderEntryQueue.size() > size)
	{
		header_entry_queue_t::iterator iter = mHeaderEntryQueue.begin() ;
		HeaderEntryInfo* entry = *iter ;			
		mHandleEntryMap.erase(entry->mHandle);
		mHeaderEntryQueue.erase(iter) ;
		removeFromCache(entry) ;
		delete entry;
	}
	mNumEntries = mHandleEntryMap.size() ;
}

void LLVOCache::writeToCache(U64 handle, const LLUUID& id, const LLVOCacheEntry::vocache_entry_map_t& cache_entry_map, BOOL dirty_cache) 
{
	if(!mEnabled)
	{
		LL_WARNS() << "Not writing cache for handle " << handle << "): Cache is currently disabled." << LL_ENDL;
		return ;
	}
	llassert_always(mInitialized);

	if(mReadOnly)
	{
		LL_WARNS() << "Not writing cache for handle " << handle << "): Cache is currently in read-only mode." << LL_ENDL;
		return ;
	}	

	HeaderEntryInfo* entry;
	handle_entry_map_t::iterator iter = mHandleEntryMap.find(handle) ;
	if(iter == mHandleEntryMap.end()) //new entry
	{				
		if(mNumEntries >= mCacheSize - 1)
		{
			purgeEntries(mCacheSize - 1) ;
		}

		entry = new HeaderEntryInfo();
		entry->mHandle = handle ;
		entry->mTime = time(NULL) ;
		entry->mIndex = mNumEntries++;
		mHeaderEntryQueue.insert(entry) ;
		mHandleEntryMap[handle] = entry ;
	}
	else
	{
		// Update access time.
		entry = iter->second ;		

		//resort
		mHeaderEntryQueue.erase(entry) ;
		
		entry->mTime = time(NULL) ;
		mHeaderEntryQueue.insert(entry) ;
	}

	//update cache header
	if(!updateEntry(entry))
	{
		LL_WARNS() << "Failed to update cache header index " << entry->mIndex << ". handle = " << handle << LL_ENDL;
		return ; //update failed.
	}

	if(!dirty_cache)
	{
		LL_WARNS() << "Skipping write to cache for handle " << handle << ": cache not dirty" << LL_ENDL;
		return ; //nothing changed, no need to update.
	}

	//write to cache file
	bool success = true ;
	{
		std::string filename;
		getObjectCacheFilename(handle, filename);
		LLAPRFile apr_file(filename, APR_CREATE|APR_WRITE|APR_BINARY);
	
		success = check_write(&apr_file, (void*)id.mData, UUID_BYTES) ;

	
		if(success)
		{
			S32 num_entries = cache_entry_map.size() ;
			success = check_write(&apr_file, &num_entries, sizeof(S32));
	
			for (LLVOCacheEntry::vocache_entry_map_t::const_iterator iter = cache_entry_map.begin(); success && iter != cache_entry_map.end(); ++iter)
			{
				success = iter->second->writeToFile(&apr_file) ;
			}
		}
	}

	if(!success)
	{
		removeEntry(entry) ;

	}

	return ;
}

