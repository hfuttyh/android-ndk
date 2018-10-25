#version 310 es
#define PIXW 32
#define PIXH 32
#define CONVCORESIZE 9
layout(std140, binding = 0) buffer destBuffer
{
      float data[];
} workSpace;
layout(std140, binding = 1) buffer convBuffer
{
      float data[];
} convCore;

// Declare what size is the group. In our case is 8x8, which gives
// 64 group size.
layout (local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
// 一个group执行一个通道的卷积，x、y的size和图像大小一致
// 通过WorkGroupId定位输出在destBuffer中的起始偏移,WorkGroup按一维定义
// LocalInvocationId为当前线程在workgroup中的id，可以确定执行哪个像素的卷积运算
// [0,PIXW*PIXH)为原始通道图像内容，输出从PIXW*PIXH开始
void main()
{
	int baseOffset = (int(gl_WorkGroupID.x)+1)*PIXW*PIXH;
	int convCoreOffset = int(gl_WorkGroupID.x)*(CONVCORESIZE+1); // 9 convcore, 1 bias
      int pixOffset = int(gl_LocalInvocationID.y)*PIXW + int(gl_LocalInvocationID.x);
      ivec2 position = ivec2(gl_LocalInvocationID.xy);

      float pix[9];
      pix[0] = position.y > 0 && position.x > 0 ? workSpace.data[pixOffset-PIXW-1] : 0.f;
      pix[1] = position.y > 0 ? workSpace.data[pixOffset-PIXW] : 0.f;
      pix[2] = position.y > 0 && position.x < PIXW-1 ? workSpace.data[pixOffset-PIXW+1] : 0.f;
      pix[3] = position.x > 0 ? workSpace.data[pixOffset-1] : 0.f;
      pix[4] = workSpace.data[pixOffset];
      pix[5] = position.x < PIXW-1 ? workSpace.data[pixOffset+1] : 0.f;
      pix[6] = position.y < PIXH-1 && position.x > 0 ? workSpace.data[pixOffset+PIXW-1] : 0.f;
      pix[7] = position.y < PIXH-1 ? workSpace.data[pixOffset+PIXW] : 0.f;
      pix[8] = position.y < PIXH-1 && position.x < PIXW-1 ? workSpace.data[pixOffset+PIXW+1] : 0.f;

      float result = 0.f;
      for (int k = 0; k < 9; k++)
      {
            result += convCore.data[convCoreOffset+k]*pix[k];
      }

      workSpace.data[baseOffset+pixOffset] = result;
}