//------------------------------------------------------------------------------
// <copyright file="DepthWithColor-D3D.fx" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

Texture2D<int>    txDepth  : register(t0);
Texture2D<float4> txColor  : register(t1);
SamplerState      samColor : register(s0);

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
cbuffer cbChangesEveryFrame : register(b0)
{
    matrix  View;
    matrix  Projection;
    float4  XYScale;
	float4  rect;
};


struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Col : COLOR;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS(float4 Pos : POSITION)
{
	PS_INPUT output;
	output.Pos = Pos;
	output.Col = float4(1.0,0.1,0.5, 1.0);
	return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	return float4(input.Col.rgb, 1.0);
}