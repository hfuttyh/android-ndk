/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include "gles3jni.h"
#include "RenderES3.h"

#define PIXW 320
#define PIXH 480

void print2dFloatArray(float* buff, int x, int y, int offset){
    char str[102400];
    for (int i = 0; i < y; i++){
        memset(str, 0, 102400);
        sprintf(str, "%d:", i);
        for (int j = 0; j< x; j++){
            sprintf(str, "%s %6.4f", str, buff[offset+i*x+j]);
        }
        ALOGE("%s", str);
    }
}

void print2dUintArray(uint * buff, int x, int y, int offset){
    char str[10240];
    for (int i = 0; i < y; i++){
        memset(str, 0, 1024);
        sprintf(str, "%d:", i);
        for (int j = 0; j< x; j++){
            sprintf(str, "%s %d", str, buff[offset+i*x+j]);
        }
        ALOGE("%s", str);
    }
}

float getDataByXY(float* data, int x, int y)
{
    if (x < 0 || y < 0 || x >= PIXW || y >= PIXH)
        return 0;

    return data[y*PIXW+x];
}

void computerConvolutionCPU(float* data, float* conv)
{
    ALOGE("==============CPU START=============");
    float outbuff[PIXH*PIXW];
    for (int y = 0; y < PIXH; y++)
    {
        for (int x = 0; x < PIXW; x++)
        {
            float ret =
                getDataByXY(data, x-1, y-1)*conv[0] +
                getDataByXY(data, x, y-1) * conv[1] +
                getDataByXY(data, x+1, y-1)*conv[2] +
                getDataByXY(data, x-1, y) * conv[3] +
                getDataByXY(data, x, y) * conv[4] +
                getDataByXY(data, x+1, y) * conv[5] +
                getDataByXY(data, x-1, y+1)*conv[6] +
                getDataByXY(data, x, y+1) * conv[7] +
                getDataByXY(data, x+1, y+1)*conv[8] +
                conv[9];
            outbuff[y*PIXW+x] = tanh(ret);
        }
    }
    ALOGE("==============CPU END=============");
    print2dFloatArray(outbuff, PIXW, 40, 0);
}

RenderES3* createES3Renderer(AAssetManager* asset) {
    RenderES3* renderer = new RenderES3;
    if (!renderer->init(asset)) {
        delete renderer;
        return NULL;
    }
    return renderer;
}

RenderES3::RenderES3()
:   mEglContext(eglGetCurrentContext()),
    mProgram(0),
    mVBState(0)
{}

bool RenderES3::init(AAssetManager* mgr) {
    char* buff;
    AAsset* asset = AAssetManager_open(mgr, "conv1_group.vs", AASSET_MODE_BUFFER);
    off64_t len = AAsset_getLength64(asset);
    buff = new char[len];
    AAsset_read(asset, buff, len);
    AAsset_close(asset);

    mComputeProgram = createComputeProgram(buff);
    if (!mComputeProgram)
        return false;
    delete[] buff;

    asset = AAssetManager_open(mgr, "model_param.bin", AASSET_MODE_BUFFER);
    len = AAsset_getLength64(asset);
    mConvLen = len/4;
    mConvCoreBuff = new float[len/4];
    buff = (char*) mConvCoreBuff;
    AAsset_read(asset, buff, len);
    AAsset_close(asset);

    asset = AAssetManager_open(mgr, "img_y.bin", AASSET_MODE_BUFFER);
    len = AAsset_getLength64(asset);
    mDataLen = len;
    mDataBuff = new float[mDataLen];
    buff = (char*) mDataBuff;
    AAsset_read(asset, buff, len);
    AAsset_close(asset);

    memcpy(buff+len, buff, len);
    memcpy(buff+2*len, buff, 2*len);

    glGenBuffers(1, &gDataBuff);
    glGenBuffers(1, &gCCBuff);

    step();

    computerConvolutionCPU(mDataBuff, mConvCoreBuff);
    ALOGV("Using OpenGL ES 3.0 renderer");
    return true;
}

RenderES3::~RenderES3() {
    /* The destructor may be called after the context has already been
     * destroyed, in which case our objects have already been destroyed.
     *
     * If the context exists, it must be current. This only happens when we're
     * cleaning up after a failed init().
     */
    if (eglGetCurrentContext() != mEglContext)
        return;
    glDeleteProgram(mProgram);
    glDeleteProgram(mComputeProgram);
}

void RenderES3::step() {
    glUseProgram(mComputeProgram);

    GLint iInputChannel = glGetUniformLocation(mComputeProgram, "inputChannel");
    glUniform1i(iInputChannel, 1);

    GLint iInputW = glGetUniformLocation(mComputeProgram, "inputW");
    glUniform1i(iInputW, PIXW);

    GLint iInputH = glGetUniformLocation(mComputeProgram, "inputH");
    glUniform1i(iInputH, PIXH);

    float infoData[256];
//Bind Data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gDataBuff);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*mDataLen, mDataBuff, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gDataBuff);
    // print2dFloatArray(mDataBuff, PIXW, PIXH, 0);

//Bind it
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gCCBuff);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*mConvLen, mConvCoreBuff,  GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gCCBuff);
    print2dFloatArray(mConvCoreBuff, 3, 3, 0);

    GLuint gOutId;
    float outBuff[mDataLen];
    glGenBuffers(1, &gOutId);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gOutId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*mDataLen, outBuff, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gOutId);

    GLuint gInfoId;
    glGenBuffers(1, &gInfoId);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gInfoId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*256, infoData, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gInfoId);

    GLint out;
    GLenum enumName = GL_MAX_COMPUTE_WORK_GROUP_COUNT;
    ALOGE("GL_MAX_COMPUTE_WORK_GROUP_COUNT: ");
    for (int k = 0; k < 3; k++)
    {
        glGetIntegeri_v(enumName, 0, &out);
        ALOGE("%d\n", out);
    }

    enumName = GL_MAX_COMPUTE_WORK_GROUP_SIZE;
    ALOGE("GL_MAX_COMPUTE_WORK_GROUP_SIZE: ");
    for (int k = 0; k < 3; k++)
    {
        glGetIntegeri_v(enumName, 0, &out);
        ALOGE("%d\n", out);
    }

    enumName = GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS;
    ALOGE("GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS: ");
    for (int k = 0; k < 3; k++)
    {
        glGetIntegeri_v(enumName, 0, &out);
        ALOGE("%d\n", out);
    }

    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &out);
    ALOGE("GL_MAX_SHADER_STORAGE_BLOCK_SIZE:%d\n", out);
    ALOGE("===============GPU start=============");
    glDispatchCompute(PIXW, PIXH, 1);


    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glBindBuffer( GL_SHADER_STORAGE_BUFFER, gOutId );
    float* buff = (float *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4*mDataLen, GL_MAP_READ_BIT);
    ALOGE("===============GPU end=============");

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);

    print2dFloatArray(buff, PIXW, 30, 0);


    glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0);
    checkGlError("Renderer::render");
}

void RenderES3::render() {
}
