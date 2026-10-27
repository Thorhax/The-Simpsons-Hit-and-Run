//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#ifndef _GLCON_HPP_
#define _GLCON_HPP_

#define BUFFERED_VERTS 1024

#include <pddi/pddi.hpp>
#include <pddi/base/basecontext.hpp>

class pglDisplay;
class pglDevice;

class pglExtContext;
class pglExtGamma;

//--------------------------------------------------------------
// Skips GL state calls that wouldn't change anything. Mesa still pays for
// every call, and some (materials, enables) make it revalidate the
// fixed-function state before the next draw. Only the GL backend changes this
// state, so the cache stays in sync as long as all changes go through it.
struct pglStateCache
{
    enum Cap
    {
        CAP_TEXTURE_2D,
        CAP_ALPHA_TEST,
        CAP_BLEND,
        CAP_LIGHTING,
        CAP_CULL_FACE,
        NUM_CAPS
    };

    void Invalidate();

    void Enable(Cap cap, bool enable);
    void AlphaFunc(unsigned func, float ref);
    void BlendEquation(unsigned equation);
    void BlendFunc(unsigned src, unsigned dst);
    void TexEnvMode(int mode);
    void Material(float shininess, const float* ambient, const float* emissive,
                  const float* diffuse, const float* specular);
    void SwapInterval(int interval);

private:
    signed char enabled[NUM_CAPS];  // -1 when unknown
    bool alphaValid, blendEquationValid, blendFuncValid, materialValid;
    unsigned alphaFunc, blendEquation, blendSrc, blendDst;
    float alphaRef;
    int texEnvMode;
    int swapInterval;
    float material[17];             // shininess and the four colours
};

//--------------------------------------------------------------
class pglContext : public pddiBaseContext
{
public :
    pglContext(pglDevice* dev, pglDisplay* disp);
    ~pglContext();

    // frame synchronisation
    void BeginFrame();
    void EndFrame();

    // buffer clearing
    void Clear(unsigned bufferMask);

    // viewport clipping
    void SetScissor(pddiRect* rect);

    // immediate mode prim rendering
    pddiPrimStream* BeginPrims(pddiShader* material, pddiPrimType primType, unsigned vertexType, int vertexCount = 0, unsigned pass = 0);
    void EndPrims(pddiPrimStream* stream);

    // retained mode prim rendering
    void DrawPrimBuffer(pddiShader* material, pddiPrimBuffer* buffer);

    // lighting
    int GetMaxLights();
    void SetAmbientLight(pddiColour col);

    // backface culling
    void SetCullMode(pddiCullMode mode);

    // colour buffer control
    void SetColourWrite( bool red, bool green, bool blue, bool alpha );

    // z-buffer control
    void EnableZBuffer(bool enable);
    void SetZCompare(pddiCompareMode compareMode);
    void SetZWrite(bool);
    void SetZBias(float bias);
    void SetZRange(float n, float f);

    // stencil buffer control
    void EnableStencilBuffer(bool enable);
    void SetStencilCompare(pddiCompareMode compare);
    void SetStencilRef(int ref);
    void SetStencilMask(unsigned mask);
    void SetStencilWriteMask(unsigned mask);
    void SetStencilOp(pddiStencilOp failOp, pddiStencilOp zFailOp, pddiStencilOp zPassOp);

    // polygon fill
    void SetFillMode(pddiFillMode mode);

    // fog
    void EnableFog(bool enable);
    void SetFog(pddiColour colour, float start, float end);

    // utility
    int GetMaxTextureDimension(void);

    // extensions
    pddiExtension* GetExtension(unsigned extID);
    bool VerifyExtension(unsigned extID);

    // internal pddiglfunctions
    pglDisplay* GetDisplay(void) {return display;}

    unsigned contextID;

    pglStateCache stateCache;

protected:
    void LoadHardwareMatrix(pddiMatrixType id);
    void SetupHardwareProjection(void);
    void SetupHardwareLight(int);
    void  BeginTiming(void);
    float EndTiming(void);

    void SetVertexArray(unsigned descr, void* data, int count);

    pglDevice* device;
    pglDisplay* display;

    pglExtContext* extContext;
    pglExtGamma* extGamma;

    pddiShader* defaultShader;

//   int nBuffered;
//   unsigned currentMatId;
//   pddiScreenVertex buffer[BUFFERED_VERTS];

// void FlushBuffer(void);
// BOOL ZTestQuery(float x, float y, float z);
// void ZTestPoint(float x, float y, float z);
//   pddiFillMode fillMode;

//   int yRes;

//   ULONG compareMode;
//   BOOL zWrite;
//   float alphaRef;

    int maxTexSize;
};

class pglPrimBufferStream;

class pglPrimBuffer : public pddiPrimBuffer
{
public:
    pglPrimBuffer(pglContext* context, pddiPrimType type, unsigned vertexFormat, int nVertex, int nIndex);
    ~pglPrimBuffer();

    pddiPrimBufferStream* Lock();
    void Unlock(pddiPrimBufferStream* stream);

    unsigned char* LockIndexBuffer();
    void UnlockIndexBuffer(int count);

    void SetIndices(unsigned short* indices, int count);

    bool CheckMemImageVersion(int version) { return false; }
    void* LockMemImage(unsigned) { return NULL;}
    void UnlockMemImage() { }
    unsigned GetMemImageLength() {return 0; }
    void SetMemImageParam(unsigned param, unsigned value) { /**/ }

    void Display(void);

protected:
    friend class pglPrimBufferStream;
    pglPrimBufferStream* stream;
    pglContext* context;

    pddiPrimType primType;
    unsigned vertexType;

    int nStrips;
    int* strips;

    float* coord;
    float* normal;
    float* uv;
    unsigned char* colour;

    unsigned allocated;
    unsigned total;

    unsigned short* indices;
    unsigned indexCount;

    bool valid;
    GLuint vertexBuffer;
    GLuint indexBuffer;

    unsigned mem;
};
    
#endif

