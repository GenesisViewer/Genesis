/** 
 * @file llvfs.h
 * @brief Definition of virtual file system
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLVFS_H
#define LL_LLVFS_H

#include <deque>
#include "lluuid.h"
#include "llassettype.h"
#include "llthread.h"

enum EVFSValid 
{
	VFSVALID_UNKNOWN = 0, 
	VFSVALID_OK = 1,
	VFSVALID_BAD_CORRUPT = 2,
	VFSVALID_BAD_CANNOT_OPEN_READONLY = 3,
	VFSVALID_BAD_CANNOT_CREATE = 4
};

// Lock types for open vfiles, pending async reads, and pending async appends
// (There are no async normal writes, currently)
enum EVFSLock
{
	VFSLOCK_OPEN = 0,
	VFSLOCK_READ = 1,
	VFSLOCK_APPEND = 2,

	VFSLOCK_COUNT = 3
};

//<edit>
//the VFS explorer requires that the class definition of these be available outside of llvfs
class LLVFSBlock
{
public:
	LLVFSBlock();

	LLVFSBlock(U32 loc, S32 size);

	static bool locationSortPredicate(
		const LLVFSBlock* lhs,
		const LLVFSBlock* rhs);

public:
	U32 mLocation;
	S32	mLength;		// allocated block size
};

class LLVFSFileSpecifier
{
public:
	LLVFSFileSpecifier();
	LLVFSFileSpecifier(const LLUUID &file_id, const LLAssetType::EType file_type);
	bool operator<(const LLVFSFileSpecifier &rhs) const;
	bool operator==(const LLVFSFileSpecifier &rhs) const;

public:
	LLUUID mFileID;
	LLAssetType::EType mFileType;
};

class LLVFSFileBlock : public LLVFSBlock, public LLVFSFileSpecifier
{
public:
	LLVFSFileBlock();
	LLVFSFileBlock(const LLUUID &file_id, LLAssetType::EType file_type, U32 loc = 0, S32 size = 0);
	void init();
#ifdef LL_LITTLE_ENDIAN
	inline void swizzleCopy(void *dst, void *src, int size);
#else
	inline U32 swizzle32(U32 x);
	inline U16 swizzle16(U16 x);
	inline void swizzleCopy(void *dst, void *src, int size);
#endif
	void serialize(U8 *buffer);
	void deserialize(U8 *buffer, const S32 index_loc);
	static BOOL insertLRU(LLVFSFileBlock* const& first,
						  LLVFSFileBlock* const& second);
	S32  mSize;
	S32  mIndexLocation; // location of index entry
	U32  mAccessTime;
	BOOL mLocks[VFSLOCK_COUNT]; // number of outstanding locks of each type

	static const S32 SERIAL_SIZE;
};
//<edit>

class LLVFS
{
private:
	// Use createLLVFS() to open a VFS file
	// Pass 0 to not presize
	LLVFS(const std::string& index_filename, 
			const std::string& data_filename, 
			const BOOL read_only, 
			const U32 presize, 
			const BOOL remove_after_crash);
public:
	~LLVFS();

	// Use this function normally to create LLVFS files
	// Pass 0 to not presize
	static LLVFS * createLLVFS(const std::string& index_filename, 
			const std::string& data_filename, 
			const BOOL read_only, 
			const U32 presize, 
			const BOOL remove_after_crash);

	BOOL isValid() const			{ return (VFSVALID_OK == mValid); }
	EVFSValid getValidState() const	{ return mValid; }

	// ---------- The following fucntions lock/unlock mDataMutex ----------
	BOOL getExists(const LLUUID &file_id, const LLAssetType::EType file_type);
	S32	 getSize(const LLUUID &file_id, const LLAssetType::EType file_type);

	BOOL checkAvailable(S32 max_size);
	
	S32  getMaxSize(const LLUUID &file_id, const LLAssetType::EType file_type);
	BOOL setMaxSize(const LLUUID &file_id, const LLAssetType::EType file_type, S32 max_size);

	void renameFile(const LLUUID &file_id, const LLAssetType::EType file_type,
		const LLUUID &new_id, const LLAssetType::EType &new_type);
	void removeFile(const LLUUID &file_id, const LLAssetType::EType file_type);

	S32 getData(const LLUUID &file_id, const LLAssetType::EType file_type, U8 *buffer, S32 location, S32 length);
	S32 storeData(const LLUUID &file_id, const LLAssetType::EType file_type, const U8 *buffer, S32 location, S32 length);

	void incLock(const LLUUID &file_id, const LLAssetType::EType file_type, EVFSLock lock);
	void decLock(const LLUUID &file_id, const LLAssetType::EType file_type, EVFSLock lock);
	BOOL isLocked(const LLUUID &file_id, const LLAssetType::EType file_type, EVFSLock lock);
	// ----------------------------------------------------------------

	// Used to trigger evil WinXP behavior of "preloading" entire file into memory.
	void pokeFiles();

	// Verify that the index file contents match the in-memory file structure
	// Very slow, do not call routinely. JC
	void audit();
	// Check for uninitialized blocks.  Slow, do not call in release. JC
	void checkMem();
	// for debugging, prints a map of the vfs
	void dumpMap();
	void dumpLockCounts();
	void dumpStatistics();
	void listFiles();
	void dumpFiles();

protected:
	void removeFileBlock(LLVFSFileBlock *fileblock);
	
	void eraseBlockLength(LLVFSBlock *block);
	void eraseBlock(LLVFSBlock *block);
	void addFreeBlock(LLVFSBlock *block);
	//void mergeFreeBlocks();
	void useFreeSpace(LLVFSBlock *free_block, S32 length);
	void sync(LLVFSFileBlock *block, BOOL remove = FALSE);
	void presizeDataFile(const U32 size);

	static LLFILE *openAndLock(const std::string& filename, const char* mode, BOOL read_lock);
	static void unlockAndClose(FILE *fp);
	
	// Can initiate LRU-based file removal to make space.
	// The immune file block will not be removed.
	LLVFSBlock *findFreeBlock(S32 size, LLVFSFileBlock *immune = NULL);

	// lock/unlock data mutex (mDataMutex)
	void lockData() { mDataMutex->lock(); }
	void unlockData() { mDataMutex->unlock(); }	
	
protected:
	LLMutex* mDataMutex;

//<edit>
public:
	typedef std::map<LLVFSFileSpecifier, LLVFSFileBlock*> fileblock_map;
	std::map<LLVFSFileSpecifier, LLVFSFileBlock*> getFileList();
//</edit>
protected:
	fileblock_map mFileBlocks;

	typedef std::multimap<S32, LLVFSBlock*>	blocks_length_map_t;
	blocks_length_map_t 	mFreeBlocksByLength;
	typedef std::multimap<U32, LLVFSBlock*>	blocks_location_map_t;
	blocks_location_map_t 	mFreeBlocksByLocation;

	LLFILE *mDataFP;
	LLFILE *mIndexFP;

	std::deque<S32> mIndexHoles;

	std::string mIndexFilename;
	std::string mDataFilename;
	BOOL mReadOnly;

	EVFSValid mValid;

	S32 mLockCounts[VFSLOCK_COUNT];
	BOOL mRemoveAfterCrash;
};

extern LLVFS *gVFS;

#endif
