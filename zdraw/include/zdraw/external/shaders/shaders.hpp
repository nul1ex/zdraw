#ifndef SHADERS_HPP
#define SHADERS_HPP

namespace shaders {

    constexpr const char* vertex_shader_src = R"(
        cbuffer ProjectionBuffer : register(b0)
        {
            float4x4 projection;
        };

        struct VS_INPUT
        {
            float2 pos : POSITION;
            float2 uv  : TEXCOORD0;
            float4 col : COLOR0;
        };

        struct PS_INPUT
        {
            float4 pos : SV_POSITION;
            float2 uv  : TEXCOORD0;
            float4 col : COLOR0;
        };

        PS_INPUT main(VS_INPUT input)
        {
            PS_INPUT output;
            output.pos = mul(projection, float4(input.pos, 0.0f, 1.0f));
            output.uv  = input.uv;
            output.col = input.col;
            return output;
        }
    )";

    constexpr const char* pixel_shader_src = R"(
        Texture2D tex     : register(t0);
        SamplerState samp : register(s0);

        struct PS_INPUT
        {
            float4 pos : SV_POSITION;
            float2 uv  : TEXCOORD0;
            float4 col : COLOR0;
        };

        float4 main(PS_INPUT input) : SV_Target
        {
            float4 texColor = tex.Sample(samp, input.uv);
            return texColor * input.col;
        }
    )";

} // namespace shaders

#endif // SHADERS_HPP