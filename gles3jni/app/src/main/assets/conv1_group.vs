#version 310 es
#define PIXW 320
#define PIXH 480
#define CONVCORESIZE 9
layout(std430, binding = 0) buffer inBuffer
{
    float pic[];
} picBuffer;

layout(std430, binding = 1) buffer inconvBuffer
{
    float conv[];
} convBuffer;

layout(std430, binding = 2) buffer oBuffer
{
	float data[];
} outBuffer;

layout(std430, binding = 3) buffer tBuffer
{
	float tmp[];
} tmpBuffer;

// Declare what size is the group. In our case is 8x8, which gives
// 64 group size.
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
// 一个group线程数有上限，最低上限1024，所以设计成一个group执行一列卷积，y和高度一致
// 通过WorkGroupId定位输出在destBuffer中的起始偏移,WorkGroup按一维定义
// LocalInvocationId为当前线程在workgroup中的id，可以确定执行哪个像素的卷积运算
// gl_WorkGroupID.x定义当前卷积处理像素的横坐标
// gl_WorkGroupID.y应当恒为1
// gl_WorkGroupID.z定义卷积核id即输出通道数
void main()
{
	// if (gl_LocalInvocationID.y >= uint(PIXH))
	// 	return;

	int baseOffset = (int(gl_WorkGroupID.x)+1)*PIXW*PIXH;
	int convCoreOffset = int(gl_WorkGroupID.z)*(CONVCORESIZE+1); // 9 convcore, 1 bias
	ivec2 position = ivec2(gl_WorkGroupID.x, gl_WorkGroupID.y);
	int pixOffset = position.y*PIXW + position.x;

	// tmpBuffer.tmp[9] = float(gl_WorkGroupID.x);
	// tmpBuffer.tmp[10] = float(gl_WorkGroupID.y);
	// tmpBuffer.tmp[11] = float(gl_LocalInvocationID.x);
	// tmpBuffer.tmp[12] = float(gl_LocalInvocationID.y);

	float pix[9];
	pix[0] = position.y > 0 && position.x > 0 ? picBuffer.pic[pixOffset-PIXW-1] : 0.f;
	pix[1] = position.y > 0 ? picBuffer.pic[pixOffset-PIXW] : 0.f;
	pix[2] = position.y > 0 && position.x < PIXW-1 ? picBuffer.pic[pixOffset-PIXW+1] : 0.f;
	pix[3] = position.x > 0 ? picBuffer.pic[pixOffset-1] : 0.f;
	pix[4] = picBuffer.pic[pixOffset];
	pix[5] = position.x < PIXW-1 ? picBuffer.pic[pixOffset+1] : 0.f;
	pix[6] = position.y < PIXH-1 && position.x > 0 ? picBuffer.pic[pixOffset+PIXW-1] : 0.f;
	pix[7] = position.y < PIXH-1 ? picBuffer.pic[pixOffset+PIXW] : 0.f;
	pix[8] = position.y < PIXH-1 && position.x < PIXW-1 ? picBuffer.pic[pixOffset+PIXW+1] : 0.f;

	float result = 0.f;
	for (int k = 0; k < 9; k++)
	{
		// tmpBuffer.tmp[k] = pix[k];
	    result += convBuffer.conv[convCoreOffset+k]*pix[k];
	}
	// tmpBuffer.tmp[0] = picBuffer.pic[pixOffset];
	// tmpBuffer.tmp[1] = picBuffer.pic[0];

	outBuffer.data[pixOffset] = result + convBuffer.conv[convCoreOffset+9];
}