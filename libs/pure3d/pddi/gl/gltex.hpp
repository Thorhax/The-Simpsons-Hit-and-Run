//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#ifndef _GLTEX_HPP_
#define _GLTEX_HPP_
                 
#include <pddi/pddi.hpp>
#include <pddi/pdditype.hpp>
class pglContext;
         
class pglTexture : public pddiTexture
{
public:
    pglTexture(pglContext*);
    ~pglTexture();

    bool Create(int xSize, int ySize, int bpp, int alphaDepth, int nMip, pddiTextureType type = PDDI_TEXTYPE_RGB, pddiTextureUsageHint usageHint = PDDI_USAGE_STATIC);

    pddiPixelFormat GetPixelFormat();
    int GetWidth();
    int GetHeight();
    int GetDepth();
    int GetNumMipMaps();
    int GetAlphaDepth();

    // palette managment
    int GetNumPaletteEntries(void);
    void SetPalette(int nEntries, pddiColour* palette);
    int GetPalette(pddiColour* palette);

    // texture upload
    pddiLockInfo* Lock(int mipLevel, pddiRect* rect = 0);
    void Unlock(int mipLevel);

    // paging control
    void Prefetch(void);
    void Discard(void);
    void SetPriority(int priority);
    int GetPriority();

    // gl specific stuff
    void SetGLState(void);
    void Upload(void);

    // Textures are uploaded to the GPU on first use, so many new ones becoming
    // visible at once made long frames. Filled textures are queued instead and
    // a few are uploaded at the start of every frame. Returns false once the
    // deadline (an SDL performance counter) has passed.
    static void InitUploadQueue(void);
    static bool UploadQueued(unsigned long long deadline);
    // Filtering and wrapping of the bound texture, skipped when unchanged
    void SetSampler(int magFilter, int minFilter, int wrap);

protected:
    pglContext* context;
    unsigned contextID;

    int log2X, log2Y;
    int xSize, ySize;
    pddiTextureType type;
    int nMipMap;

    bool valid;
    unsigned int gltexture;
    int priority;
    int samplerMag, samplerMin, samplerWrap;
    bool queued;    // in the upload queue

    pddiLockInfo lock;

    char** bits;
};
#endif

