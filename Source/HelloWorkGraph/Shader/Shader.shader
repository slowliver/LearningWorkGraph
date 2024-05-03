RWByteAddressBuffer Output : register(u0);

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1, 1, 1)]
void BroadcastNode()
{
	Output.Store3(0, uint3(0x6C6C6548, 0x6F57206F, 0x00646C72));
}

struct VSInput
{
	float2 position : POSITION;
};

struct VSOutputToPSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

struct SceneParameters
{
	float4x4 viewProjectionMatrix;
};
ConstantBuffer<SceneParameters> sceneParameters : register(b0);

VSOutputToPSInput VSMain(VSInput i)
{
	VSOutputToPSInput o = (VSOutputToPSInput)0;
	
	float3 position = float3(i.position, 1.0f);
	position = mul(position, (float3x3)sceneParameters.viewProjectionMatrix);

	o.position = float4(position.xy, 0.0f, 1.0f);
	o.color = float4(i.position, 0.0f, 1.0f);
	
	return o;
}

float4 PSMain(VSOutputToPSInput i) : SV_TARGET
{
	return float4(i.color);
}
