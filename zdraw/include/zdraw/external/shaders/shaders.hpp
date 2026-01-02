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

	constexpr const char* zscene_vertex_shader_src = R"(
		cbuffer TransformBuffer : register(b0)
		{
			float4x4 world;
			float4x4 view;
			float4x4 projection;
		};

		cbuffer BoneBuffer : register(b1)
		{
			float4x4 bones[600];
		};

		struct VS_INPUT
		{
			float3 position : POSITION;
			float3 normal : NORMAL;
			float2 uv : TEXCOORD;
			uint4 bone_indices : BLENDINDICES;
			float4 bone_weights : BLENDWEIGHT;
		};

		struct VS_OUTPUT
		{
			float4 position : SV_POSITION;
			float3 normal : NORMAL;
			float2 uv : TEXCOORD0;
			float3 world_pos : TEXCOORD1;
		};

		VS_OUTPUT main(VS_INPUT input)
		{
			VS_OUTPUT output;
    
			float4 skinned_pos = float4(0, 0, 0, 0);
			float3 skinned_normal = float3(0, 0, 0);
    
			float total_weight = input.bone_weights.x + input.bone_weights.y + input.bone_weights.z + input.bone_weights.w;
    
			if (total_weight > 0.0001f)
			{
				for (int i = 0; i < 4; ++i)
				{
					float weight = 0;
					uint idx = 0;
            
					if (i == 0) { weight = input.bone_weights.x; idx = input.bone_indices.x; }
					else if (i == 1) { weight = input.bone_weights.y; idx = input.bone_indices.y; }
					else if (i == 2) { weight = input.bone_weights.z; idx = input.bone_indices.z; }
					else { weight = input.bone_weights.w; idx = input.bone_indices.w; }
            
					if (weight > 0.0001f)
					{
						float4x4 bone_mat = bones[idx];
						skinned_pos += mul(float4(input.position, 1.0f), bone_mat) * weight;
						skinned_normal += mul(input.normal, (float3x3)bone_mat) * weight;
					}
				}
			}
			else
			{
				skinned_pos = float4(input.position, 1.0f);
				skinned_normal = input.normal;
			}
    
			float4 world_pos = mul(skinned_pos, world);
			output.world_pos = world_pos.xyz;
			output.position = mul(mul(world_pos, view), projection);
			output.normal = normalize(mul(skinned_normal, (float3x3)world));
			output.uv = input.uv;
    
			return output;
		}
	)";

	constexpr const char* zscene_pixel_shader_src = R"(
        Texture2D albedo_texture : register(t0);
        SamplerState samp : register(s0);

        struct PS_INPUT
        {
            float4 position : SV_POSITION;
            float3 normal : NORMAL;
            float2 uv : TEXCOORD0;
            float3 world_pos : TEXCOORD1;
        };

        float4 main(PS_INPUT input) : SV_TARGET
        {
            float4 albedo = albedo_texture.Sample(samp, input.uv);

            float3 light_dir = normalize(float3(0.5f, 1.0f, 0.3f));
            float3 normal = normalize(input.normal);

            float ndotl = max(dot(normal, light_dir), 0.0f);
            float3 diffuse = albedo.rgb * (ndotl * 0.7f + 0.3f);

            return float4(diffuse, albedo.a);
        }
    )";

} // namespace shaders

#endif // SHADERS_HPP