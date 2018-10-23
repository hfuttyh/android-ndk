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
    mProgram = createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
    if (!mProgram)
        return false;

    AAsset* asset = AAssetManager_open(mgr, "computer_shader.vs", AASSET_MODE_BUFFER);
    off64_t len = AAsset_getLength64(asset);
    char* src = new char[len];
    AAsset_read(asset, src, len);
    AAsset_close(asset);

    mComputeProgram = createComputeProgram(src);
    if (!mComputeProgram)
        return false;

    iLocPosition = glGetAttribLocation(mProgram, "a_v4Position");
    iLocFillColor = glGetAttribLocation(mProgram, "a_v4FillColor");

    glGenBuffers(1, &gVBO);

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
    GLint iLocRadius = glGetUniformLocation(mComputeProgram, "radius");
    int gIndexBufferBinding = 0;

    glUniform1f(iLocRadius, (float)mFrameNumber);
    mFrameNumber = (++mFrameNumber)%1000;

//Bind it
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gVBO);

//Set the data of the buffer
    glBufferData(GL_SHADER_STORAGE_BUFFER, 32 * 256, &mBuf, GL_DYNAMIC_DRAW);

// Bind the VBO onto SSBO, which is going to filled in witin the compute
// shader.
// gIndexBufferBinding is equal to 0 (same as the compute shader binding)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, gIndexBufferBinding, gVBO);

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
// Submit job for the compute shader execution.
// GROUP_SIZE_HEIGHT = GROUP_SIZE_WIDTH = 8
// NUM_VERTS_H = NUM_VERTS_V = 16
// As the result the function is called with the following parameters:
// glDispatchCompute(2, 2, 1)
    glDispatchCompute(4, 4, 1);
// Unbind the SSBO buffer.
// gIndexBufferBinding is equal to 0 (same as the compute shader binding)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, gIndexBufferBinding, 0);

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT|GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBuffer( GL_ARRAY_BUFFER, gVBO );
    int32_t* buff = (int32_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, 32*256, GL_MAP_READ_BIT);
    for (int k = 0; k < 256; k++)
    {
        int offset = k*8;
        ALOGE("id:%d, position wx:%d, wy:%d, gx:%d, gy:%d, lxx:%d, ly:%d, nx:%d, ny:%d\n", k,
              buff[offset], buff[offset+1], buff[offset+2], buff[offset+3],
              buff[offset+4], buff[offset+5], buff[offset+6], buff[offset+7]);
    }
    glBindBuffer( GL_ARRAY_BUFFER, 0);
    checkGlError("Renderer::render");
}

void RenderES3::render() {
//    step();
//    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
//
//    glBindBuffer( GL_ARRAY_BUFFER, gVBO );
//
    glUseProgram(mProgram);
//
//    glEnableVertexAttribArray(iLocPosition);
//    glEnableVertexAttribArray(iLocFillColor);
//// Draw points from VBO
//    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
//    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
//    // glDrawArrays(GL_POINTS, 0, 256);
//    float* buff = (float*)glMapBufferRange(GL_ARRAY_BUFFER, 0, 32*256, GL_MAP_READ_BIT);
//    for (int k = 0; k < 256; k++)
//    {
//        ALOGE("id:%d, position x:%5.2f, y:%5.2f, z:%5.2f, w:%5.2f\n", k, buff[offset], buff[offset+1], buff[offset+2], buff[offset+3]);
//    }
//    glBindBuffer( GL_ARRAY_BUFFER, 0);
//    checkGlError("Renderer::render");
}
