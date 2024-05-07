struct ApplicationConstantBuffer
{
	uint numSortElements;
	uint3 dummy0;
	uint4 dummy1[15];
};
ConstantBuffer<ApplicationConstantBuffer> applicationConstantBuffer : register(b0);

globallycoherent  RWByteAddressBuffer output : register(u0);

#if defined(WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID) && WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID
#	define ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID 1
#else
#	define ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID 0
#endif

#if ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID
struct LaunchRecord
{
	uint dispatchGrid : SV_DispatchGrid;
};
#endif

struct PassRecord
{
#if !ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID
	uint dispatchGrid : SV_DispatchGrid;
#else
	uint index;
#endif
	uint inc;
	uint dir;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
#if ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID
[NodeMaxDispatchGrid(65535, 1, 1)]
[NumThreads(1024, 1, 1)]
#else
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1, 1, 1)]
#endif
void LaunchWorkGraph
(
#if ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID
	uint dispatchThreadID : SV_DispatchThreadID,
	DispatchNodeInputRecord<LaunchRecord> launchRecord,
#endif
	[MaxRecords(1)] NodeOutput<PassRecord> SecondNode
)
{
#if ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID
	if (dispatchThreadID >= applicationConstantBuffer.numSortElements / 2)
	{
		return;
	}
#endif

	const uint log2n = log2(applicationConstantBuffer.numSortElements);
	uint inc = 0;

	// Main-block.
	for (uint i = 0; i <log2n; ++i)
	{
		inc = 1u << i;
		// Sub-block.
		for (uint j = 0; j < i + 1; ++j)
		{
			const bool isFirstStep = (i == 0 && j == 0);

			if (!isFirstStep)
			{
				Barrier(output, DEVICE_SCOPE | GROUP_SYNC);
			}

			ThreadNodeOutputRecords<PassRecord> record = SecondNode.GetThreadNodeOutputRecords(1);
#if !ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID
			record.Get().dispatchGrid = max(1, applicationConstantBuffer.numSortElements / 2 / 1024);
#else
			record.Get().index = dispatchThreadID;
#endif
			record.Get().inc =inc;
			record.Get().dir = 2u << i;
			record.OutputComplete();

			inc /= 2;
		}
	}
}

[Shader("node")]
#if ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID
[NodeLaunch("thread")]
#else
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(65535, 1, 1)]
[NumThreads(1024, 1, 1)]
#endif
void SecondNode
(
#if ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID
	ThreadNodeInputRecord<PassRecord> passRecord
#else
	uint dispatchThreadID : SV_DispatchThreadID,
	DispatchNodeInputRecord<PassRecord> passRecord
#endif
)
{
	const uint inc = passRecord.Get().inc;
	const uint dir = passRecord.Get().dir;
#if ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID
	const uint index = passRecord.Get().index;
#else
	const uint index = dispatchThreadID;
#endif
#if !ENABLE_WORK_GRAPH_LAUNCHED_MULTI_DISPATCH_GRID
	if (dispatchThreadID >= applicationConstantBuffer.numSortElements / 2)
	{
		return;
	}
#endif

	// https://www.bealto.com/gpu-sorting_parallel-bitonic-1.html	
	const uint mask = (inc - 1);
	const uint low = mask & index; // low order bits (below INC)
	const uint i = (index * 2) - low; // insert 0 at position INC

	// Load
	const uint a = output.Load(i * 4);
	const uint b = output.Load((i + inc) * 4);

	// Sort & Store
	{
		const bool reverse = ((dir & i) == 0); // asc/desc order
		const bool swap = reverse ? (a >= b) : (a < b);
		if (swap)
		{
			// Store
			output.Store(i * 4, b);
			output.Store((i + inc) * 4, a);
		}
	}
}

struct PassConstantBuffer
{
	uint inc;
	uint dir;
	uint2 dummy;
};
ConstantBuffer<PassConstantBuffer> passConstantBuffer : register(b1);

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
	const uint a = output.Load(i * 4);
	const uint b = output.Load((i + passConstantBuffer.inc) * 4);

	// Sort & Store
	{
		const bool reverse = ((passConstantBuffer.dir & i) == 0); // asc/desc order
		const bool swap = reverse ? (a >= b) : (a < b);
		if (swap)
		{
			// Store
			output.Store(i * 4, b);
			output.Store((i + passConstantBuffer.inc) * 4, a);
		}
	}
}
