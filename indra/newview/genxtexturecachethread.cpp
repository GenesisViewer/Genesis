#include "llviewerprecompiledheaders.h"
#include "llimage.h"
#include "genxtexturecachethread.h"
#include "genxtexturecache.h"
#include "llimagej2c.h"
class GenxTextureCacheThreadWorker : public LLWorkerClass
{
	friend class GenxTextureCacheThread;
public:
	GenxTextureCacheThreadWorker(GenxTextureCacheThread* cache, const std::string& filename,const LLUUID& id,
						 LLPointer<LLImageFormatted> formattedImage,
						 LLResponder* responder)
		: LLWorkerClass(cache, "GenxTextureCacheThreadWorker"),
		  mID(id),
		  mCache(cache),
          mFileName(filename),
		  
		  mFormattedImage(formattedImage),
          
		  mResponder(responder)

	{
		
	}
	~GenxTextureCacheThreadWorker()
	{
		llassert_always(!haveWork());
		
	}
    handle_t read() { addWork(0, LLWorkerThread::PRIORITY_HIGH | mPriority); return mRequestHandle; }
	handle_t write() { addWork(1, LLWorkerThread::PRIORITY_HIGH | mPriority); return mRequestHandle; }
    bool complete() { return checkWork(); }
    bool doWork(S32 param); // Called from LLWorkerThread::processRequest()
    bool doRead();
	bool doWrite();
protected:
	GenxTextureCacheThread* mCache;
	U32 mPriority;
	LLUUID	mID;
	
	LLPointer<LLImageFormatted> mFormattedImage;
	U8* mWriteData;
	
    std::string mFileName;
	LLPointer<LLResponder> mResponder;
private:
    virtual void startWork(S32 param); // called from addWork() (MAIN THREAD)
	virtual void finishWork(S32 param, bool completed); // called from finishRequest() (WORK THREAD)
	virtual void endWork(S32 param, bool aborted); // called from doWork() (MAIN THREAD)    

};
void GenxTextureCacheThreadWorker::startWork(S32 param)
{
}
void GenxTextureCacheThreadWorker::endWork(S32 param,bool aborted)
{
}
void GenxTextureCacheThreadWorker::finishWork(S32 param,bool completed)
{
    if (mResponder.notNull())
	{
        mResponder->completed(true);
	}
}
void GenxTextureCacheThread::addCompleted(LLResponder* responder, bool success)
{
	LLMutexLock lock(&mListMutex);
	mCompletedList.push_back(std::make_pair(responder,success));
}
S32 GenxTextureCacheThread::update(F32 max_time_ms)
{
	

	S32 res;
	res = LLWorkerThread::update(max_time_ms);

	
	
	

	return res;
}
bool GenxTextureCacheThreadWorker::doRead()
{
    
    GenxTextureCache::instance().readTextureCache(mID,mFormattedImage,mFileName);
   
    
    
    
    return true;
}
bool GenxTextureCacheThreadWorker::doWrite()
{
    return true;
}
//virtual
bool GenxTextureCacheThreadWorker::doWork(S32 param)
{
	bool res = false;
	if (param == 0) // read
	{
		res = doRead();
	}
	else if (param == 1) // write
	{
		res = doWrite();
	}
	else
	{
		llassert_always(0);
	}
	return res;
}
GenxTextureCacheThread::GenxTextureCacheThread()
	: LLWorkerThread("GenxTextureCache", TRUE)
	  
{
    
}

GenxTextureCacheThread::~GenxTextureCacheThread()
{
	
}

//////////////////////////////////////////////////////////////////////////////

// Calls from texture pipeline thread (i.e. LLTextureFetch)

GenxTextureCacheThread::handle_t GenxTextureCacheThread::readFromCache(const std::string& filename, const LLUUID& id, LLPointer<LLImageFormatted> mFormattedImage, LLResponder* responder)
{
	// // Note: checking to see if an entry exists can cause a stall,
	// //  so let the thread handle it
	// LLMutexLock lock(&mWorkersMutex);
	GenxTextureCacheThreadWorker* worker = new GenxTextureCacheThreadWorker(this, filename, id,
																	 mFormattedImage,
																	 responder);
	handle_t handle = worker->read();
	
	return handle;
}




// bool GenxTextureCacheThread::readComplete(handle_t handle, bool abort)
// {
// 	mWorkersMutex.lock();
// 	handle_map_t::iterator iter = mReaders.find(handle);
// 	GenxTextureCacheThreadWorker* worker = NULL;
// 	bool complete = false;
// 	if (iter != mReaders.end())
// 	{
// 		worker = iter->second;
// 		complete = worker->complete();

// 		if(!complete && abort)
// 		{
// 			abortRequest(handle, true);
// 		}
// 	}
// 	if (worker && (complete || abort))
// 	{
// 		mReaders.erase(iter);
//       	mWorkersMutex.unlock();
// 		worker->scheduleDelete();
// 	}
// 	else
// 	{
// 		mWorkersMutex.unlock();
// 	}
// 	return (complete || abort);
// }



