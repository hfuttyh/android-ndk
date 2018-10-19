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
    bool init();

private:
    enum {VB_INSTANCE, VB_SCALEROT, VB_OFFSET, VB_COUNT};

    virtual float* mapOffsetBuf();
    virtual void unmapOffsetBuf();
    virtual float* mapTransformBuf();
    virtual void unmapTransformBuf();
    virtual void draw(unsigned int numInstances);
    virtual void step();
    virtual void render();

    const EGLContext mEglContext;
    GLuint mProgram;
    GLuint mComputeProgram;
    GLuint mVB[VB_COUNT];
    GLuint mVBState;

    int mFrameNumber = 0;
    GLuint gVBO;
    GLuint iLocPosition;
    GLuint iLocFillColor;
};

Renderer* createES3Renderer() {
    RendererES3* renderer = new RendererES3;
    if (!renderer->init()) {
        delete renderer;
        return NULL;
    }
    return renderer;
}

RendererES3::RendererES3()
:   mEglContext(eglGetCurrentContext()),
    mProgram(0),
    mVBState(0)
{
    for (int i = 0; i < VB_COUNT; i++)
        mVB[i] = 0;
}

bool RendererES3::init() {
    mProgram = createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
    if (!mProgram)
        return false;

    mComputeProgram = createComputeProgram(COMPUTER_SHADER);
    if (!mComputeProgram)
        return false;

    iLocPosition = glGetAttribLocation(mProgram, "a_v4Position");
    iLocFillColor = glGetAttribLocation(mProgram, "a_v4FillColor");
//    glGenBuffers(VB_COUNT, mVB);
//    glBindBuffer(GL_ARRAY_BUFFER, mVB[VB_INSTANCE]);
//    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD), &QUAD[0], GL_STATIC_DRAW);
//    glBindBuffer(GL_ARRAY_BUFFER, mVB[VB_SCALEROT]);
//    glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCES * 4*sizeof(float), NULL, GL_DYNAMIC_DRAW);
//    glBindBuffer(GL_ARRAY_BUFFER, mVB[VB_OFFSET]);
//    glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCES * 2*sizeof(float), NULL, GL_STATIC_DRAW);
//
//    glGenVertexArrays(1, &mVBState);
//    glBindVertexArray(mVBState);
//
//    glBindBuffer(GL_ARRAY_BUFFER, mVB[VB_INSTANCE]);
//    glVertexAttribPointer(POS_ATTRIB, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const GLvoid*)offsetof(Vertex, pos));
//    glVertexAttribPointer(COLOR_ATTRIB, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (const GLvoid*)offsetof(Vertex, rgba));
//    glEnableVertexAttribArray(POS_ATTRIB);
//    glEnableVertexAttribArray(COLOR_ATTRIB);
//
//    glBindBuffer(GL_ARRAY_BUFFER, mVB[VB_SCALEROT]);
//    glVertexAttribPointer(SCALEROT_ATTRIB, 4, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);
//    glEnableVertexAttribArray(SCALEROT_ATTRIB);
//    glVertexAttribDivisor(SCALEROT_ATTRIB, 1);
//
//    glBindBuffer(GL_ARRAY_BUFFER, mVB[VB_OFFSET]);
//    glVertexAttribPointer(OFFSET_ATTRIB, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), 0);
//    glEnableVertexAttribArray(OFFSET_ATTRIB);
//    glVertexAttribDivisor(OFFSET_ATTRIB, 1);

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

float* RendererES3::mapOffsetBuf() {
    glBindBuffer(GL_ARRAY_BUFFER, mVB[VB_OFFSET]);
    return (float*)glMapBufferRange(GL_ARRAY_BUFFER,
            0, MAX_INSTANCES * 2*sizeof(float),
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
}

void RendererES3::unmapOffsetBuf() {
    glUnmapBuffer(GL_ARRAY_BUFFER);
}

float* RendererES3::mapTransformBuf() {
    glBindBuffer(GL_ARRAY_BUFFER, mVB[VB_SCALEROT]);
    return (float*)glMapBufferRange(GL_ARRAY_BUFFER,
            0, MAX_INSTANCES * 4*sizeof(float),
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
}

void RendererES3::unmapTransformBuf() {
    glUnmapBuffer(GL_ARRAY_BUFFER);
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
// Bind the VBO onto SSBO, which is going to filled in witin the compute
// shader.
// gIndexBufferBinding is equal to 0 (same as the compute shader binding)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, gIndexBufferBinding, gVBO);
// Submit job for the compute shader execution.
// GROUP_SIZE_HEIGHT = GROUP_SIZE_WIDTH = 8
// NUM_VERTS_H = NUM_VERTS_V = 16
// As the result the function is called with the following parameters:
// glDispatchCompute(2, 2, 1)
    glDispatchCompute(2, 2, 1);
// Unbind the SSBO buffer.
// gIndexBufferBinding is equal to 0 (same as the compute shader binding)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, gIndexBufferBinding, 0);
}

void RendererES3::render() {
    step();
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glBindBuffer( GL_ARRAY_BUFFER, gVBO );

    glUseProgram(mProgram);

    glEnableVertexAttribArray(iLocPosition);
    glEnableVertexAttribArray(iLocFillColor);
// Draw points from VBO
    glDrawArrays(GL_POINTS, 0, 16);
    checkGlError("Renderer::render");
}
