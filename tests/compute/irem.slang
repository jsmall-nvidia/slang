// irem.slang

//TEST(compute):COMPARE_COMPUTE:-d3d11
//TEST(compute):COMPARE_COMPUTE:-cpu
//TEST(compute):COMPARE_COMPUTE:-vk
//TEST(compute):COMPARE_COMPUTE:-d3d12 -use-dxil
//TEST(compute):COMPARE_COMPUTE:-d3d12 

RWStructuredBuffer<int> gInput;
//TEST_INPUT:ubuffer(data=[2 1 5 2 2 -4 -5 2], stride=4):dxbinding(0),glbinding(0),name=gInput

int test(int idx)
{
    int a = gInput[idx * 2];
    int b = gInput[idx * 2 + 1];
    return a % b;
}

//TEST_INPUT:ubuffer(data=[0 0 0 0], stride=4):dxbinding(1),glbinding(1),out,name=outputBuffer
RWStructuredBuffer<int> outputBuffer;

[numthreads(4, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	int idx = int(dispatchThreadID.x);
	outputBuffer[idx] = test(idx);
}
