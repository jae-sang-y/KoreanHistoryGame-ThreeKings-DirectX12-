#include "Common.hlsl"


[maxvertexcount(3)]
void GS(
	triangle VertexOut input[3],
	inout TriangleStream< GSOutput > output
)
{
	for (uint i = 0; i < 3; i++)
	{
		GSOutput element;
		element = input[i];
		element.PosH.y += sin(element.PosH.x);
		output.Append(element);
	}
}