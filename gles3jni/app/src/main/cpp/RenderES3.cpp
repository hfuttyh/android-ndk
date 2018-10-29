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

#define PIXW 160
#define PIXH 240

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

    asset = AAssetManager_open(mgr, "model_param_conv1.bin", AASSET_MODE_BUFFER);
    len = AAsset_getLength64(asset);
    mConv1Len = len/4;
    mConv1Buff = new float[len/4];
    buff = (char*) mConv1Buff;
    AAsset_read(asset, buff, len);
    AAsset_close(asset);

    asset = AAssetManager_open(mgr, "model_param_conv3.bin", AASSET_MODE_BUFFER);
    len = AAsset_getLength64(asset);
    mConv3Len = len/4;
    mConv3Buff = new float[len/4];
    buff = (char*) mConv3Buff;
    AAsset_read(asset, buff, len);
    AAsset_close(asset);

    asset = AAssetManager_open(mgr, "img_y.bin", AASSET_MODE_BUFFER);
    len = AAsset_getLength64(asset);
    mDataLen = len/4;
    mDataBuff = new float[mDataLen];
    buff = (char*) mDataBuff;
    AAsset_read(asset, buff, len);
    AAsset_close(asset);

    glGenBuffers(1, &gDataBuff);
    glGenBuffers(1, &gCCBuff);

    GPUInfo();
    conv1();

    // computerConvolutionCPU(mDataBuff, mConvCoreBuff);
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

void RenderES3::GPUInfo() {
    glUseProgram(mComputeProgram);

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
}

void RenderES3::computeConv(int pixW, int pixH, int inputChannel, int outputChannel,
    GLuint gDataInt, GLuint gConvInt, GLuint gOutDataInt)
{
    glUseProgram(mComputeProgram);

    GLint iInputChannel = glGetUniformLocation(mComputeProgram, "inputChannel");
    glUniform1i(iInputChannel, inputChannel);

    GLint iInputW = glGetUniformLocation(mComputeProgram, "inputW");
    glUniform1i(iInputW, pixW);

    GLint iInputH = glGetUniformLocation(mComputeProgram, "inputH");
    glUniform1i(iInputH, pixH);

    float infoData[256];
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gDataInt);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gConvInt);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gOutDataInt);

    GLuint gInfoId;
    glGenBuffers(1, &gInfoId);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gInfoId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*256, infoData, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gInfoId);

    ALOGE("===============GPU start=============");
    glDispatchCompute(PIXW, PIXH, outputChannel);

    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
    glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0);
}

void RenderES3::conv1() {
    int outputChannel = 4;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gDataBuff);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*mDataLen, mDataBuff, GL_DYNAMIC_DRAW);
    // print2dFloatArray(mDataBuff, PIXW, PIXH, 0);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gCCBuff);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*mConv1Len, mConv1Buff,  GL_STATIC_DRAW);
    print2dFloatArray(mConv1Buff, 3, 3, 0);

    ConvOutData outData;
    glGenBuffers(1, &outData.gint);
    outData.buffLen = outputChannel*PIXW*PIXH;
    outData.buff = new float[outData.buffLen];
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, outData.gint);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*outData.buffLen, outData.buff, GL_DYNAMIC_DRAW);

    computeConv(PIXW, PIXH, 1, outputChannel, gDataBuff, gCCBuff, outData.gint);

    float * buff;
//    glBindBuffer( GL_SHADER_STORAGE_BUFFER, outData.gint );
//    buff = (float *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4*outData.buffLen, GL_MAP_READ_BIT);
    ALOGE("===============GPU end=============");
//    print2dFloatArray(buff, PIXW, 30, 0);
//    print2dFloatArray(buff, PIXW, 30, PIXW*PIXH);
//    print2dFloatArray(buff, PIXW, 30, 2*PIXW*PIXH);
//    print2dFloatArray(buff, PIXW, 30, 3*PIXW*PIXH);
//    glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gDataBuff);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*mDataLen, mDataBuff, GL_DYNAMIC_DRAW);
    // print2dFloatArray(mDataBuff, PIXW, PIXH, 0);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gCCBuff);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*mConv3Len, mConv3Buff,  GL_STATIC_DRAW);
    // print2dFloatArray(mConv1Buff, 3, 3, 0);

    ConvOutData outData2;
    glGenBuffers(1, &outData2.gint);
    outData2.buffLen = outputChannel*PIXW*PIXH;
    outData2.buff = new float[outData.buffLen];
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, outData2.gint);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*outData2.buffLen, outData2.buff, GL_DYNAMIC_DRAW);

    computeConv(PIXW, PIXH, 4, outputChannel, outData.gint, gCCBuff, outData2.gint);

    glBindBuffer( GL_SHADER_STORAGE_BUFFER, outData2.gint );
    buff = (float *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4*outData2.buffLen, GL_MAP_READ_BIT);
    ALOGE("===============GPU end=============");
    print2dFloatArray(buff, PIXW, 30, 0);
    print2dFloatArray(buff, PIXW, 30, PIXW*PIXH);
    print2dFloatArray(buff, PIXW, 30, 2*PIXW*PIXH);
    print2dFloatArray(buff, PIXW, 30, 3*PIXW*PIXH);
    glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0);
}

void RenderES3::conv2() {

}

void RenderES3::render() {
}
