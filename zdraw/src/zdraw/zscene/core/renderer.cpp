#include <include/global.hpp>
#include <include/zdraw/external/shaders/shaders.hpp>

namespace zscene {

	struct transform_cb_data
	{
		XMMATRIX world;
		XMMATRIX view;
		XMMATRIX projection;
	};

	struct bones_cb_data
	{
		XMMATRIX bones[ k_max_bones ];
	};

	bool renderer::initialize( ID3D11Device* device, ID3D11DeviceContext* context )
	{
		this->m_device = device;
		this->m_context = context;

		if (
			!this->create_shaders( ) ||
			!this->create_states( ) ||
			!this->create_buffers( )
			)
		{
			return false;
		}

		return true;
	}

	void renderer::shutdown( )
	{
		this->m_vertex_shader.Reset( );
		this->m_pixel_shader.Reset( );
		this->m_input_layout.Reset( );
		this->m_rasterizer_state.Reset( );
		this->m_depth_state.Reset( );
		this->m_blend_state.Reset( );
		this->m_sampler_state.Reset( );
		this->m_transform_cb.Reset( );
		this->m_bones_cb.Reset( );
		this->m_white_texture.Reset( );
		this->m_white_srv.Reset( );
	}

	bool renderer::create_shaders( )
	{
		ComPtr<ID3DBlob> vs_blob{};
		ComPtr<ID3DBlob> error_blob{};

		auto hr = D3DCompile( shaders::zscene_vertex_shader_src, std::strlen( shaders::zscene_vertex_shader_src ), nullptr, nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs_blob, &error_blob );
		if ( FAILED( hr ) )
		{
			return false;
		}

		hr = this->m_device->CreateVertexShader( vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ), nullptr, &this->m_vertex_shader );
		if ( FAILED( hr ) )
		{
			return false;
		}

		D3D11_INPUT_ELEMENT_DESC layout[ ] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof( skinned_vertex, position ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof( skinned_vertex, normal ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof( skinned_vertex, uv ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, offsetof( skinned_vertex, bone_indices ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof( skinned_vertex, bone_weights ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		hr = this->m_device->CreateInputLayout( layout, _countof( layout ), vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ), &this->m_input_layout );
		if ( FAILED( hr ) )
		{
			return false;
		}

		ComPtr<ID3DBlob> ps_blob{};
		error_blob.Reset( );

		hr = D3DCompile( shaders::zscene_pixel_shader_src, std::strlen( shaders::zscene_pixel_shader_src ), nullptr, nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps_blob, &error_blob );
		if ( FAILED( hr ) )
		{
			return false;
		}

		hr = this->m_device->CreatePixelShader( ps_blob->GetBufferPointer( ), ps_blob->GetBufferSize( ), nullptr, &this->m_pixel_shader );
		return SUCCEEDED( hr );
	}

	bool renderer::create_states( )
	{
		D3D11_RASTERIZER_DESC raster_desc{};
		raster_desc.FillMode = D3D11_FILL_SOLID;
		raster_desc.CullMode = D3D11_CULL_BACK;
		raster_desc.FrontCounterClockwise = FALSE;
		raster_desc.DepthClipEnable = TRUE;

		if ( FAILED( this->m_device->CreateRasterizerState( &raster_desc, &this->m_rasterizer_state ) ) )
		{
			return false;
		}

		D3D11_DEPTH_STENCIL_DESC depth_desc{};
		depth_desc.DepthEnable = TRUE;
		depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depth_desc.DepthFunc = D3D11_COMPARISON_LESS;

		if ( FAILED( this->m_device->CreateDepthStencilState( &depth_desc, &this->m_depth_state ) ) )
		{
			return false;
		}

		D3D11_BLEND_DESC blend_desc{};
		blend_desc.RenderTarget[ 0 ].BlendEnable = FALSE;
		blend_desc.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		if ( FAILED( this->m_device->CreateBlendState( &blend_desc, &m_blend_state ) ) )
		{
			return false;
		}

		D3D11_SAMPLER_DESC sampler_desc{};
		sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
		sampler_desc.MaxAnisotropy = 16;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampler_desc.MinLOD = 0;
		sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

		return SUCCEEDED( this->m_device->CreateSamplerState( &sampler_desc, &this->m_sampler_state ) );
	}

	bool renderer::create_buffers( )
	{
		D3D11_BUFFER_DESC cb_desc{};
		cb_desc.ByteWidth = sizeof( transform_cb_data );
		cb_desc.Usage = D3D11_USAGE_DYNAMIC;
		cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		auto hr = m_device->CreateBuffer( &cb_desc, nullptr, &m_transform_cb );
		if ( FAILED( hr ) )
		{
			return false;
		}

		cb_desc.ByteWidth = sizeof( bones_cb_data );
		hr = m_device->CreateBuffer( &cb_desc, nullptr, &m_bones_cb );
		if ( FAILED( hr ) )
		{
			return false;
		}

		D3D11_TEXTURE2D_DESC tex_desc{};
		tex_desc.Width = 1;
		tex_desc.Height = 1;
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		std::uint32_t white_pixel{ 0xFFFFFFFF };

		D3D11_SUBRESOURCE_DATA init_data{};
		init_data.pSysMem = &white_pixel;
		init_data.SysMemPitch = 4;

		if ( FAILED( this->m_device->CreateTexture2D( &tex_desc, &init_data, &this->m_white_texture ) ) )
		{
			return false;
		}

		return SUCCEEDED( this->m_device->CreateShaderResourceView( this->m_white_texture.Get( ), nullptr, &this->m_white_srv ) );
	}

	void renderer::render( const model& mdl, const XMMATRIX& world, const camera& cam, float aspect )
	{
		{
			D3D11_MAPPED_SUBRESOURCE mapped{};
			if ( SUCCEEDED( this->m_context->Map( this->m_transform_cb.Get( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) ) )
			{
				auto* data = static_cast< transform_cb_data* >( mapped.pData );
				data->world = XMMatrixTranspose( world );
				data->view = XMMatrixTranspose( cam.view_matrix( ) );
				data->projection = XMMatrixTranspose( cam.projection_matrix( aspect ) );
				this->m_context->Unmap( this->m_transform_cb.Get( ), 0 );
			}
		}

		{
			D3D11_MAPPED_SUBRESOURCE mapped{};
			if ( SUCCEEDED( this->m_context->Map( this->m_bones_cb.Get( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) ) )
			{
				auto* data = static_cast< bones_cb_data* >( mapped.pData );

				for ( std::size_t i = 0; i < k_max_bones; ++i )
				{
					if ( i < mdl.bone_matrices.size( ) )
					{
						data->bones[ i ] = XMMatrixTranspose( mdl.bone_matrices[ i ] );
					}
					else
					{
						data->bones[ i ] = XMMatrixIdentity( );
					}
				}

				this->m_context->Unmap( this->m_bones_cb.Get( ), 0 );
			}
		}

		this->m_context->IASetInputLayout( this->m_input_layout.Get( ) );
		this->m_context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

		this->m_context->VSSetShader( this->m_vertex_shader.Get( ), nullptr, 0 );
		this->m_context->VSSetConstantBuffers( 0, 1, this->m_transform_cb.GetAddressOf( ) );
		this->m_context->VSSetConstantBuffers( 1, 1, this->m_bones_cb.GetAddressOf( ) );

		this->m_context->PSSetShader( this->m_pixel_shader.Get( ), nullptr, 0 );
		this->m_context->PSSetSamplers( 0, 1, this->m_sampler_state.GetAddressOf( ) );

		this->m_context->RSSetState( this->m_rasterizer_state.Get( ) );
		this->m_context->OMSetDepthStencilState( this->m_depth_state.Get( ), 0 );

		const float blend_factor[ 4 ]{ 0.0f, 0.0f, 0.0f, 0.0f };
		this->m_context->OMSetBlendState( this->m_blend_state.Get( ), blend_factor, 0xFFFFFFFF );

		for ( const auto& m : mdl.meshes )
		{
			if ( !m.vertex_buffer || !m.index_buffer )
			{
				continue;
			}

			auto srv = this->m_white_srv.Get( );

			if ( m.material_index >= 0 && m.material_index < static_cast< int >( mdl.materials.size( ) ) )
			{
				if ( mdl.materials[ m.material_index ].albedo_texture )
				{
					srv = mdl.materials[ m.material_index ].albedo_texture.Get( );
				}
			}

			this->m_context->PSSetShaderResources( 0, 1, &srv );

			UINT stride{ sizeof( skinned_vertex ) };
			UINT offset{ 0 };

			this->m_context->IASetVertexBuffers( 0, 1, m.vertex_buffer.GetAddressOf( ), &stride, &offset );
			this->m_context->IASetIndexBuffer( m.index_buffer.Get( ), DXGI_FORMAT_R32_UINT, 0 );
			this->m_context->DrawIndexed( static_cast< UINT >( m.indices.size( ) ), 0, 0 );
		}
	}

} // namespace zscene