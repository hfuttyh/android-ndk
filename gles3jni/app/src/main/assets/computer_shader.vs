#version 310 es
// The uniform parameters which is passed from application for every frame.
uniform float radius;
// Declare custom data struct, which represents either vertex or colour.
struct Vector3f
{
      uint x;
      uint y;
      uint z;
      uint w;
};
// Declare the custom data type, which represents one point of a circle.
// And this is vertex position and colour respectively.
// As you may already noticed that will define the interleaved data within
// buffer which is Vertex|Colour|Vertex|Colour|…
struct AttribData
{
      Vector3f v;
      Vector3f c;
};
// Declare input/output buffer from/to wich we will read/write data.
// In this particular shader we only write data into the buffer.
// If you do not want your data to be aligned by compiler try to use:
// packed or shared instead of std140 keyword.
// We also bind the buffer to index 0. You need to set the buffer binding
// in the range [0..3] – this is the minimum range approved by Khronos.
// Notice that various platforms might support more indices than that.
layout(std140, binding = 0) buffer destBuffer
{
      AttribData data[];
} outBuffer;
// Declare what size is the group. In our case is 8x8, which gives
// 64 group size.
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
// Declare main program function which is executed once
// glDispatchCompute is called from the application.
void main()
{
      // Read current global position for this thread
      ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);
      // Calculate the global number of threads (size) for this
      uint gWidth = gl_WorkGroupSize.x * gl_NumWorkGroups.x;
      uint gHeight = gl_WorkGroupSize.y * gl_NumWorkGroups.y;
      uint gSize = gWidth * gHeight;
      // Since we have 1D array we need to calculate offset.
      uint offset = uint(storePos.y) * gWidth + uint(storePos.x);
      // Calculate an angle for the current thread
      float alpha = 2.0 * 3.14159265359 * (float(offset) / float(gSize));
      // Calculate vertex position based on the already calculate angle
      // and radius, which is given by application
      // outBuffer.data[offset].v.x = sin(alpha) * radius;
      // outBuffer.data[offset].v.y = cos(alpha) * radius;
      // outBuffer.data[offset].v.z = 0.0;
      // outBuffer.data[offset].v.w = 1.0;
      // Assign colour for the vertex
      // outBuffer.data[offset].c.x = float(storePos.x) / float(gWidth);
      // outBuffer.data[offset].c.y = 0.0;
      // outBuffer.data[offset].c.z = 1.0;
      // outBuffer.data[offset].c.w = 1.0;
      outBuffer.data[offset].v.x = gl_WorkGroupID.x;
      outBuffer.data[offset].v.y = gl_WorkGroupID.y;
      outBuffer.data[offset].v.z = gl_GlobalInvocationID.x;
      outBuffer.data[offset].v.w = gl_GlobalInvocationID.y;
      outBuffer.data[offset].c.x = gl_LocalInvocationID.x;
      outBuffer.data[offset].c.y = gl_LocalInvocationID.y;
      outBuffer.data[offset].c.z = gl_NumWorkGroups.x;
      outBuffer.data[offset].c.w = gl_NumWorkGroups.y;

}