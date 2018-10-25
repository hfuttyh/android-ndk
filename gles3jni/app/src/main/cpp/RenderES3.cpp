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

#define STR(s) #s
#define STRV(s) STR(s)

#define POS_ATTRIB 0
#define COLOR_ATTRIB 1
#define SCALEROT_ATTRIB 2
#define OFFSET_ATTRIB 3

static const char VERTEX_SHADER[] =
    "#version 310 es\n"
    "in vec4 a_v4Position;\n"
    "in vec4 a_v4FillColor;\n"
    "out vec4 v_v4FillColor;\n"
    "void main()\n"
    "{\n"
    "      v_v4FillColor = a_v4FillColor;\n"
    "      gl_Position = a_v4Position;\n"
    "}\n";

static const char FRAGMENT_SHADER[] =
    "#version 310 es\n"
    "precision mediump float;\n"
    "in vec4 v_v4FillColor;\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "      FragColor = v_v4FillColor;\n"
    "}";

void print2dFloatArray(float* buff, int x, int y, int offset){
    char str[10240];
    for (int i = 0; i < y; i++){
        memset(str, 0, 1024);
        sprintf(str, "%d:", i);
        for (int j = 0; j< x; j++){
            sprintf(str, "%s %6.4f", str, buff[offset+i*x+j]);
        }
        ALOGE("%s", str);
    }
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
    mDataLen = len/4*2;
    mDataBuff = new float[len/4*2];
    buff = (char*) mDataBuff;
    AAsset_read(asset, buff, len);
    AAsset_close(asset);

    glGenBuffers(1, &gDataBuff);
    glGenBuffers(1, &gCCBuff);

    step();
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

//Bind Data
    int gIndexBufferBinding = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gDataBuff);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*mDataLen, mDataBuff, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gDataBuff);
    print2dFloatArray(mDataBuff, 160, 240, 0);
    ALOGE("-0-0-0-0-0-0-0-0-0-0-0-0");
    print2dFloatArray(mDataBuff, 160, 240, 38400);

//Bind it
    gIndexBufferBinding = 1;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gCCBuff);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*mConvLen, mConvCoreBuff,  GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gCCBuff);
    print2dFloatArray(mConvCoreBuff, 3, 3, 0);

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
    glDispatchCompute(1, 1, 1);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);

    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glBindBuffer( GL_ARRAY_BUFFER, gDataBuff );
    float* buff = (float *)glMapBufferRange(GL_ARRAY_BUFFER, 0, 4*mDataLen, GL_MAP_READ_BIT);

    print2dFloatArray(buff, 160, 240, 0);
    ALOGE("=============================================");
    print2dFloatArray(buff, 160, 240, 160*240);

    glBindBuffer( GL_ARRAY_BUFFER, 0);
    checkGlError("Renderer::render");
}

void RenderES3::render() {
}
