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
#include <EGL/egl.h>
#include <stdio.h>
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

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

static const char COMPUTER_SHADER[] =
    "#version 310 es\n"
    "// The uniform parameters which is passed from application for every frame.\n"
    "uniform float radius;\n"
    "// Declare custom data struct, which represents either vertex or colour.\n"
    "struct Vector3f\n"
    "{\n"
    "      float x;\n"
    "      float y;\n"
    "      float z;\n"
    "      float w;\n"
    "};\n"
    "// Declare the custom data type, which represents one point of a circle.\n"
    "// And this is vertex position and colour respectively.\n"
    "// As you may already noticed that will define the interleaved data within\n"
    "// buffer which is Vertex|Colour|Vertex|Colour|…\n"
    "struct AttribData\n"
    "{\n"
    "      Vector3f v;\n"
    "      Vector3f c;\n"
    "};\n"
    "// Declare input/output buffer from/to wich we will read/write data.\n"
    "// In this particular shader we only write data into the buffer.\n"
    "// If you do not want your data to be aligned by compiler try to use:\n"
    "// packed or shared instead of std140 keyword.\n"
    "// We also bind the buffer to index 0. You need to set the buffer binding\n"
    "// in the range [0..3] – this is the minimum range approved by Khronos.\n"
    "// Notice that various platforms might support more indices than that.\n"
    "layout(std140, binding = 0) buffer destBuffer\n"
    "{\n"
    "      AttribData data[];\n"
    "} outBuffer;\n"
    "// Declare what size is the group. In our case is 8x8, which gives\n"
    "// 64 group size.\n"
    "layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n"
    "// Declare main program function which is executed once\n"
    "// glDispatchCompute is called from the application.\n"
    "void main()\n"
    "{\n"
    "      // Read current global position for this thread\n"
    "      ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);\n"
    "      // Calculate the global number of threads (size) for this\n"
    "      uint gWidth = gl_WorkGroupSize.x * gl_NumWorkGroups.x;\n"
    "      uint gHeight = gl_WorkGroupSize.y * gl_NumWorkGroups.y;\n"
    "      uint gSize = gWidth * gHeight;\n"
    "      // Since we have 1D array we need to calculate offset.\n"
    "      uint offset = uint(storePos.y) * gWidth + uint(storePos.x);\n"
    "      // Calculate an angle for the current thread\n"
    "      float alpha = 2.0 * 3.14159265359 * (float(offset) / float(gSize));\n"
    "      // Calculate vertex position based on the already calculate angle\n"
    "      // and radius, which is given by application\n"
    "      outBuffer.data[offset].v.x = sin(alpha) * radius;\n"
    "      outBuffer.data[offset].v.y = cos(alpha) * radius;\n"
    "      outBuffer.data[offset].v.z = 0.0;\n"
    "      outBuffer.data[offset].v.w = 1.0;\n"
    "      // Assign colour for the vertex\n"
    "      outBuffer.data[offset].c.x = float(storePos.x) / float(gWidth);\n"
    "      outBuffer.data[offset].c.y = 0.0;\n"
    "      outBuffer.data[offset].c.z = 1.0;\n"
    "      outBuffer.data[offset].c.w = 1.0;\n"
    "}";

class RendererES3: public Renderer {
public:
    RendererES3();
    virtual ~RendererES3();
    bool init(AAssetManager* mgr);

private:
    virtual void draw(unsigned int numInstances);
    virtual void step();
    virtual void render();

    const EGLContext mEglContext;
    GLuint mProgram;
    GLuint mComputeProgram;
    GLuint mVBState;

    int mFrameNumber = 0;
    GLuint gVBO;
    GLuint iLocPosition;
    GLuint iLocFillColor;

    int32_t mBuf[32*256];
};

Renderer* createES3Renderer(AAssetManager* asset) {
    RendererES3* renderer = new RendererES3;
    if (!renderer->init(asset)) {
        delete renderer;
        return NULL;
    }
    return renderer;
}

RendererES3::RendererES3()
:   mEglContext(eglGetCurrentContext()),
    mProgram(0),
    mVBState(0)
{}

bool RendererES3::init(AAssetManager* mgr) {
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

RendererES3::~RendererES3() {
    /* The destructor may be called after the context has already been
     * destroyed, in which case our objects have already been destroyed.
     *
     * If the context exists, it must be current. This only happens when we're
     * cleaning up after a failed init().
     */
    if (eglGetCurrentContext() != mEglContext)
        return;
//    glDeleteVertexArrays(1, &mVBState);
//    glDeleteBuffers(VB_COUNT, mVB);
    glDeleteProgram(mProgram);
}

void RendererES3::draw(unsigned int numInstances) {
    glUseProgram(mProgram);
    glBindVertexArray(mVBState);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, numInstances);
}

void RendererES3::step() {
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
// Submit job for the compute shader execution.
// GROUP_SIZE_HEIGHT = GROUP_SIZE_WIDTH = 8
// NUM_VERTS_H = NUM_VERTS_V = 16
// As the result the function is called with the following parameters:
// glDispatchCompute(2, 2, 1)
    glDispatchCompute(2, 2, 1);
// Unbind the SSBO buffer.
// gIndexBufferBinding is equal to 0 (same as the compute shader binding)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, gIndexBufferBinding, 0);

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glBindBuffer( GL_ARRAY_BUFFER, gVBO );

    glUseProgram(mProgram);

//    glEnableVertexAttribArray(iLocPosition);
//    glEnableVertexAttribArray(iLocFillColor);
// Draw points from VBO
//    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
//    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
    // glDrawArrays(GL_POINTS, 0, 256);
    int32_t* buff = (int32_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, 32*256, GL_MAP_READ_BIT);
    int size = 8;
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

void RendererES3::render() {
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
