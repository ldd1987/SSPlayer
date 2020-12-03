
matrix worldMatrix;
matrix viewMatrix : register(b0);
matrix projectionMatrix;
Texture2D TextureSourceY;
Texture2D TextureSourceU;
Texture2D TextureSourceV;
Texture2D TextureSourceA;
int PixType;
int sourcewidth;
int sourceheight;
SamplerState SamplerDiffuse
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	ComparisonFunc = ALWAYS;
};


struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD;
};

VS_OUTPUT VS(float4 inPos : POSITION, float2 inTexCoord : TEXCOORD)
{
	VS_OUTPUT output;
	inPos.w = 1.0f;

	// Calculate the position of the vertex against the world, view, and projection matrices.
//	output.Pos = mul(inPos, worldMatrix);
	output.Pos = mul(inPos, viewMatrix);
//	output.Pos = mul(output.Pos, projectionMatrix);
	//output.Pos = inPos;
	// Store the texture coordinates for the pixel shader.
	output.TexCoord = inTexCoord;
	return output;
}

inline float4 PSPacked422_Reverse(VS_OUTPUT vert_in, uint u_pos, uint v_pos, int y0_pos, uint y1_pos)
{
	return float4(0, 1, 0, 1.0);
}

inline float4 PSPlanar420_Reverse(VS_OUTPUT vert_in)
{
	float y = TextureSourceY.Sample(SamplerDiffuse, vert_in.TexCoord).r;
	float2 pos = float2(vert_in.TexCoord.x, vert_in.TexCoord.y);
	float u = TextureSourceU.Sample(SamplerDiffuse,pos).r;
	float v = TextureSourceV.Sample(SamplerDiffuse, pos).r;

	return float4(y,u,v,1.0);
}

inline float4 PSPlanar42010_Reverse(VS_OUTPUT vert_in)
{
	float2 pos = float2(vert_in.TexCoord.x, vert_in.TexCoord.y);
	float y = TextureSourceY.Sample(SamplerDiffuse, pos).r * 64;
	
	float u = TextureSourceU.Sample(SamplerDiffuse, pos).r * 64;
	float v = TextureSourceV.Sample(SamplerDiffuse, pos).r * 64;

	return float4(y, u, v, 1.0);
}

inline float4 PSPlanarRGB_Reverse(VS_OUTPUT vert_in)
{
	return float4(0, 1, 0, 1.0);
}

float4 bt709yuv = float4(0.0625f, 0.5f, 0.5f, 0.0f);

float4x4 bt709matrix =
{
	1.164, 1.164, 1.164, 0,
	0, -0.213, 2.112, 0,
	1.793, -0.533, 0, 0,
	0, 0, 0, 0
};

inline float4 GetRGBA(VS_OUTPUT input)
{
	if (6 == PixType)
	{
		float4 rgba = PSPacked422_Reverse(input, 3, 1, 2, 0);
		float4 rgbasub = rgba - bt709yuv;
		float4 rgbarsp = mul(rgbasub, bt709matrix);
		rgbarsp.a = 1;
		return rgbarsp;
	}
	else if (5 == PixType)
	{
		float4 rgba = PSPacked422_Reverse(input, 2, 0, 1, 3);
			float4 rgbasub = rgba - bt709yuv;
			float4 rgbarsp = mul(rgbasub, bt709matrix);
			rgbarsp.a = 1;
		return rgbarsp;
	}
	else if (3 == PixType)
	{
		float4 rgba = PSPacked422_Reverse(input, 1, 3, 2, 0);
			float4 rgbasub = rgba - bt709yuv;
			float4 rgbarsp = mul(rgbasub, bt709matrix);
			rgbarsp.a = 1;
		return rgbarsp;
	}
	else if (2 == PixType)
	{
		float4 rgba = PSPlanar420_Reverse(input);
			float4 rgbasub = rgba - bt709yuv;
			float4 rgbarsp = mul(rgbasub, bt709matrix);
			rgbarsp.a = 1;
		return rgbarsp;
	}
	else if (1 == PixType) // bgra
	{
		float4 rgba = TextureSourceY.Sample(SamplerDiffuse, input.TexCoord);
		return rgba;
	}
	else if (4 == PixType) // bgra
	{
		float4 rgba = PSPlanarRGB_Reverse(input);
		return rgba;
	}
	else if (7 == PixType)
	{
		float4 rgba = TextureSourceY.Sample(SamplerDiffuse, input.TexCoord);
		return rgba;
	}
	else if (8 == PixType)
	{
		float4 rgba = PSPlanar42010_Reverse(input);
		float4 rgbasub = rgba - bt709yuv;
		float4 rgbarsp = mul(rgbasub, bt709matrix);
		rgbarsp.a = 1;
		return rgbarsp;
	}
	else
	{
		return float4(0, 1, 0, 1);
	}
}
float4 PS(VS_OUTPUT input) : SV_TARGET
{
		float4 rgba = GetRGBA(input);
		return rgba;
}


technique11 BasicTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetGeometryShader( NULL );
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
}
