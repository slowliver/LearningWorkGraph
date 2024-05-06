struct ApplicationConstantBuffer
{
	uint numSortElements;
	uint3 dummy0;
	uint4 dummy1[15];
};
struct PassConstantBuffer
{
	uint inc;
	uint dir;
	uint2 dummy;
};

ConstantBuffer<ApplicationConstantBuffer> applicationConstantBuffer : register(b0);
ConstantBuffer<PassConstantBuffer> passConstantBuffer : register(b1);

//ByteAddressBuffer input : register(t0);
globallycoherent  RWByteAddressBuffer Output : register(u0);

struct MyRecord
{
	uint numSortElements;
};

struct MyRecord2
{
	uint dispatchGrid : SV_DispatchGrid;
	MyRecord myRecord;
	uint inc;
	uint dir;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1, 1, 1)]
void LaunchWorkGraph
(
	DispatchNodeInputRecord<MyRecord> inputData,
#if 1
	[MaxRecords(1)] NodeOutput<MyRecord2> SecondNode
#endif
)
{
	const uint log2n = log2(inputData.Get().numSortElements);
	uint inc = 0;
#if 1
	// Main-block.
	for (uint i = 0; i <log2n; ++i)
	{
		inc = 1u << i;
		// Sub-block.
		//for (uint j = 0; j < i + 1; ++j)
		for (uint j = 0; j < 1; ++j)
		{
			const bool isFirstStep = (i == 0 && j == 0);

			if (!isFirstStep)
			{
				Barrier(Output, DEVICE_SCOPE | GROUP_SYNC);
			}

			ThreadNodeOutputRecords<MyRecord2> record = SecondNode.GetThreadNodeOutputRecords(1);
			record.Get().dispatchGrid = max(1, inputData.Get().numSortElements / 2 / 1024);
			record.Get().myRecord = inputData.Get();
			record.Get().inc = inc;
			record.Get().dir = 2u << i;
			record.OutputComplete();

			inc /= 2;
		}
	}
#endif
#if 0
	// Main-block.
	for (uint i = 0; i <inputData.Get().numSortElements; ++i)
	{
		Output.Store(i * 4, i);
	}
#endif
//	Output.Store3(0, uint3(0x6C6C6548, 0x6F57206F, 0x00646C72));
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(65535, 1, 1)]
[NumThreads(1024, 1, 1)]
void SecondNode
(
	uint dispatchThreadID : SV_DispatchThreadID,
	DispatchNodeInputRecord<MyRecord2> inputData
)
{
	if (dispatchThreadID >= inputData.Get().myRecord.numSortElements / 2)
	{
		return;
	}

	// https://www.bealto.com/gpu-sorting_parallel-bitonic-1.html	
	const uint mask = (inputData.Get().inc - 1);
	const uint low = mask & dispatchThreadID; // low order bits (below INC)
	const uint i = (dispatchThreadID * 2) - low; // insert 0 at position INC

	// Load
	const uint a = Output.Load(i * 4);
	const uint b = Output.Load((i + inputData.Get().inc) * 4);

	// Sort & Store
	{
		const bool reverse = ((inputData.Get().dir & i) == 0); // asc/desc order
		const bool swap = reverse ? (a >= b) : (a < b);
		if (swap)
		{
			// Store
			Output.Store(i * 4, b);
			Output.Store((i + inputData.Get().inc) * 4, a);
		}
	}
}

[numthreads(1024, 1, 1)]
void CSMain(uint dispatchThreadID : SV_DispatchThreadID, uint groupID : SV_GroupID)
{
	if (dispatchThreadID >= applicationConstantBuffer.numSortElements / 2)
	{
		return;
	}

	// https://www.bealto.com/gpu-sorting_parallel-bitonic-1.html	
	const uint mask = (passConstantBuffer.inc - 1);
	const uint low = mask & dispatchThreadID; // low order bits (below INC)
	const uint i = (dispatchThreadID * 2) - low; // insert 0 at position INC

	// Load
	const uint a = Output.Load(i * 4);
	const uint b = Output.Load((i + passConstantBuffer.inc) * 4);

	// Sort & Store
	{
		const bool reverse = ((passConstantBuffer.dir & i) == 0); // asc/desc order
		const bool swap = reverse ? (a >= b) : (a < b);
		if (swap)
		{
			// Store
			Output.Store(i * 4, b);
			Output.Store((i + passConstantBuffer.inc) * 4, a);
		}
	}
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
