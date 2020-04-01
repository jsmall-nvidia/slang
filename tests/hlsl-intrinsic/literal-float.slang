//TEST(compute):COMPARE_COMPUTE_EX:-cpu -compute  -output-using-type
//TEST(compute):COMPARE_COMPUTE_EX:-slang -compute -output-using-type
//TEST(compute):COMPARE_COMPUTE_EX:-slang -compute -dx12 -output-using-type
//TEST(compute, vulkan):COMPARE_COMPUTE_EX:-vk -compute -output-using-type
//TEST(compute):COMPARE_COMPUTE_EX:-cuda -compute -output-using-type

//TEST_INPUT:ubuffer(data=[0 0 0 0], stride=4):out,name outputBuffer
RWStructuredBuffer<float> outputBuffer;

[numthreads(4, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int idx = int(dispatchThreadID.x);
    
    float v = 0.0f;

    if (idx == 0)
    {
        v = 5e+8;
    }
    else if (idx == 1)
    {
        v = -5e-7;
    }
    else if (idx == 2)
    {
        v = 3.14159265358979323846264338328e-5;
    }
    else if (idx == 3)
    {
        v = 14159265358979323846264338328.4979879732634534543234234e+9;
    }
    
    outputBuffer[idx] = v;
}