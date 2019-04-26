
matrix worldMatrix;
matrix viewMatrix : register(b0);
matrix projectionMatrix;
Texture2D TextureSource;
Texture2D TextureCG;
int PixType;
int BlendType = 0;
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

inline float GetIntOffsetColor(uint offset)
{
	return TextureSource.Load(int3(offset % sourcewidth,
		offset / sourcewidth,
		0)).r;
}
#define PRECISION_OFFSET 0.2
inline float4 PSPacked422_Reverse(VS_OUTPUT vert_in, uint u_pos, uint v_pos, int y0_pos, uint y1_pos)
{
	float y = vert_in.TexCoord.y;
	float odd = floor(fmod(sourcewidth * vert_in.TexCoord.x + PRECISION_OFFSET, 2.0));
	float x = floor(sourcewidth / 2 * vert_in.TexCoord.x + PRECISION_OFFSET) * (1.0 / (sourcewidth / 2)); // width_d2_i;
	x += (1.0 / sourcewidth) / 2;
	float4 texel = TextureSource.Sample(SamplerDiffuse, float2(x, y));
	return float4(odd > 0.5 ? texel[y1_pos] : texel[y0_pos],texel[u_pos], texel[v_pos], 1.0);
}

inline float4 PSPlanar420_Reverse(VS_OUTPUT vert_in)
{
	uint x = uint(vert_in.TexCoord.x * sourcewidth);
	uint y = uint(vert_in.TexCoord.y * sourceheight);

	uint lum_offset = y * sourcewidth + x;
	uint chroma_offset = (y / 2) * (sourcewidth / 2) + x / 2;
	uint chroma1 = sourcewidth * sourceheight + chroma_offset;
	uint chroma2 = sourcewidth *sourceheight*0.25 + chroma1;

	return float4(
		GetIntOffsetColor(lum_offset),
		GetIntOffsetColor(chroma1),
		GetIntOffsetColor(chroma2),
		1.0
		);
}

inline float4 PSPlanarRGB_Reverse(VS_OUTPUT vert_in)
{
	uint x = uint(vert_in.TexCoord.x * sourcewidth);
	uint y = uint(vert_in.TexCoord.y * sourceheight);

	uint lum_offset = (y * sourcewidth + x) * 3;
	uint chroma1 = lum_offset + 1;
	uint chroma2 = lum_offset + 2;
	uint nWidth = sourcewidth * 3;
	float r = TextureSource.Load(int3(lum_offset % nWidth, lum_offset / nWidth, 0)).r;
	float g = TextureSource.Load(int3(chroma1 % nWidth, chroma1 / nWidth, 0)).r;
	float b = TextureSource.Load(int3(chroma2 % nWidth, chroma2 / nWidth, 0)).r;
	return float4(r, g, b, 1.0);
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
		float4 rgba = TextureSource.Sample(SamplerDiffuse, input.TexCoord);
		return rgba;
	}
	else if (4 == PixType) // bgra
	{
		float4 rgba = PSPlanarRGB_Reverse(input);
		return rgba;
	}
	else if (7 == PixType)
	{
		float4 rgba = TextureSource.Sample(SamplerDiffuse, input.TexCoord);
		return rgba;
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
