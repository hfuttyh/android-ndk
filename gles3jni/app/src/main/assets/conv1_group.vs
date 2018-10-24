#version 310 es
#define PIXW 160
#define PIXH 240
#define CONVCORESIZE 9
layout(std140, binding = 0) buffer workSpace
{
      float data[];
};
layout(std140, binding = 1) buffer convCore
{
      float data[];
};

// Declare what size is the group. In our case is 8x8, which gives
// 64 group size.
layout (local_size_x = PIXW, local_size_y = PIXH, local_size_z = 1) in;
// 一个group执行一个通道的卷积，x、y的size和图像大小一致
// 通过WorkGroupId定位输出在destBuffer中的起始偏移,WorkGroup按一维定义
// LocalInvocationId为当前线程在workgroup中的id，可以确定执行哪个像素的卷积运算
// [0,PIXW*PIXH)为原始通道图像内容，输出从PIXW*PIXH开始
void main()
{
	uint outBufferBase = (gl_WorkGroupID.x+1)*PIXW*PIXH;
	uint offset = outBufferBase;
	uint convCoreOffset = gl_WorkGroupID.x*(CONVCORESIZE+1); // 9 convcore, 1 bias
	ivec2 position = gl_LocalInvocationID.xy;

	for (int k = 0; k < CONVCORESIZE; )

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