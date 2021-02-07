
matrix worldMatrix;
matrix viewMatrix : register(b0);
matrix projectionMatrix;
Texture2D TextureSourceY;
Texture2D TextureSourceU;
Texture2D TextureSourceV;
Texture2D TextureSourceA;
cbuffer PS_COLOR_TRANSFORM
{
	float4x4 WhitePoint;
	float4x4 Colorspace;
	float4x4 TransPrimaries;
};
int PixType;
int transfer;
int distransfer;
int primaries;
int disprimaries;
int fullrange;
int srcrange;
float LuminanceScale;
int DrawLine;


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
	output.Pos = mul(inPos, viewMatrix);
	output.TexCoord = inTexCoord;
	return output;
}

inline float4 PSPlanar420_Reverse(VS_OUTPUT vert_in)
{
	float y = TextureSourceY.Sample(SamplerDiffuse, vert_in.TexCoord).r;
	float u = TextureSourceU.Sample(SamplerDiffuse, vert_in.TexCoord).r;
	float v = TextureSourceV.Sample(SamplerDiffuse, vert_in.TexCoord).r;
	return float4(y,u,v,1.0);
}

inline float4 PSPlanar42010_Reverse(VS_OUTPUT vert_in)
{
	float y = TextureSourceY.Sample(SamplerDiffuse, vert_in.TexCoord).r * 64;
	float u = TextureSourceU.Sample(SamplerDiffuse, vert_in.TexCoord).r * 64;
	float v = TextureSourceV.Sample(SamplerDiffuse, vert_in.TexCoord).r * 64;
	return float4(y, u, v, 1.0);
}

// hlg 电光转换函数
inline float inverse_HLG(float x) 
{
		const float B67_a = 0.17883277; 
		const float B67_b = 0.28466892; 
		const float B67_c = 0.55991073; 
		const float B67_inv_r2 = 4.0; 
		if (x <= 0.5)
			x = x * x * B67_inv_r2; 
		else
			x = exp((x - B67_c) / B67_a) + B67_b; 
			return x; 
}

// hlg 光电转换函数
inline float LineToHLG(float Lc)
{
	const double a = 0.17883277;
	const double b = 0.28466892;
	const double c = 0.55991073;
	return (0.0 > Lc) ? 0.0 :
		(Lc <= 1.0 / 12.0 ? sqrt(3.0 * Lc) : a * log(12.0 * Lc - b) + c);
}

// pq 电光转换函数  
float4 ST2084TOLinear(float4 rgb)
{
const float ST2084_m1 = 2610.0 / (4096.0 * 4);
const float ST2084_m2 = (2523.0 / 4096.0) * 128.0;
const float ST2084_c1 = 3424.0 / 4096.0;
const float ST2084_c2 = (2413.0 / 4096.0) * 32.0;
const float ST2084_c3 = (2392.0 / 4096.0) * 32.0;
rgb = pow(max(rgb, 0), 1.0/ST2084_m2);
rgb = max(rgb - ST2084_c1, 0.0) / (ST2084_c2 - ST2084_c3 * rgb);
rgb = pow(rgb, 1.0/ST2084_m1);
return rgb*10000;
}

// HLG 电光转换函数 （这里进行了光光转换）
float4 HLGTOLinear(float4 rgb)
{
	const float alpha_gain = 1000; 
		rgb.r = inverse_HLG(rgb.r);
		rgb.g = inverse_HLG(rgb.g);
		rgb.b = inverse_HLG(rgb.b);
		float3 ootf_2020 = float3(0.2627, 0.6780, 0.0593);
		float ootf_ys = alpha_gain * dot(ootf_2020, rgb.rgb);
		return rgb * pow(ootf_ys, 0.200);
}



float4 BT709TOLinear(float4 rgb)
{
	return pow(rgb, 1.0 / 0.45);
}

float4 BT470M_SRGB_TOLinear(float4 rgb)
{
	return pow(rgb, 2.2);
}
float4 BT470BGTOLinear(float4 rgb)
{
	return pow(rgb, 2.8);
}

float4 LineTOSRGB(float4 rgb)
{
	return pow(rgb, 1.0 / 2.2);
}

// pq 光电转换函数
float4 LineTOST2084(float4 rgb)
{
	const float ST2084_m1 = 2610.0 / (4096.0 * 4);
const float ST2084_m2 = (2523.0 / 4096.0) * 128.0;
const float ST2084_c1 = 3424.0 / 4096.0;
const float ST2084_c2 = (2413.0 / 4096.0) * 32.0;
const float ST2084_c3 = (2392.0 / 4096.0) * 32.0;
rgb = pow(rgb / 10000, ST2084_m1);
rgb = (ST2084_c1 + ST2084_c2 * rgb) / (1 + ST2084_c3 * rgb);
rgb = pow(rgb, ST2084_m2);
return rgb;
}

inline float4 hable(float4 x)
{
		const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30; 
		return ((x * (A*x + (C*B)) + (D*E)) / (x * (A*x + B) + (D*F))) - E / F;
}

float4 HDRToneMapping(float4 rgb)
{
	float4 HABLE_DIV = hable(11.2);
	float4 rgba = hable(rgb* LuminanceScale) / HABLE_DIV;
	return rgba;
}

float4 sourceToLinear(float4 rgb)
{
	if (transfer == distransfer)
	{
		return rgb;
	}
	else if (transfer == 8)  //line 
	{
		return rgb;
	}
	else if (transfer == 16) // pq
	{
		return ST2084TOLinear(rgb);
	}
	else if (transfer == 18) // hlg
	{
		return HLGTOLinear(rgb);
	}
	else if (transfer == 1) // bt709
	{
		return BT709TOLinear(rgb);
	}
	else if (transfer == 4)
	{
		return BT470M_SRGB_TOLinear(rgb);
	}
	else if (transfer == 5)
	{
		return BT470BGTOLinear(rgb);
	}
	else
	{
		return rgb;
	}
	
	
}

float4 transformPrimaries(float4 rgb)
{
	if (primaries != disprimaries)
	{
		return max(mul(rgb, TransPrimaries), 0);
	}
	else
	{
		return rgb;
	}
}

float4 toneMapping(float4 rgb)
{
	if (distransfer == transfer)
	{
		return rgb;
	}
	if (distransfer == 1 || distransfer == 4)
	{
		if (transfer == 16 || transfer == 18)
		{
			return HDRToneMapping(rgb);
		}
		else
		{
			return rgb * LuminanceScale;
		}
	}
	else
	{
		return rgb * LuminanceScale;
	}

}

float4 adjustRange(float4 rgb)
{
	return rgb;
}

float4 linearToDisplay(float4 rgb)
{
	if (distransfer == transfer)
	{
		return  rgb;
	}
	else if (distransfer == 16)
	{
		return LineTOST2084(rgb);
	}
	else if (distransfer == 18) // DX11 无hlg显示模式
	{
		return rgb;
	}
	else if (distransfer == 1)
	{
		return pow(rgb, 1.0 / 2.2);
	}
	else if (distransfer == 4)
	{
		return pow(rgb, 1.0 / 2.2);
	}
	else
	{
		return rgb;
	}
}


float4 reorderPlanes(float4 rgb)
{
	return rgb;
}

float4 RenderFloat(float4 rgb)
{ 
	float a = rgb.a;
	if (DrawLine == 1)
	{
		// 只进行颜色空间转换
	}
	else
	{
		rgb = sourceToLinear(rgb);
		rgb = transformPrimaries(rgb);
		rgb = toneMapping(rgb);
		rgb = linearToDisplay(rgb);
		rgb = adjustRange(rgb);
		rgb = reorderPlanes(rgb);
	}
	return float4(rgb.rgb, a); 
}
inline float4 PSPacked422_Reverse(VS_OUTPUT input)
{
	return float4(0, 1, 0, 0);
}

inline float4 GetRGBA(VS_OUTPUT input)
{
	float4x4 yuv2rgbmatrix = Colorspace;
	if (6 == PixType)
	{
		float4 rgba = PSPacked422_Reverse(input);
		float4 rr = mul(rgba, WhitePoint);
		float4 rgbarsp = max(mul(rr, yuv2rgbmatrix), 0);
		rgbarsp = RenderFloat(rgbarsp);
		rgbarsp.a = 1;
		return rgbarsp;
	}
	else if (5 == PixType)
	{
		float4 rgba = PSPacked422_Reverse(input);
		float4 rr = mul(rgba, WhitePoint);
		float4 rgbarsp = max(mul(rr, yuv2rgbmatrix), 0);
		rgbarsp = RenderFloat(rgbarsp);
		rgbarsp.a = 1;
		return rgbarsp;
	}
	else if (3 == PixType)
	{
		float4 rgba = PSPacked422_Reverse(input);
		float4 rr = mul(rgba, WhitePoint);
		float4 rgbarsp = max(mul(rr, yuv2rgbmatrix), 0);
			rgbarsp = RenderFloat(rgbarsp);
			rgbarsp.a = 1;
		return rgbarsp;
	}
	else if (2 == PixType)
	{
		float4 rgba = PSPlanar420_Reverse(input);
		float4 rr = mul(rgba, WhitePoint);
		float4 rgbarsp = max(mul(rr, yuv2rgbmatrix), 0);
		float	rgbarsp1 = RenderFloat(rgbarsp);
		rgbarsp.a = 1;
		return rgbarsp;
	}
	else if (1 == PixType) // bgra
	{
		float4 rgba = TextureSourceY.Sample(SamplerDiffuse, input.TexCoord);
		rgba.a = 0;
		rgba = RenderFloat(rgba);
		rgba.a = 1;
		return rgba;
	}
	else if (4 == PixType) // bgra
	{
		float4 rgba = TextureSourceY.Sample(SamplerDiffuse, input.TexCoord);
		rgba = RenderFloat(rgba);
		return rgba;
	}
	else if (7 == PixType)
	{
		float4 rgba = TextureSourceY.Sample(SamplerDiffuse, input.TexCoord);
		rgba = RenderFloat(rgba);
		return rgba;
	}
	else if (8 == PixType)
	{
		float4 rgba = PSPlanar42010_Reverse(input);
		float4 rgbasub = rgba;// -bt709yuv;
		float4 rr = mul(rgba, WhitePoint);
		float4 rgbarsp = max(mul(rr, yuv2rgbmatrix), 0);
		rgbarsp = RenderFloat(rgbarsp);
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
