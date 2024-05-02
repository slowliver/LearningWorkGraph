RWByteAddressBuffer Output : register(u0);

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1, 1, 1)]
void BroadcastNode()
{
	Output.Store3(0, uint3(0x6C6C6548, 0x6F57206F, 0x00646C72));
}

void VSMain()
{
}