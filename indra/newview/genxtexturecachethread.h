#ifndef GENX_TEXTURE_CACHE_THREAD_H
#define GENX_TEXTURE_CACHE_THREAD_H
#include "llworkerthread.h"
class GenxTextureCacheThread : public LLWorkerThread
{
    friend class LLImageFormatted;
    friend class GenxTextureCacheThreadWorker;
public:

	
	
	

	
    GenxTextureCacheThread();
	~GenxTextureCacheThread();
    /*virtual*/ S32 update(F32 max_time_ms);
    handle_t readFromCache(const std::string& filename, const LLUUID& id, LLPointer<LLImageFormatted> mFormattedImage, LLResponder* responder);

	
	bool readComplete(handle_t handle, bool abort);
    void addCompleted(LLResponder* responder, bool success);
private:
    typedef std::vector<std::pair<LLPointer<LLResponder>, bool> > responder_list_t;
	responder_list_t mCompletedList;
    typedef std::map<handle_t, GenxTextureCacheThreadWorker*> handle_map_t;
	// Internal
	// Internal
	LLMutex mWorkersMutex;
	LLMutex mHeaderMutex;
	LLMutex mListMutex;
    handle_map_t mReaders;    
    typedef std::vector<handle_t> handle_list_t;
	handle_list_t mPrioritizeWriteList;

	
};
#endif GENX_TEXTURE_CACHE_THREAD_H