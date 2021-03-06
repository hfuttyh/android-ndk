//
// Created by Stephenjin on 2018/10/23.
//

#ifndef GLES3JNI_RENDERES3_H
#define GLES3JNI_RENDERES3_H
#if DYNAMIC_ES3
#include "gl3stub.h"
#else
// Include the latest possible header file( GL version header )
#if __ANDROID_API__ >= 24
#include <GLES3/gl32.h>
#elif __ANDROID_API__ >= 21
#include <GLES3/gl31.h>
#else
#include <GLES3/gl3.h>
#endif

#endif

#include <EGL/egl.h>
#include <stdio.h>
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

typedef struct convOutStruct{
    GLuint gint;
    float* buff;
    int buffLen;
} ConvOutData;

class RenderES3{
public:
    RenderES3();
    virtual ~RenderES3();
    bool init(AAssetManager* mgr);
    void resize(int w, int h){
        glViewport(0, 0, w, h);
    }
    void computeConv(int pixW, int pixH, int inputChannel, int outputChannel,
                     GLuint gDataInt, GLuint gConvInt, GLuint gOutDataInt);
    void GPUInfo();
    void conv1();
    void conv2();
    void render();

private:

    const EGLContext mEglContext;
    GLuint mProgram;
    GLuint mComputeProgram;
    GLuint mVBState;

    int mFrameNumber = 0;
    GLuint gDataBuff;
    GLuint gCCBuff;

    int mDataLen;
    int mConv1Len;
    int mConv3Len;
    float *mDataBuff;
    float *mConv1Buff;
    float *mConv3Buff;
};
#endif //GLES3JNI_RENDERES3_H
