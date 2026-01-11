#pragma once

namespace zscene {

	using Microsoft::WRL::ComPtr;

	struct camera
	{
		XMFLOAT3 position{ 0.0f, 1.0f, 3.0f };
		XMFLOAT3 target{ 0.0f, 1.0f, 0.0f };
		XMFLOAT3 up{ 0.0f, 1.0f, 0.0f };

		float fov{ XM_PIDIV4 };
		float near_z{ 0.1f };
		float far_z{ 100.0f };

		[[nodiscard]] XMMATRIX view_matrix( ) const
		{
			return XMMatrixLookAtLH
			(
				XMLoadFloat3( &position ),
				XMLoadFloat3( &target ),
				XMLoadFloat3( &up )
			);
		}

		[[nodiscard]] XMMATRIX projection_matrix( float aspect ) const
		{
			return XMMatrixPerspectiveFovLH( fov, aspect, near_z, far_z );
		}
	};

	class renderer
	{
	public:
		bool initialize( ID3D11Device* device, ID3D11DeviceContext* context );
		void shutdown( );

		void render( const model& mdl, const XMMATRIX& world, const camera& cam, float aspect );

	private:
		bool create_shaders( );
		bool create_states( );
		bool create_buffers( );

		ID3D11Device* m_device{ nullptr };
		ID3D11DeviceContext* m_context{ nullptr };

		ComPtr<ID3D11VertexShader> m_vertex_shader{};
		ComPtr<ID3D11PixelShader> m_pixel_shader{};
		ComPtr<ID3D11InputLayout> m_input_layout{};

		ComPtr<ID3D11RasterizerState> m_rasterizer_state{};
		ComPtr<ID3D11DepthStencilState> m_depth_state{};
		ComPtr<ID3D11BlendState> m_blend_state{};
		ComPtr<ID3D11SamplerState> m_sampler_state{};

		ComPtr<ID3D11Buffer> m_transform_cb{};
		ComPtr<ID3D11Buffer> m_bones_cb{};

		ComPtr<ID3D11Texture2D> m_white_texture{};
		ComPtr<ID3D11ShaderResourceView> m_white_srv{};
	};

} // namespace zscene