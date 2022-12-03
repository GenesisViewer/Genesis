#include "llsingleton.h" 
#include "llsqlmgr.h"
#include "llimage.h"
#ifndef GENX_TEXTURE_CACHE_H
#define GENX_TEXTURE_CACHE_H



class GenxTextureCache: public LLSingleton<GenxTextureCache>
{
 public:
	GenxTextureCache();
	void writeTextureCache (LLUUID textureId, S32 size, U8* buffer);
    void readTextureCache(LLUUID textureId, LLPointer<LLImageFormatted> formattedImage,std::string url);
};

#endif // GENX_TEXTURE_CACHE_H