#version 310 es
#define CONVCORESIZE 9
// X <= Y return 1.0f, otherwise return 0.0f
// X lessEqual Y
#define XLEY(X, Y) step(float(X), float(Y))
// X bigger than Y
#define XBY(X, Y) (1.0f-step(float(X), float(Y)))

uniform int inputChannel;
uniform int inputW;
uniform int inputH;

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

// shader代码定义为计算一个卷积核在相应像素位置计算卷积结果
// gl_LocalInvocationId.z为当前卷积输入通道数
// 受制于LocalInvocation上限较低，使用WorkGroupID来定位当前处理的像素位置
// gl_WorkGroupID.x定义当前卷积处理像素的x坐标
// gl_WorkGroupID.y应当当前卷积处理像素的y坐标
// gl_WorkGroupID.z定义卷积核id即输出通道数
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

// float tanh(float x)
// {
// 	float ex = exp(x);
// 	float enx = exp(-x);
// 	return (ex-enx)/(ex+enx);
// }

void main()
{
	// if (gl_LocalInvocationID.y >= uint(inputH))
	// 	return;

	int baseOffset = (int(gl_WorkGroupID.x)+1)*inputW*inputH;
	int convCoreOffset = int(gl_WorkGroupID.z)*(CONVCORESIZE+1); // 9 convcore, 1 bias
	ivec2 position = ivec2(gl_WorkGroupID.x, gl_WorkGroupID.y);
	int pixOffset = position.y*inputW + position.x;

	float pix[9];
	float boolret;
	// pix[0] = position.y > 0 && position.x > 0 ? picBuffer.pic[pixOffset-inputW-1] : 0.f;
	// pix[1] = position.y > 0 ? picBuffer.pic[pixOffset-inputW] : 0.f;
	// pix[2] = position.y > 0 && position.x < inputW-1 ? picBuffer.pic[pixOffset-inputW+1] : 0.f;
	// pix[3] = position.x > 0 ? picBuffer.pic[pixOffset-1] : 0.f;
	// pix[4] = picBuffer.pic[pixOffset];
	// pix[5] = position.x < inputW-1 ? picBuffer.pic[pixOffset+1] : 0.f;
	// pix[6] = position.y < inputH-1 && position.x > 0 ? picBuffer.pic[pixOffset+inputW-1] : 0.f;
	// pix[7] = position.y < inputH-1 ? picBuffer.pic[pixOffset+inputW] : 0.f;
	// pix[8] = position.y < inputH-1 && position.x < inputW-1 ? picBuffer.pic[pixOffset+inputW+1] : 0.f;
	// upleft
	boolret = XBY(position.y, 0)*XBY(position.x, 0);
	for (int k = 0; k < inputChannel; k++)
		pix[0] += boolret*picBuffer.pic[pixOffset + k * inputW * inputH-inputW-1];
	// up
	boolret = XBY(position.y, 0);
	for (int k = 0; k < inputChannel; k++)
		pix[1] += boolret*picBuffer.pic[pixOffset + k * inputW * inputH-inputW];
	// upright
	boolret = XBY(position.y, 0)*XLEY(position.x, inputW-2);
	for (int k = 0; k < inputChannel; k++)
		pix[2] += boolret*picBuffer.pic[pixOffset + k * inputW * inputH-inputW+1];
	// left
	boolret = XBY(position.x, 0);
	for (int k = 0; k < inputChannel; k++)
		pix[3] += boolret*picBuffer.pic[pixOffset + k * inputW * inputH-1];
	// center
	for (int k = 0; k < inputChannel; k++)
		pix[4] += picBuffer.pic[pixOffset + k * inputW * inputH];
	// right
	boolret = XLEY(position.x, inputW-2);
	for (int k = 0; k < inputChannel; k++)
		pix[5] += boolret*picBuffer.pic[pixOffset + k * inputW * inputH+1];
	// bottomleft
	boolret = XLEY(position.y, inputH-2)*XBY(position.x, 0);
	for (int k = 0; k < inputChannel; k++)
		pix[6] += boolret*picBuffer.pic[pixOffset + k * inputW * inputH+inputW-1];
	// bottom
	boolret = XLEY(position.y, inputH-2);
	for (int k = 0; k < inputChannel; k++)
		pix[7] += boolret*picBuffer.pic[pixOffset + k * inputW * inputH+inputW];
	// bottomright
	boolret = XLEY(position.y, inputH-2)*XLEY(position.x, inputW-2);
	for (int k = 0; k < inputChannel; k++)
		pix[8] += boolret*picBuffer.pic[pixOffset + k * inputW * inputH+inputW+1];

	float result = 0.f;
	for (int k = 0; k < 9; k++)
	{
		// tmpBuffer.tmp[k] = pix[k];
	    result += convBuffer.conv[convCoreOffset+k]*pix[k];
	}
	// tmpBuffer.tmp[0] = picBuffer.pic[pixOffset];
	// tmpBuffer.tmp[1] = picBuffer.pic[0];

	outBuffer.data[pixOffset] = tanh(result + convBuffer.conv[convCoreOffset+9]);
}