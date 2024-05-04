ByteAddressBuffer input : register(t0);
RWByteAddressBuffer Output : register(u0);

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1, 1, 1)]
void BroadcastNode()
{
	Output.Store3(0, uint3(0x6C6C6548, 0x6F57206F, 0x00646C72));
}

[numthreads(1024, 1, 1)]
void CSMain(uint dispatchThreadID : SV_DispatchThreadID)
{
	uint value = input.Load(dispatchThreadID * 4);
	Output.Store(dispatchThreadID * 4, value);
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








/// ----------------------------------------------------------------------------------------------

#if 0
struct D3D12_DRAW_INDEXED_ARGUMENTS
{
	uint IndexCountPerInstance;
	uint InstanceCount;
	uint StartIndexLocation;
	int BaseVertexLocation;
	uint StartInstanceLocation;
};

struct D3D12_INDEX_BUFFER
{
	uint64_t Address;
	uint SizeInBytes;
};

struct MyDrawIndexedData
{
	float3 perDrawConstant;

	D3D12_DRAW_INDEXED_ARGUMENTS drawArgs : SV_DrawIndexedArgs;
	uint vbTable : SV_VertexBufferTable;
	D3D12_INDEX_BUFFER indexBuffer : SV_IndexBuffer;
};

// -------------------------------------------------------------------------------------------------
// Simple compute shader for the compute node.  Runs a single thread, which unconditionally fills
// out a MyDrawIndexedData record representing the draw to launch.
//
[Shader("node")]
[NodeID("MyComputeNode")]
[NodeLaunch("broadcasting")]
[NodeIsProgramEntry]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1, 1, 1)]
void MyComputeShader(
  [MaxRecords(1)][NodeID("MyDrawIndexedNode")]
  NodeOutput<MyDrawIndexedData> nodeOutput)
{
  GroupNodeOutputRecords<MyDrawIndexedData> record = GetGroupNodeOutputRecords(nodeOutput, 1);

#if 0
  // In the future `Get().` below will have `->` as an alternative.
  record.Get().drawArgs.IndexCountPerInstance = /* Omitted for brevity. */;
  record.Get().drawArgs.InstanceCount = /* Omitted for brevity. */;
  record.Get().drawArgs.StartIndexLocation = /* Omitted for brevity. */;
  record.Get().drawArgs.BaseVertexLocation = /* Omitted for brevity. */;
  record.Get().drawArgs.StartInstanceLocation = /* Omitted for brevity. */;

  record.Get().vbTable.GeometryDescriptorHeapOffset = /* Omitted for brevity. */;

  record.Get().indexBuffer.Address = /* Omitted for brevity. */;
  record.Get().indexBuffer.SizeInBytes = /* Omitted for brevity. */;

  record.Get().perDrawConstant = float3(/* Omitted for brevity. */);
#endif

  record.OutputComplete();
}


struct MyVSInput
{
	float3 position : POSITION;
	float2 texCoord : TEXCOORD0;
};

struct MyPSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD0;
};

// -------------------------------------------------------------------------------------------------
// Simple vertex shader for the draw-indexed node.
//
// The [NodeID()] attribute is optional on the entry shader for a program at a graphics node,
// vertex or mesh shader.  In its absence, when the node is defined at the API it would have
// to specify a NodeID, or can override the name specified here.
//
// The [NodeIsProgramEntry] attribute is optional here, and its presence means that the draw indexed
// node could be launched directly from DispatchNodes(), without going through one or more compute 
// nodes.
//
// Note that there is no special interaction with the index or vertex buffer "handles" contained in
// the input record.  The handles are part of the input record, but there is nothing to do with them
// because the normal Input Assembler pipeline stage still functions for this node.
//
[Shader("node")]
[NodeID("material", 7)]
[NodeLaunch("drawindexed")]
MyPSInput MyVertexShader(
  DispatchNodeInputRecord< MyDrawIndexedData> nodeInput,
  in MyVSInput vsInput)
{
	MyPSInput psInput;
  // In the future `Get().` below will have `->` as an alternative.
	psInput.position = float4(vsInput.position + nodeInput.Get().perDrawConstant, 1.0f);
	psInput.texCoord = vsInput.texCoord;

	return psInput;
}
#endif
