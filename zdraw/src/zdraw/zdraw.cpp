#include <include/zdraw/zdraw.hpp>

#include <algorithm>
#include <fstream>
#include <numbers>
#include <cstring>
#include <cmath>

#include <d3dcompiler.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <include/zdraw/external/stb/truetype.hpp>
#include <include/zdraw/external/stb/image.hpp>
#include <include/zdraw/external/fonts/pixter.hpp>
#include <include/zdraw/external/shaders/shaders.hpp>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace zdraw {

	namespace detail {

		using Microsoft::WRL::ComPtr;

		struct persistent_buffer
		{
			ComPtr<ID3D11Buffer> m_buffer{};
			void* m_mapped_data{ nullptr };
			std::uint32_t m_size{ 0 };
			std::uint32_t m_write_offset{ 0 };
			std::uint32_t m_capacity{ 0 };
			bool m_is_mapped{ false };

			bool create( ID3D11Device* device, std::uint32_t initial_capacity, D3D11_BIND_FLAG bind_flags )
			{
				this->m_capacity = initial_capacity;

				D3D11_BUFFER_DESC desc{};
				desc.ByteWidth = this->m_capacity;
				desc.Usage = D3D11_USAGE_DYNAMIC;
				desc.BindFlags = bind_flags;
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

				return SUCCEEDED( device->CreateBuffer( &desc, nullptr, &this->m_buffer ) );
			}

			bool map_discard( ID3D11DeviceContext* context )
			{
				if ( this->m_is_mapped )
				{
					this->unmap( context );
				}

				D3D11_MAPPED_SUBRESOURCE mapped{};
				auto hr{ context->Map( this->m_buffer.Get( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) };

				if ( SUCCEEDED( hr ) )
				{
					this->m_mapped_data = mapped.pData;
					this->m_is_mapped = true;
					this->m_write_offset = 0;
					return true;
				}
				else
				{
					return false;
				}
			}

			void unmap( ID3D11DeviceContext* context ) noexcept
			{
				if ( this->m_is_mapped )
				{
					context->Unmap( this->m_buffer.Get( ), 0 );
					this->m_mapped_data = nullptr;
					this->m_is_mapped = false;
				}
			}

			[[nodiscard]] void* allocate( std::uint32_t bytes )
			{
				if ( !this->m_is_mapped )
				{
					return nullptr;
				}

				if ( this->m_write_offset + bytes > this->m_capacity ) [[unlikely]]
				{
					return nullptr;
				}

				auto result{ static_cast< char* >( this->m_mapped_data ) + this->m_write_offset };
				this->m_write_offset += bytes;
				return result;
			}

			void reset_offsets( ) noexcept
			{
				this->m_write_offset = 0;
			}

			[[nodiscard]] bool needs_resize( std::uint32_t required_size ) const noexcept
			{
				return required_size > this->m_capacity;
			}

			void resize( ID3D11Device* device, ID3D11DeviceContext* context, std::uint32_t new_capacity, D3D11_BIND_FLAG bind_flags )
			{
				this->unmap( context );
				this->m_buffer.Reset( );
				this->m_capacity = new_capacity;
				this->create( device, this->m_capacity, bind_flags );
				this->m_write_offset = 0;
			}
		};

		struct render_state_cache
		{
			ID3D11ShaderResourceView* m_last_texture{ nullptr };
			bool m_state_dirty{ true };

			void reset_frame( ) noexcept
			{
				this->m_last_texture = nullptr;
				this->m_state_dirty = true;
			}

			[[nodiscard]] bool needs_texture_bind( ID3D11ShaderResourceView* new_tex ) const noexcept
			{
				return this->m_last_texture != new_tex;
			}

			void set_texture( ID3D11ShaderResourceView* tex ) noexcept
			{
				this->m_last_texture = tex;
			}
		};

		struct render_data
		{
			ComPtr<ID3D11Device> m_device{};
			ComPtr<ID3D11DeviceContext> m_context{};

			persistent_buffer m_vertex_buffer{};
			persistent_buffer m_index_buffer{};

			ComPtr<ID3D11Buffer> m_constant_buffer{};
			ComPtr<ID3D11VertexShader> m_vertex_shader{};
			ComPtr<ID3D11PixelShader> m_pixel_shader{};
			ComPtr<ID3D11InputLayout> m_input_layout{};
			ComPtr<ID3D11RasterizerState> m_rasterizer_state{};
			ComPtr<ID3D11BlendState> m_blend_state{};
			ComPtr<ID3D11DepthStencilState> m_depth_stencil_state{};
			ComPtr<ID3D11SamplerState> m_sampler_state{};
			ComPtr<ID3D11Texture2D> m_white_texture{};
			ComPtr<ID3D11ShaderResourceView> m_white_texture_srv{};

			static constexpr std::uint32_t k_initial_vertex_capacity{ 65536u * static_cast< std::uint32_t >( sizeof( vertex ) ) };
			static constexpr std::uint32_t k_initial_index_capacity{ 131072u * static_cast< std::uint32_t >( sizeof( std::uint32_t ) ) };
			static constexpr std::uint32_t k_max_vertex_capacity{ 1048576u * static_cast< std::uint32_t >( sizeof( vertex ) ) };
			static constexpr std::uint32_t k_max_index_capacity{ 2097152u * static_cast< std::uint32_t >( sizeof( std::uint32_t ) ) };

			draw_list m_current_draw_list{};
			render_state_cache m_state_cache{};

			std::vector<std::unique_ptr<font>> m_loaded_fonts{};
			font* m_normal_font{ nullptr };

			std::uint32_t m_frame_vertex_count{ 0 };
			std::uint32_t m_frame_index_count{ 0 };
			std::uint32_t m_buffer_resize_count{ 0 };
		};

		struct constant_buffer_data
		{
			float m_projection[ 4 ][ 4 ];
		};

		static render_data g_render{};

		[[nodiscard]] static bool create_shaders( )
		{
			ComPtr<ID3DBlob> vs_blob{};
			ComPtr<ID3DBlob> error_blob{};

			auto hr{ D3DCompile( shaders::vertex_shader_src, std::strlen( shaders::vertex_shader_src ), nullptr, nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &vs_blob, &error_blob ) };
			if ( FAILED( hr ) ) [[unlikely]]
			{
				return false;
			}

			hr = g_render.m_device->CreateVertexShader( vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ), nullptr, &g_render.m_vertex_shader );
			if ( FAILED( hr ) ) [[unlikely]]
			{
				return false;
			}

			constexpr D3D11_INPUT_ELEMENT_DESC layout[ ]
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof( vertex, m_pos ),D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof( vertex, m_uv ),D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof( vertex, m_col ),D3D11_INPUT_PER_VERTEX_DATA, 0},
			};

			hr = g_render.m_device->CreateInputLayout( layout, 3, vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ), &g_render.m_input_layout );
			if ( FAILED( hr ) ) [[unlikely]]
			{
				return false;
			}

			ComPtr<ID3DBlob> ps_blob{};
			hr = D3DCompile( shaders::pixel_shader_src, std::strlen( shaders::pixel_shader_src ), nullptr, nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &ps_blob, &error_blob );
			if ( FAILED( hr ) ) [[unlikely]]
			{
				return false;
			}

			hr = g_render.m_device->CreatePixelShader( ps_blob->GetBufferPointer( ), ps_blob->GetBufferSize( ), nullptr, &g_render.m_pixel_shader );
			return SUCCEEDED( hr );
		}

		[[nodiscard]] static bool create_render_states( )
		{
			D3D11_RASTERIZER_DESC raster_desc{};
			raster_desc.FillMode = D3D11_FILL_SOLID;
			raster_desc.CullMode = D3D11_CULL_NONE;
			raster_desc.ScissorEnable = FALSE;
			raster_desc.DepthClipEnable = FALSE;
			raster_desc.MultisampleEnable = FALSE;
			raster_desc.AntialiasedLineEnable = FALSE;

			if ( FAILED( g_render.m_device->CreateRasterizerState( &raster_desc, &g_render.m_rasterizer_state ) ) ) [[unlikely]]
			{
				return false;
			}

			D3D11_BLEND_DESC blend_desc{};
			blend_desc.RenderTarget[ 0 ].BlendEnable = TRUE;
			blend_desc.RenderTarget[ 0 ].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blend_desc.RenderTarget[ 0 ].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blend_desc.RenderTarget[ 0 ].BlendOp = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[ 0 ].SrcBlendAlpha = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[ 0 ].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			blend_desc.RenderTarget[ 0 ].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			if ( FAILED( g_render.m_device->CreateBlendState( &blend_desc, &g_render.m_blend_state ) ) ) [[unlikely]]
			{
				return false;
			}

			D3D11_DEPTH_STENCIL_DESC depth_desc{};
			depth_desc.DepthEnable = FALSE;

			if ( FAILED( g_render.m_device->CreateDepthStencilState( &depth_desc, &g_render.m_depth_stencil_state ) ) ) [[unlikely]]
			{
				return false;
			}

			D3D11_SAMPLER_DESC sampler_desc{};
			sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

			return SUCCEEDED( g_render.m_device->CreateSamplerState( &sampler_desc, &g_render.m_sampler_state ) );
		}

		[[nodiscard]] static bool create_white_texture( )
		{
			D3D11_TEXTURE2D_DESC tex_desc{};
			tex_desc.Width = 1;
			tex_desc.Height = 1;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			constexpr std::uint32_t white_pixel{ 0xFFFFFFFFu };
			D3D11_SUBRESOURCE_DATA init_data{ &white_pixel, 4u, 0u };

			auto hr{ g_render.m_device->CreateTexture2D( &tex_desc, &init_data, &g_render.m_white_texture ) };
			if ( FAILED( hr ) ) [[unlikely]]
			{
				return false;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Format = tex_desc.Format;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;

			hr = g_render.m_device->CreateShaderResourceView( g_render.m_white_texture.Get( ), &srv_desc, &g_render.m_white_texture_srv );
			return SUCCEEDED( hr );
		}

		[[nodiscard]] static bool create_constant_buffer( )
		{
			D3D11_BUFFER_DESC desc{};
			desc.ByteWidth = static_cast< UINT >( sizeof( constant_buffer_data ) );
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			return SUCCEEDED( g_render.m_device->CreateBuffer( &desc, nullptr, &g_render.m_constant_buffer ) );
		}

		[[nodiscard]] static bool create_persistent_buffers( )
		{
			return g_render.m_vertex_buffer.create( g_render.m_device.Get( ), g_render.k_initial_vertex_capacity, D3D11_BIND_VERTEX_BUFFER ) && g_render.m_index_buffer.create( g_render.m_device.Get( ), g_render.k_initial_index_capacity, D3D11_BIND_INDEX_BUFFER );
		}

		static void ensure_buffer_capacity( )
		{
			auto& dl{ g_render.m_current_draw_list };

			const std::uint32_t required_vertex_bytes{ static_cast< std::uint32_t >( dl.m_vertices.size( ) ) * static_cast< std::uint32_t >( sizeof( vertex ) ) };
			const std::uint32_t required_index_bytes{ static_cast< std::uint32_t >( dl.m_indices.size( ) ) * static_cast< std::uint32_t >( sizeof( std::uint32_t ) ) };

			if ( g_render.m_vertex_buffer.needs_resize( required_vertex_bytes ) )
			{
				std::uint32_t new_capacity{ std::max( g_render.m_vertex_buffer.m_capacity * 2u, required_vertex_bytes ) };
				new_capacity = std::min( new_capacity, g_render.k_max_vertex_capacity );
				g_render.m_vertex_buffer.resize( g_render.m_device.Get( ), g_render.m_context.Get( ), new_capacity, D3D11_BIND_VERTEX_BUFFER );
				g_render.m_buffer_resize_count += 1u;
			}

			if ( g_render.m_index_buffer.needs_resize( required_index_bytes ) )
			{
				std::uint32_t new_capacity{ std::max( g_render.m_index_buffer.m_capacity * 2u, required_index_bytes ) };
				new_capacity = std::min( new_capacity, g_render.k_max_index_capacity );
				g_render.m_index_buffer.resize( g_render.m_device.Get( ), g_render.m_context.Get( ), new_capacity, D3D11_BIND_INDEX_BUFFER );
				g_render.m_buffer_resize_count += 1u;
			}
		}

		static void setup_projection_matrix( float width, float height )
		{
			D3D11_MAPPED_SUBRESOURCE mapped{};
			if ( SUCCEEDED( g_render.m_context->Map( g_render.m_constant_buffer.Get( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) ) )
			{
				auto* cb{ static_cast< constant_buffer_data* >( mapped.pData ) };

				constexpr auto offset{ -0.5f };
				const auto L{ offset };
				const auto R{ width + offset };
				const auto T{ offset };
				const auto B{ height + offset };

				const float ortho_projection[ 4 ][ 4 ]
				{
					{ 2.0f / ( R - L ),  0.0f, 0.0f, 0.0f },
					{ 0.0f, 2.0f / ( T - B ), 0.0f, 0.0f },
					{ 0.0f, 0.0f, 0.5f, 0.0f },
					{ ( R + L ) / ( L - R ), ( T + B ) / ( B - T ), 0.5f, 1.0f },
				};

				std::memcpy( cb->m_projection, ortho_projection, sizeof( ortho_projection ) );
				g_render.m_context->Unmap( g_render.m_constant_buffer.Get( ), 0 );
			}
		}

		static void setup_render_state( )
		{
			auto& d{ g_render };

			d.m_context->IASetInputLayout( d.m_input_layout.Get( ) );
			d.m_context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

			d.m_context->VSSetShader( d.m_vertex_shader.Get( ), nullptr, 0 );
			d.m_context->VSSetConstantBuffers( 0, 1, d.m_constant_buffer.GetAddressOf( ) );
			d.m_context->PSSetShader( d.m_pixel_shader.Get( ), nullptr, 0 );
			d.m_context->PSSetSamplers( 0, 1, d.m_sampler_state.GetAddressOf( ) );

			d.m_context->RSSetState( d.m_rasterizer_state.Get( ) );

			constexpr float blend_factor[ 4 ]{ 0.0f, 0.0f, 0.0f, 0.0f };
			d.m_context->OMSetBlendState( d.m_blend_state.Get( ), blend_factor, 0xFFFFFFFFu );
			d.m_context->OMSetDepthStencilState( d.m_depth_stencil_state.Get( ), 0 );

			constexpr std::uint32_t stride{ static_cast< std::uint32_t >( sizeof( vertex ) ) };
			constexpr std::uint32_t offset{ 0u };

			d.m_context->IASetVertexBuffers( 0, 1, d.m_vertex_buffer.m_buffer.GetAddressOf( ), &stride, &offset );
			d.m_context->IASetIndexBuffer( d.m_index_buffer.m_buffer.Get( ), DXGI_FORMAT_R32_UINT, 0 );
		}

		static void generate_circle_vertices( float x, float y, float radius, int segments, std::vector<float>& points )
		{
			points.clear( );
			points.reserve( static_cast< std::vector<float>::size_type >( segments ) * 2 );

			const auto angle_increment{ 2.0f * std::numbers::pi_v<float> / static_cast< float >( segments ) };
			for ( int i{ 0 }; i < segments; ++i )
			{
				const auto angle{ angle_increment * static_cast< float >( i ) };
				points.push_back( x + std::cos( angle ) * radius );
				points.push_back( y + std::sin( angle ) * radius );
			}
		}

	} // namespace detail

	void draw_list::ensure_draw_cmd( ID3D11ShaderResourceView* texture )
	{
		auto actual_texture{ texture != nullptr ? texture : detail::g_render.m_white_texture_srv.Get( ) };
		if ( this->m_commands.size( ) == 0 || this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_texture.Get( ) != actual_texture )
		{
			auto cmd{ this->m_commands.allocate( 1 ) };
			*cmd = draw_cmd{};
			cmd->m_texture = actual_texture;
			cmd->m_idx_offset = static_cast< std::uint32_t >( this->m_indices.size( ) );
		}
	}

	void draw_list::add_line( float x0, float y0, float x1, float y1, rgba color, float thickness )
	{
		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };
		const auto dx{ x1 - x0 };
		const auto dy{ y1 - y0 };
		const auto length{ std::sqrt( dx * dx + dy * dy ) };

		if ( length < 0.0001f ) [[unlikely]]
		{
			return;
		}

		const auto norm_dx{ dx / length };
		const auto norm_dy{ dy / length };

		const auto perp_x{ -norm_dy };
		const auto perp_y{ norm_dx };

		const auto half_thickness{ thickness * 0.5f };
		constexpr auto aa_fringe{ 1.0f };
		constexpr auto aa_half{ aa_fringe * 0.5f };

		const auto core_offset{ half_thickness - aa_half };
		const auto core_tx{ perp_x * core_offset };
		const auto core_ty{ perp_y * core_offset };

		const auto outer_offset{ half_thickness + aa_half };
		const auto outer_tx{ perp_x * outer_offset };
		const auto outer_ty{ perp_y * outer_offset };

		auto transparent_color{ color };
		transparent_color.a = 0;

		this->push_vertex( x0 + core_tx, y0 + core_ty, 0.0f, 0.0f, color );
		this->push_vertex( x1 + core_tx, y1 + core_ty, 1.0f, 0.0f, color );
		this->push_vertex( x1 - core_tx, y1 - core_ty, 1.0f, 1.0f, color );
		this->push_vertex( x0 - core_tx, y0 - core_ty, 0.0f, 1.0f, color );

		this->push_vertex( x0 + outer_tx, y0 + outer_ty, 0.0f, 0.0f, transparent_color );
		this->push_vertex( x1 + outer_tx, y1 + outer_ty, 1.0f, 0.0f, transparent_color );
		this->push_vertex( x1 - outer_tx, y1 - outer_ty, 1.0f, 1.0f, transparent_color );
		this->push_vertex( x0 - outer_tx, y0 - outer_ty, 0.0f, 1.0f, transparent_color );

		std::uint32_t* idx{ this->m_indices.allocate( 18 ) };

		idx[ 0 ] = vtx_base + 0; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;
		idx[ 3 ] = vtx_base + 0; idx[ 4 ] = vtx_base + 2; idx[ 5 ] = vtx_base + 3;
		idx[ 6 ] = vtx_base + 0; idx[ 7 ] = vtx_base + 4; idx[ 8 ] = vtx_base + 5;
		idx[ 9 ] = vtx_base + 0; idx[ 10 ] = vtx_base + 5; idx[ 11 ] = vtx_base + 1;
		idx[ 12 ] = vtx_base + 2; idx[ 13 ] = vtx_base + 6; idx[ 14 ] = vtx_base + 7;
		idx[ 15 ] = vtx_base + 2; idx[ 16 ] = vtx_base + 7; idx[ 17 ] = vtx_base + 3;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 18u;
	}

	void draw_list::add_rect( float x, float y, float w, float h, rgba color, float thickness )
	{
		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };
		const auto inner_x{ x + thickness };
		const auto inner_y{ y + thickness };
		const auto inner_w{ w - thickness * 2.0f };
		const auto inner_h{ h - thickness * 2.0f };

		this->push_vertex( x, y, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + h, 0.0f, 0.0f, color );
		this->push_vertex( x, y + h, 0.0f, 0.0f, color );

		this->push_vertex( inner_x, inner_y, 0.0f, 0.0f, color );
		this->push_vertex( inner_x + inner_w, inner_y, 0.0f, 0.0f, color );
		this->push_vertex( inner_x + inner_w, inner_y + inner_h, 0.0f, 0.0f, color );
		this->push_vertex( inner_x, inner_y + inner_h, 0.0f, 0.0f, color );

		std::uint32_t* idx{ this->m_indices.allocate( 24 ) };
		idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 5;
		idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 5; idx[ 5 ] = vtx_base + 4;
		idx[ 6 ] = vtx_base + 1; idx[ 7 ] = vtx_base + 2; idx[ 8 ] = vtx_base + 6;
		idx[ 9 ] = vtx_base + 1; idx[ 10 ] = vtx_base + 6; idx[ 11 ] = vtx_base + 5;
		idx[ 12 ] = vtx_base + 2; idx[ 13 ] = vtx_base + 3; idx[ 14 ] = vtx_base + 7;
		idx[ 15 ] = vtx_base + 2; idx[ 16 ] = vtx_base + 7; idx[ 17 ] = vtx_base + 6;
		idx[ 18 ] = vtx_base + 3; idx[ 19 ] = vtx_base; idx[ 20 ] = vtx_base + 4;
		idx[ 21 ] = vtx_base + 3; idx[ 22 ] = vtx_base + 4; idx[ 23 ] = vtx_base + 7;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 24u;
	}

	void draw_list::add_rect_cornered( float x, float y, float w, float h, rgba color, float corner_length, float thickness )
	{
		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };
		const auto max_corner{ std::min( w, h ) * 0.5f };
		const auto actual_corner_length{ std::min( corner_length, max_corner ) };

		this->push_vertex( x, y, 0.0f, 0.0f, color );
		this->push_vertex( x + actual_corner_length, y, 0.0f, 0.0f, color );
		this->push_vertex( x + actual_corner_length, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + thickness, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + thickness, y + actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x, y + actual_corner_length, 0.0f, 0.0f, color );

		this->push_vertex( x + w - actual_corner_length, y, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w - actual_corner_length, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w - thickness, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x + w - thickness, y + actual_corner_length, 0.0f, 0.0f, color );

		this->push_vertex( x + w - thickness, y + h - actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + h - actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w - thickness, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w - actual_corner_length, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + h, 0.0f, 0.0f, color );
		this->push_vertex( x + w - actual_corner_length, y + h, 0.0f, 0.0f, color );

		this->push_vertex( x, y + h - actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x + thickness, y + h - actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x + thickness, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + actual_corner_length, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + actual_corner_length, y + h, 0.0f, 0.0f, color );
		this->push_vertex( x, y + h, 0.0f, 0.0f, color );

		std::uint32_t* idx{ this->m_indices.allocate( 48 ) };

		for ( int i{ 0 }; i < 8; ++i )
		{
			const auto base{ i * 6 };
			const auto vertex_base{ static_cast< std::uint32_t >( vtx_base + i * 4 ) };

			idx[ base + 0 ] = vertex_base; idx[ base + 1 ] = vertex_base + 1;
			idx[ base + 2 ] = vertex_base + 2;
			idx[ base + 3 ] = vertex_base; idx[ base + 4 ] = vertex_base + 2;
			idx[ base + 5 ] = vertex_base + 3;
		}

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 48u;
	}

	void draw_list::add_rect_filled( float x, float y, float w, float h, rgba color )
	{
		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

		this->push_vertex( x, y, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y, 1.0f, 0.0f, color );
		this->push_vertex( x + w, y + h, 1.0f, 1.0f, color );
		this->push_vertex( x, y + h, 0.0f, 1.0f, color );

		std::uint32_t* idx{ this->m_indices.allocate( 6 ) };
		idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;
		idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 2; idx[ 5 ] = vtx_base + 3;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 6u;
	}

	void draw_list::add_rect_filled_multi_color( float x, float y, float w, float h, rgba color_tl, rgba color_tr, rgba color_br, rgba color_bl )
	{
		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

		this->push_vertex( x, y, 0.0f, 0.0f, color_tl );
		this->push_vertex( x + w, y, 1.0f, 0.0f, color_tr );
		this->push_vertex( x + w, y + h, 1.0f, 1.0f, color_br );
		this->push_vertex( x, y + h, 0.0f, 1.0f, color_bl );

		std::uint32_t* idx{ this->m_indices.allocate( 6 ) };
		idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;
		idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 2; idx[ 5 ] = vtx_base + 3;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 6u;
	}

	void draw_list::add_rect_textured( float x, float y, float w, float h, ID3D11ShaderResourceView* tex, float u0, float v0, float u1, float v1 )
	{
		this->ensure_draw_cmd( tex );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

		this->push_vertex( x, y, u0, v0, 0xFFFFFFFFu );
		this->push_vertex( x + w, y, u1, v0, 0xFFFFFFFFu );
		this->push_vertex( x + w, y + h, u1, v1, 0xFFFFFFFFu );
		this->push_vertex( x, y + h, u0, v1, 0xFFFFFFFFu );

		std::uint32_t* idx{ this->m_indices.allocate( 6 ) };
		idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;
		idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 2; idx[ 5 ] = vtx_base + 3;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 6u;
	}

	void draw_list::add_convex_poly_filled( std::span<const float> points, rgba color )
	{
		const auto num_points{ static_cast< int >( points.size( ) ) / 2 };
		if ( num_points < 3 ) [[unlikely]]
		{
			return;
		}

		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

		for ( std::size_t i{ 0 }; i < static_cast< std::size_t >( num_points ); ++i )
		{
			const auto x{ points[ i * 2 + 0 ] };
			const auto y{ points[ i * 2 + 1 ] };
			this->push_vertex( x, y, 0.5f, 0.5f, color );
		}

		const auto num_triangles{ num_points - 2 };
		std::uint32_t* idx{ this->m_indices.allocate( static_cast< std::size_t >( num_triangles ) * 3u ) };

		for ( int i{ 0 }; i < num_triangles; ++i )
		{
			const auto base_idx{ i * 3 };
			idx[ base_idx + 0 ] = vtx_base;
			idx[ base_idx + 1 ] = vtx_base + static_cast< std::uint32_t >( i + 1 );
			idx[ base_idx + 2 ] = vtx_base + static_cast< std::uint32_t >( i + 2 );
		}

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += static_cast< std::uint32_t >( num_triangles ) * 3u;
	}

	void draw_list::add_polyline( std::span<const float> points, rgba color, bool closed, float thickness )
	{
		const auto num_points{ static_cast< int >( points.size( ) ) / 2 };
		if ( num_points < 2 ) [[unlikely]]
		{
			return;
		}

		this->ensure_draw_cmd( nullptr );

		const auto num_segments{ closed ? num_points : ( num_points - 1 ) };
		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

		constexpr auto aa_fringe{ 1.0f };
		constexpr auto aa_half{ aa_fringe * 0.5f };
		const auto half_thickness{ thickness * 0.5f };
		const auto core_thickness{ half_thickness - aa_half };
		const auto outer_thickness{ half_thickness + aa_half };

		auto transparent_color{ color };
		transparent_color.a = 0;

		std::vector<float> normals_x( static_cast< std::size_t >( num_segments ) );
		std::vector<float> normals_y( static_cast< std::size_t >( num_segments ) );

		for ( int i{ 0 }; i < num_segments; ++i )
		{
			const auto p1_idx{ i };
			const auto p2_idx{ closed ? ( ( i + 1 ) % num_points ) : ( i + 1 ) };
			const auto dx{ points[ static_cast< std::size_t >( p2_idx * 2 + 0 ) ] - points[ static_cast< std::size_t >( p1_idx * 2 + 0 ) ] };
			const auto dy{ points[ static_cast< std::size_t >( p2_idx * 2 + 1 ) ] - points[ static_cast< std::size_t >( p1_idx * 2 + 1 ) ] };

			const auto length{ std::sqrt( dx * dx + dy * dy ) };
			if ( length > 0.0001f )
			{
				normals_x[ static_cast< std::size_t >( i ) ] = -dy / length;
				normals_y[ static_cast< std::size_t >( i ) ] = dx / length;
			}
			else
			{
				normals_x[ static_cast< std::size_t >( i ) ] = 0.0f;
				normals_y[ static_cast< std::size_t >( i ) ] = 0.0f;
			}
		}

		for ( int i{ 0 }; i < num_points; ++i )
		{
			const auto x{ points[ static_cast< std::size_t >( i ) * 2 + 0 ] };
			const auto y{ points[ static_cast< std::size_t >( i ) * 2 + 1 ] };

			auto normal_x{ 0.0f };
			auto normal_y{ 0.0f };

			if ( closed )
			{
				const auto prev_seg{ ( i - 1 + num_segments ) % num_segments };
				const auto curr_seg{ i % num_segments };
				normal_x = ( normals_x[ static_cast< std::size_t >( prev_seg ) ] + normals_x[ static_cast< std::size_t >( curr_seg ) ] ) * 0.5f;
				normal_y = ( normals_y[ static_cast< std::size_t >( prev_seg ) ] + normals_y[ static_cast< std::size_t >( curr_seg ) ] ) * 0.5f;
			}
			else
			{
				if ( i == 0 )
				{
					normal_x = normals_x[ 0 ];
					normal_y = normals_y[ 0 ];
				}
				else if ( i == num_points - 1 )
				{
					normal_x = normals_x[ static_cast< std::size_t >( num_segments - 1 ) ];
					normal_y = normals_y[ static_cast< std::size_t >( num_segments - 1 ) ];
				}
				else
				{
					normal_x = ( normals_x[ static_cast< std::size_t >( i - 1 ) ] + normals_x[ static_cast< std::size_t >( i ) ] ) * 0.5f;
					normal_y = ( normals_y[ static_cast< std::size_t >( i - 1 ) ] + normals_y[ static_cast< std::size_t >( i ) ] ) * 0.5f;
				}
			}

			const auto normal_length{ std::sqrt( normal_x * normal_x + normal_y * normal_y ) };
			if ( normal_length > 0.0001f )
			{
				normal_x /= normal_length;
				normal_y /= normal_length;
			}

			this->push_vertex( x + normal_x * core_thickness, y + normal_y * core_thickness, 0.0f, 0.0f, color );
			this->push_vertex( x - normal_x * core_thickness, y - normal_y * core_thickness, 1.0f, 1.0f, color );

			this->push_vertex( x + normal_x * outer_thickness, y + normal_y * outer_thickness, 0.0f, 0.0f, transparent_color );
			this->push_vertex( x - normal_x * outer_thickness, y - normal_y * outer_thickness, 1.0f, 1.0f, transparent_color );
		}

		std::uint32_t* idx{ this->m_indices.allocate( static_cast< std::size_t >( num_segments ) * 18u ) };

		for ( int i{ 0 }; i < num_segments; ++i )
		{
			const auto next_i{ closed ? ( ( i + 1 ) % num_points ) : ( i + 1 ) };
			const auto base_idx{ i * 18 };

			const auto curr_core_top{ vtx_base + static_cast< std::uint32_t >( i * 4 + 0 ) };
			const auto curr_core_bot{ vtx_base + static_cast< std::uint32_t >( i * 4 + 1 ) };
			const auto curr_outer_top{ vtx_base + static_cast< std::uint32_t >( i * 4 + 2 ) };
			const auto curr_outer_bot{ vtx_base + static_cast< std::uint32_t >( i * 4 + 3 ) };
			const auto next_core_top{ vtx_base + static_cast< std::uint32_t >( next_i * 4 + 0 ) };
			const auto next_core_bot{ vtx_base + static_cast< std::uint32_t >( next_i * 4 + 1 ) };
			const auto next_outer_top{ vtx_base + static_cast< std::uint32_t >( next_i * 4 + 2 ) };
			const auto next_outer_bot{ vtx_base + static_cast< std::uint32_t >( next_i * 4 + 3 ) };

			idx[ base_idx + 0 ] = curr_core_top;
			idx[ base_idx + 1 ] = curr_core_bot;
			idx[ base_idx + 2 ] = next_core_bot;
			idx[ base_idx + 3 ] = curr_core_top;
			idx[ base_idx + 4 ] = next_core_bot;
			idx[ base_idx + 5 ] = next_core_top;
			idx[ base_idx + 6 ] = curr_outer_top;
			idx[ base_idx + 7 ] = curr_core_top;
			idx[ base_idx + 8 ] = next_core_top;
			idx[ base_idx + 9 ] = curr_outer_top;
			idx[ base_idx + 10 ] = next_core_top;
			idx[ base_idx + 11 ] = next_outer_top;
			idx[ base_idx + 12 ] = curr_core_bot;
			idx[ base_idx + 13 ] = curr_outer_bot;
			idx[ base_idx + 14 ] = next_outer_bot;
			idx[ base_idx + 15 ] = curr_core_bot;
			idx[ base_idx + 16 ] = next_outer_bot;
			idx[ base_idx + 17 ] = next_core_bot;
		}

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += static_cast< std::uint32_t >( num_segments ) * 18u;
	}

	void draw_list::add_circle( float x, float y, float radius, rgba color, int segments, float thickness )
	{
		std::vector<float> points{};
		detail::generate_circle_vertices( x, y, radius, segments, points );
		this->add_polyline( points, color, true, thickness );
	}

	void draw_list::add_circle_filled( float x, float y, float radius, rgba color, int segments )
	{
		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };
		const auto idx_count{ static_cast< std::size_t >( segments ) * 3u };
		std::uint32_t* idx_data{ this->m_indices.allocate( idx_count ) };

		this->push_vertex( x, y, 0.5f, 0.5f, color );

		const auto angle_increment{ 2.0f * std::numbers::pi_v<float> / static_cast< float >( segments ) };
		for ( int i{ 0 }; i < segments; ++i )
		{
			const auto angle{ angle_increment * static_cast< float >( i ) };
			const auto px{ x + std::cos( angle ) * radius };
			const auto py{ y + std::sin( angle ) * radius };
			this->push_vertex( px, py, 0.5f, 0.5f, color );
		}

		for ( int i{ 0 }; i < segments; ++i )
		{
			const auto base_idx{ i * 3 };
			const auto next_vtx{ static_cast< std::uint32_t >( ( i + 1 ) % segments ) };

			idx_data[ base_idx + 0 ] = vtx_base;
			idx_data[ base_idx + 1 ] = vtx_base + 1 + static_cast< std::uint32_t >( i );
			idx_data[ base_idx + 2 ] = vtx_base + 1 + next_vtx;
		}

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += static_cast< std::uint32_t >( idx_count );
	}

	void draw_list::add_text( float x, float y, std::string_view text, const font* f, rgba color )
	{
		if ( f == nullptr || f->m_atlas == nullptr || f->m_atlas->m_texture_srv == nullptr ) [[unlikely]]
		{
			return;
		}

		this->ensure_draw_cmd( f->m_atlas->m_texture_srv.Get( ) );

		auto current_x{ x };
		auto current_y{ y + f->m_ascent };

		for ( char c : text )
		{
			if ( c == '\n' )
			{
				current_x = x;
				current_y += f->m_line_height;
				continue;
			}

			if ( c < 32 || c > 126 )
			{
				continue;
			}

			const auto& glyph{ f->get_glyph( c ) };
			if ( !glyph.m_valid )
			{
				continue;
			}

			const auto char_x{ current_x + glyph.m_quad_x0 };
			const auto char_y{ current_y + glyph.m_quad_y0 };
			const auto char_w{ glyph.m_quad_x1 - glyph.m_quad_x0 };
			const auto char_h{ glyph.m_quad_y1 - glyph.m_quad_y0 };

			if ( char_w > 0.0f && char_h > 0.0f )
			{
				const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

				this->push_vertex( char_x, char_y, glyph.m_uv_x0, glyph.m_uv_y0, color );
				this->push_vertex( char_x + char_w, char_y, glyph.m_uv_x1, glyph.m_uv_y0, color );
				this->push_vertex( char_x + char_w, char_y + char_h, glyph.m_uv_x1, glyph.m_uv_y1, color );
				this->push_vertex( char_x, char_y + char_h, glyph.m_uv_x0, glyph.m_uv_y1, color );

				std::uint32_t* idx{ this->m_indices.allocate( 6 ) };
				idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;
				idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 2; idx[ 5 ] = vtx_base + 3;

				this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 6u;
			}

			current_x += glyph.m_advance_x;
		}
	}

	const glyph_cache_entry& font::get_glyph( char c ) const
	{
		const auto it{ this->m_glyph_cache.find( c ) };
		if ( it != this->m_glyph_cache.end( ) )
		{
			return it->second;
		}

		auto& entry{ this->m_glyph_cache[ c ] };

		const auto char_index{ static_cast< int >( c ) - 32 };
		if ( char_index < 0 || char_index >= 95 || !this->m_packed_char_data )
		{
			entry.m_valid = false;
			return entry;
		}

		stbtt_aligned_quad q{};
		float x_after{ 0.0f };
		float y_after{ 0.0f };

		stbtt_GetPackedQuad( this->m_packed_char_data.get( ), this->m_atlas->m_width, this->m_atlas->m_height, char_index, &x_after, &y_after, &q, 0 );

		entry.m_advance_x = x_after;
		entry.m_quad_x0 = q.x0;
		entry.m_quad_y0 = q.y0;
		entry.m_quad_x1 = q.x1;
		entry.m_quad_y1 = q.y1;
		entry.m_uv_x0 = q.s0;
		entry.m_uv_y0 = q.t0;
		entry.m_uv_x1 = q.s1;
		entry.m_uv_y1 = q.t1;
		entry.m_valid = true;

		return entry;
	}

	void font::calc_text_size( std::string_view text, float& width, float& height ) const
	{
		const std::string text_str{ text };
		const auto it{ this->m_text_size_cache.find( text_str ) };
		if ( it != this->m_text_size_cache.end( ) )
		{
			width = it->second.first;
			height = it->second.second;
			return;
		}

		const auto scale{ 1.0f };
		const auto line_height{ this->m_line_height };

		width = 0.0f;
		height = 0.0f;
		auto line_width{ 0.0f };

		for ( char c : text )
		{
			if ( c == '\n' )
			{
				width = std::max( width, line_width );
				height += line_height;
				line_width = 0.0f;
				continue;
			}

			if ( c == '\r' )
			{
				continue;
			}

			if ( c < 32 || c > 126 )
			{
				continue;
			}

			const auto& glyph{ this->get_glyph( c ) };
			if ( glyph.m_valid )
			{
				line_width += glyph.m_advance_x * scale;
			}
		}

		width = std::max( width, line_width );
		if ( line_width > 0.0f || height == 0.0f )
		{
			height += line_height;
		}

		width = std::floor( width + 0.99999f );
		this->m_text_size_cache[ text_str ] = std::make_pair( width, height );
	}

	void font::clear_caches( ) const noexcept
	{
		this->m_glyph_cache.clear( );
		this->m_text_size_cache.clear( );
	}

	bool initialize( ID3D11Device* device, ID3D11DeviceContext* context )
	{
		if ( device == nullptr || context == nullptr ) [[unlikely]]
		{
			return false;
		}

		detail::g_render.m_device = device;
		detail::g_render.m_context = context;

		if (
			!detail::create_shaders( )
			|| !detail::create_render_states( )
			|| !detail::create_white_texture( )
			|| !detail::create_constant_buffer( )
			|| !detail::create_persistent_buffers( )
			)
		{
			return false;
		}

		detail::g_render.m_current_draw_list.reserve( 5000u, 10000u );
		return true;
	}

	void begin_frame( )
	{
		auto& d{ detail::g_render };

		d.m_current_draw_list.clear( );
		d.m_state_cache.reset_frame( );

		d.m_vertex_buffer.reset_offsets( );
		d.m_index_buffer.reset_offsets( );

		d.m_frame_vertex_count = 0u;
		d.m_frame_index_count = 0u;
	}

	void end_frame( )
	{
		auto& d{ detail::g_render };
		auto& dl{ d.m_current_draw_list };

		if ( dl.m_vertices.size( ) == 0 || dl.m_commands.size( ) == 0 ) [[unlikely]]
		{
			return;
		}

		detail::ensure_buffer_capacity( );

		const auto vertex_data_size{ static_cast< std::uint32_t >( dl.m_vertices.size( ) ) * static_cast< std::uint32_t >( sizeof( vertex ) ) };
		const auto index_data_size{ static_cast< std::uint32_t >( dl.m_indices.size( ) ) * static_cast< std::uint32_t >( sizeof( std::uint32_t ) ) };

		if ( !d.m_vertex_buffer.map_discard( d.m_context.Get( ) ) || !d.m_index_buffer.map_discard( d.m_context.Get( ) ) )
		{
			d.m_vertex_buffer.unmap( d.m_context.Get( ) );
			d.m_index_buffer.unmap( d.m_context.Get( ) );
			return;
		}

		auto vertex_dest{ d.m_vertex_buffer.allocate( vertex_data_size ) };
		auto index_dest{ d.m_index_buffer.allocate( index_data_size ) };

		if ( vertex_dest != nullptr && index_dest != nullptr )
		{
			std::memcpy( vertex_dest, dl.m_vertices.data( ), vertex_data_size );
			std::memcpy( index_dest, dl.m_indices.data( ), index_data_size );
		}
		else
		{
			d.m_vertex_buffer.unmap( d.m_context.Get( ) );
			d.m_index_buffer.unmap( d.m_context.Get( ) );
			return;
		}

		d.m_vertex_buffer.unmap( d.m_context.Get( ) );
		d.m_index_buffer.unmap( d.m_context.Get( ) );

		D3D11_VIEWPORT viewport{};
		std::uint32_t num_viewports{ 1u };
		d.m_context->RSGetViewports( &num_viewports, &viewport );
		detail::setup_projection_matrix( viewport.Width, viewport.Height );

		detail::setup_render_state( );

		auto& state_cache{ d.m_state_cache };
		for ( std::size_t i{ 0 }; i < dl.m_commands.size( ); ++i )
		{
			const auto& cmd{ dl.m_commands.data( )[ i ] };
			if ( cmd.m_idx_count == 0u )
			{
				continue;
			}

			if ( state_cache.needs_texture_bind( cmd.m_texture.Get( ) ) )
			{
				d.m_context->PSSetShaderResources( 0, 1, cmd.m_texture.GetAddressOf( ) );
				state_cache.set_texture( cmd.m_texture.Get( ) );
			}

			d.m_context->DrawIndexed( cmd.m_idx_count, cmd.m_idx_offset, 0 );
		}

		d.m_frame_vertex_count = static_cast< std::uint32_t >( dl.m_vertices.size( ) );
		d.m_frame_index_count = static_cast< std::uint32_t >( dl.m_indices.size( ) );
	}

	draw_list& get_draw_list( ) noexcept
	{
		return detail::g_render.m_current_draw_list;
	}

	std::pair<int, int> get_display_size( ) noexcept
	{
		static const std::pair<int, int> size{ GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ) };
		return size;
	}

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> load_texture_from_memory( std::span<const std::byte> data, int* out_width, int* out_height )
	{
		auto width{ 0 };
		auto height{ 0 };
		auto channels{ 0 };

		const auto pixels{ stbi_load_from_memory( reinterpret_cast< const stbi_uc* >( data.data( ) ), static_cast< int >( data.size( ) ), &width, &height, &channels, STBI_rgb_alpha ) };
		if ( !pixels ) [[unlikely]]
		{
			return nullptr;
		}

		if ( out_width != nullptr ) { *out_width = width; }
		if ( out_height != nullptr ) { *out_height = height; }

		D3D11_TEXTURE2D_DESC tex_desc{};
		tex_desc.Width = static_cast< UINT >( width );
		tex_desc.Height = static_cast< UINT >( height );
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA init_data{};
		init_data.pSysMem = pixels;
		init_data.SysMemPitch = static_cast< UINT >( width * 4 );
		init_data.SysMemSlicePitch = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture{};
		auto hr{ detail::g_render.m_device->CreateTexture2D( &tex_desc, &init_data, &texture ) };

		stbi_image_free( const_cast< stbi_uc* >( pixels ) );

		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		srv_desc.Format = tex_desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_srv{};
		hr = detail::g_render.m_device->CreateShaderResourceView( texture.Get( ), &srv_desc, &texture_srv );

		return FAILED( hr ) ? nullptr : texture_srv;
	}

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> load_texture_from_file( std::string_view filepath, int* out_width, int* out_height )
	{
		std::ifstream file{ std::string( filepath ), std::ios::binary | std::ios::ate };
		if ( !file.is_open( ) ) [[unlikely]]
		{
			return nullptr;
		}

		const std::streamsize size{ file.tellg( ) };
		file.seekg( 0, std::ios::beg );

		std::vector<std::byte> buffer( static_cast< std::size_t >( size ) );
		file.read( reinterpret_cast< char* >( buffer.data( ) ), size );

		return load_texture_from_memory( buffer, out_width, out_height );
	}

	font* load_font_from_memory( std::span<const std::byte> font_data, float size_pixels, int atlas_width, int atlas_height )
	{
		auto new_font{ std::make_unique<font>( ) };
		new_font->m_font_size = size_pixels;
		new_font->m_atlas = std::make_shared<font_atlas>( );
		new_font->m_atlas->m_width = atlas_width;
		new_font->m_atlas->m_height = atlas_height;

		std::vector<std::uint8_t> bitmap( static_cast< std::size_t >( atlas_width ) * static_cast< std::size_t >( atlas_height ), 0u );

		new_font->m_packed_char_data = std::make_unique<stbtt_packedchar[ ]>( 95 );

		stbtt_pack_context spc{};
		if ( !stbtt_PackBegin( &spc, bitmap.data( ), atlas_width, atlas_height, 0, 1, nullptr ) ) [[unlikely]]
		{
			return nullptr;
		}

		stbtt_PackSetOversampling( &spc, 2, 2 );
		stbtt_PackSetSkipMissingCodepoints( &spc, 1 );

		stbtt_PackFontRange( &spc, reinterpret_cast< const unsigned char* >( font_data.data( ) ), 0, size_pixels, 32, 95, new_font->m_packed_char_data.get( ) );
		stbtt_PackEnd( &spc );

		stbtt_fontinfo info{};
		stbtt_InitFont( &info, reinterpret_cast< const unsigned char* >( font_data.data( ) ), 0 );

		int ascent{ 0 };
		int descent{ 0 };
		int line_gap{ 0 };
		stbtt_GetFontVMetrics( &info, &ascent, &descent, &line_gap );

		const float scale{ stbtt_ScaleForPixelHeight( &info, size_pixels ) };

		new_font->m_ascent = static_cast< float >( ascent ) * scale;
		new_font->m_descent = static_cast< float >( descent ) * scale;
		new_font->m_line_gap = static_cast< float >( line_gap ) * scale;
		new_font->m_line_height = new_font->m_ascent - new_font->m_descent + new_font->m_line_gap;

		D3D11_TEXTURE2D_DESC tex_desc{};
		tex_desc.Width = static_cast< UINT >( atlas_width );
		tex_desc.Height = static_cast< UINT >( atlas_height );
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		std::vector<std::uint8_t> rgba_bitmap( static_cast< std::size_t >( atlas_width ) * static_cast< std::size_t >( atlas_height ) * 4u );
		for ( std::size_t i{ 0 }; i < static_cast< std::size_t >( atlas_width * atlas_height ); ++i )
		{
			rgba_bitmap[ i * 4 + 0 ] = 255u;
			rgba_bitmap[ i * 4 + 1 ] = 255u;
			rgba_bitmap[ i * 4 + 2 ] = 255u;
			rgba_bitmap[ i * 4 + 3 ] = bitmap[ i ];
		}

		D3D11_SUBRESOURCE_DATA init_data{ rgba_bitmap.data( ), static_cast< UINT >( atlas_width * 4 ), 0u };

		auto hr{ detail::g_render.m_device->CreateTexture2D( &tex_desc, &init_data, &new_font->m_atlas->m_texture ) };
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		srv_desc.Format = tex_desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;

		hr = detail::g_render.m_device->CreateShaderResourceView( new_font->m_atlas->m_texture.Get( ), &srv_desc, &new_font->m_atlas->m_texture_srv );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		detail::g_render.m_loaded_fonts.push_back( std::move( new_font ) );
		return detail::g_render.m_loaded_fonts.back( ).get( );
	}

	font* load_font_from_file( std::string_view filepath, float size_pixels, int atlas_width, int atlas_height )
	{
		std::ifstream file{ std::string( filepath ), std::ios::binary | std::ios::ate };
		if ( !file.is_open( ) ) [[unlikely]]
		{
			return nullptr;
		}

		std::streamsize size{ file.tellg( ) };
		file.seekg( 0, std::ios::beg );

		std::vector<std::byte> buffer( static_cast< std::size_t >( size ) );
		if ( !file.read( reinterpret_cast< char* >( buffer.data( ) ), size ) ) [[unlikely]]
		{
			return nullptr;
		}

		return load_font_from_memory( buffer, size_pixels, atlas_width, atlas_height );
	}

	font* get_normal_font( ) noexcept
	{
		if ( detail::g_render.m_normal_font == nullptr )
		{
			const auto font_span{ std::span( reinterpret_cast< const std::byte* >( fonts::pixter ), sizeof( fonts::pixter ) ) };
			detail::g_render.m_normal_font = load_font_from_memory( font_span, 12.0f, 256, 256 );
		}

		return detail::g_render.m_normal_font;
	}

	void line( float x0, float y0, float x1, float y1, rgba color, float thickness )
	{
		get_draw_list( ).add_line( x0, y0, x1, y1, color, thickness );
	}

	void rect( float x, float y, float w, float h, rgba color, float thickness )
	{
		get_draw_list( ).add_rect( x, y, w, h, color, thickness );
	}

	void rect_cornered( float x, float y, float w, float h, rgba color, float corner_length, float thickness )
	{
		get_draw_list( ).add_rect_cornered( x, y, w, h, color, corner_length, thickness );
	}

	void rect_filled( float x, float y, float w, float h, rgba color )
	{
		get_draw_list( ).add_rect_filled( x, y, w, h, color );
	}

	void rect_filled_multi_color( float x, float y, float w, float h, rgba color_tl, rgba color_tr, rgba color_br, rgba color_bl )
	{
		get_draw_list( ).add_rect_filled_multi_color( x, y, w, h, color_tl, color_tr, color_br, color_bl );
	}

	void rect_textured( float x, float y, float w, float h, ID3D11ShaderResourceView* tex, float u0, float v0, float u1, float v1 )
	{
		get_draw_list( ).add_rect_textured( x, y, w, h, tex, u0, v0, u1, v1 );
	}

	void convex_poly_filled( std::span<const float> points, rgba color )
	{
		get_draw_list( ).add_convex_poly_filled( points, color );
	}

	void polyline( std::span<const float> points, rgba color, bool closed, float thickness )
	{
		get_draw_list( ).add_polyline( points, color, closed, thickness );
	}

	void circle( float x, float y, float radius, rgba color, int segments, float thickness )
	{
		get_draw_list( ).add_circle( x, y, radius, color, segments, thickness );
	}

	void circle_filled( float x, float y, float radius, rgba color, int segments )
	{
		get_draw_list( ).add_circle_filled( x, y, radius, color, segments );
	}

	std::pair<float, float> measure_text( std::string_view text, const font* fnt )
	{
		const auto f{ fnt != nullptr ? fnt : get_normal_font( ) };

		float w, h;
		f->calc_text_size( text, w, h );
		return { w, h };
	}

} // namespace zdraw