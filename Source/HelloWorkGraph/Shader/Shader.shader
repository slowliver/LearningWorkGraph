globallycoherent  RWByteAddressBuffer output : register(u0);

struct ApplicationRecord
{
	uint numSortElements;
};

struct PassRecord
{
	uint dispatchGrid : SV_DispatchGrid;
	ApplicationRecord applicationRecord;
	uint inc;
	uint dir;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1, 1, 1)]
void LaunchWorkGraph
(
	DispatchNodeInputRecord<ApplicationRecord> applicationRecord,
	[MaxRecords(1)] NodeOutput<PassRecord> SecondNode
)
{
const uint log2n = log2(applicationRecord.Get().numSortElements);
	uint inc = 0;

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
				Barrier(output, DEVICE_SCOPE | GROUP_SYNC);
			}

			ThreadNodeOutputRecords<PassRecord> record = SecondNode.GetThreadNodeOutputRecords(1);
			record.Get().dispatchGrid = max(1, applicationRecord.Get().numSortElements / 2 / 1024);
			record.Get().applicationRecord = applicationRecord.Get();
			record.Get().inc = inc;
			record.Get().dir = 2u << i;
			record.OutputComplete();

			inc /= 2;
		}
	}
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(65535, 1, 1)]
[NumThreads(1024, 1, 1)]
void SecondNode
(
	uint dispatchThreadID : SV_DispatchThreadID,
	DispatchNodeInputRecord<PassRecord> passRecord
)
{
	if (dispatchThreadID >= passRecord.Get().applicationRecord.numSortElements / 2)
	{
		return;
	}

	// https://www.bealto.com/gpu-sorting_parallel-bitonic-1.html	
	const uint mask = (passRecord.Get().inc - 1);
	const uint low = mask & dispatchThreadID; // low order bits (below INC)
	const uint i = (dispatchThreadID * 2) - low; // insert 0 at position INC

	// Load
	const uint a = output.Load(i * 4);
	const uint b = output.Load((i + passRecord.Get().inc) * 4);

	// Sort & Store
	{
		const bool reverse = ((passRecord.Get().dir & i) == 0); // asc/desc order
		const bool swap = reverse ? (a >= b) : (a < b);
		if (swap)
		{
			// Store
			output.Store(i * 4, b);
			output.Store((i + passRecord.Get().inc) * 4, a);
		}
	}
}

struct ApplicationConstantBuffer
{
	uint numSortElements;
	uint3 dummy0;
	uint4 dummy1[15];
};
ConstantBuffer<ApplicationConstantBuffer> applicationConstantBuffer : register(b0);

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
