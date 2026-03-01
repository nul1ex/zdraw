#define _CRT_SECURE_NO_WARNINGS

#include "zscene.hpp"
#include "../zdraw.hpp"

#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <unordered_map>

#include "../external/shaders/shaders.hpp"

#define CGLTF_IMPLEMENTATION
#include "../external/cgltf/cgltf.h"

namespace zscene {

	bool viewport::create( ID3D11Device* device, int width, int height )
	{
		if ( !device || width <= 0 || height <= 0 )
		{
			return false;
		}

		this->m_width = width;
		this->m_height = height;

		D3D11_TEXTURE2D_DESC tex_desc{};
		tex_desc.Width = static_cast< UINT >( width );
		tex_desc.Height = static_cast< UINT >( height );
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.SampleDesc.Quality = 0;
		tex_desc.Usage = D3D11_USAGE_DEFAULT;
		tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		auto hr = device->CreateTexture2D( &tex_desc, nullptr, &this->m_render_target );
		if ( FAILED( hr ) )
		{
			return false;
		}

		D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{};
		rtv_desc.Format = tex_desc.Format;
		rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

		hr = device->CreateRenderTargetView( this->m_render_target.Get( ), &rtv_desc, &this->m_rtv );
		if ( FAILED( hr ) )
		{
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		srv_desc.Format = tex_desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;

		hr = device->CreateShaderResourceView( this->m_render_target.Get( ), &srv_desc, &this->m_srv );
		if ( FAILED( hr ) )
		{
			return false;
		}

		D3D11_TEXTURE2D_DESC depth_desc{};
		depth_desc.Width = static_cast< UINT >( width );
		depth_desc.Height = static_cast< UINT >( height );
		depth_desc.MipLevels = 1;
		depth_desc.ArraySize = 1;
		depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depth_desc.SampleDesc.Count = 1;
		depth_desc.SampleDesc.Quality = 0;
		depth_desc.Usage = D3D11_USAGE_DEFAULT;
		depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		hr = device->CreateTexture2D( &depth_desc, nullptr, &this->m_depth_texture );
		if ( FAILED( hr ) )
		{
			return false;
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
		dsv_desc.Format = depth_desc.Format;
		dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

		hr = device->CreateDepthStencilView( this->m_depth_texture.Get( ), &dsv_desc, &this->m_dsv );
		if ( FAILED( hr ) )
		{
			return false;
		}

		return true;
	}

	void viewport::destroy( )
	{
		this->m_render_target.Reset( );
		this->m_rtv.Reset( );
		this->m_srv.Reset( );
		this->m_depth_texture.Reset( );
		this->m_dsv.Reset( );
		this->m_width = 0;
		this->m_height = 0;
	}

	bool viewport::resize( ID3D11Device* device, int width, int height )
	{
		if ( width == this->m_width && height == this->m_height )
		{
			return true;
		}

		this->destroy( );
		return this->create( device, width, height );
	}

	void viewport::begin( ID3D11DeviceContext* ctx, float clear_r, float clear_g, float clear_b, float clear_a )
	{
		if ( !ctx )
		{
			return;
		}

		UINT num_viewports{ 1 };
		ctx->RSGetViewports( &num_viewports, &this->m_old_viewport );
		ctx->OMGetRenderTargets( 1, &this->m_old_rtv, &this->m_old_dsv );
		ctx->OMSetRenderTargets( 1, this->m_rtv.GetAddressOf( ), this->m_dsv.Get( ) );

		const float clear_color[ 4 ] = { clear_r, clear_g, clear_b, clear_a };
		ctx->ClearRenderTargetView( this->m_rtv.Get( ), clear_color );
		ctx->ClearDepthStencilView( this->m_dsv.Get( ), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );

		D3D11_VIEWPORT vp{};
		vp.Width = static_cast< float >( this->m_width );
		vp.Height = static_cast< float >( this->m_height );
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0.0f;
		vp.TopLeftY = 0.0f;
		ctx->RSSetViewports( 1, &vp );
	}

	void viewport::end( ID3D11DeviceContext* ctx )
	{
		if ( !ctx )
		{
			return;
		}

		ctx->OMSetRenderTargets( 1, this->m_old_rtv.GetAddressOf( ), this->m_old_dsv.Get( ) );
		ctx->RSSetViewports( 1, &this->m_old_viewport );

		this->m_old_rtv.Reset( );
		this->m_old_dsv.Reset( );
	}

	bool mesh::create_buffers( ID3D11Device* device )
	{
		if ( !device || this->vertices.empty( ) || this->indices.empty( ) )
		{
			return false;
		}

		D3D11_BUFFER_DESC vb_desc{};
		vb_desc.ByteWidth = static_cast< UINT >( this->vertices.size( ) * sizeof( skinned_vertex ) );
		vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
		vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vb_data{};
		vb_data.pSysMem = this->vertices.data( );

		if ( FAILED( device->CreateBuffer( &vb_desc, &vb_data, &this->vertex_buffer ) ) )
		{
			return false;
		}

		D3D11_BUFFER_DESC ib_desc{};
		ib_desc.ByteWidth = static_cast< UINT >( this->indices.size( ) * sizeof( std::uint32_t ) );
		ib_desc.Usage = D3D11_USAGE_IMMUTABLE;
		ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA ib_data{};
		ib_data.pSysMem = this->indices.data( );

		if ( FAILED( device->CreateBuffer( &ib_desc, &ib_data, &this->index_buffer ) ) )
		{
			return false;
		}

		return true;
	}

	static XMFLOAT3 sample_vec3( const std::vector<float>& times, const std::vector<XMFLOAT3>& values, float time )
	{
		if ( values.empty( ) )
		{
			return { 0.0f, 0.0f, 0.0f };
		}

		if ( values.size( ) == 1 || time <= times.front( ) )
		{
			return values.front( );
		}

		if ( time >= times.back( ) )
		{
			return values.back( );
		}

		std::size_t next_idx{ 0 };

		for ( std::size_t i = 0; i < times.size( ); ++i )
		{
			if ( times[ i ] > time )
			{
				next_idx = i;
				break;
			}
		}

		const auto prev_idx = next_idx > 0 ? next_idx - 1 : 0;
		const auto prev_time = times[ prev_idx ];
		const auto next_time = times[ next_idx ];
		const auto duration = next_time - prev_time;
		const auto t = duration > 0.0001f ? ( time - prev_time ) / duration : 0.0f;

		const auto& a = values[ prev_idx ];
		const auto& b = values[ next_idx ];

		return
		{
			a.x + ( b.x - a.x ) * t,
			a.y + ( b.y - a.y ) * t,
			a.z + ( b.z - a.z ) * t
		};
	}

	static XMFLOAT4 sample_quat( const std::vector<float>& times, const std::vector<XMFLOAT4>& values, float time )
	{
		if ( values.empty( ) )
		{
			return { 0.0f, 0.0f, 0.0f, 1.0f };
		}

		if ( values.size( ) == 1 || time <= times.front( ) )
		{
			return values.front( );
		}

		if ( time >= times.back( ) )
		{
			return values.back( );
		}

		std::size_t next_idx{ 0 };

		for ( std::size_t i = 0; i < times.size( ); ++i )
		{
			if ( times[ i ] > time )
			{
				next_idx = i;
				break;
			}
		}

		const auto prev_idx = next_idx > 0 ? next_idx - 1 : 0;
		const auto prev_time = times[ prev_idx ];
		const auto next_time = times[ next_idx ];
		const auto duration = next_time - prev_time;
		const auto t = duration > 0.0001f ? ( time - prev_time ) / duration : 0.0f;

		auto q0 = XMLoadFloat4( &values[ prev_idx ] );
		auto q1 = XMLoadFloat4( &values[ next_idx ] );
		auto result = XMQuaternionSlerp( q0, q1, t );

		XMFLOAT4 out;
		XMStoreFloat4( &out, result );
		return out;
	}

	void animation_clip::sample( float time, std::vector<XMMATRIX>& local_transforms ) const
	{
		if ( this->duration > 0.0f )
		{
			time = std::fmod( time, this->duration );

			if ( time < 0.0f )
			{
				time += this->duration;
			}
		}

		for ( const auto& channel : this->channels )
		{
			if ( channel.bone_index < 0 || channel.bone_index >= static_cast< int >( local_transforms.size( ) ) )
			{
				continue;
			}

			auto translation = XMFLOAT3{ 0.0f, 0.0f, 0.0f };
			auto rotation = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };
			auto scale = XMFLOAT3{ 1.0f, 1.0f, 1.0f };

			if ( !channel.translations.empty( ) )
			{
				translation = sample_vec3( channel.translation_times, channel.translations, time );
			}

			if ( !channel.rotations.empty( ) )
			{
				rotation = sample_quat( channel.rotation_times, channel.rotations, time );
			}

			if ( !channel.scales.empty( ) )
			{
				scale = sample_vec3( channel.scale_times, channel.scales, time );
			}

			const auto s = XMMatrixScaling( scale.x, scale.y, scale.z );
			const auto r = XMMatrixRotationQuaternion( XMLoadFloat4( &rotation ) );
			const auto t = XMMatrixTranslation( translation.x, translation.y, translation.z );

			local_transforms[ channel.bone_index ] = s * r * t;
		}
	}

	void model::calculate_bounds( )
	{
		this->bounds_min = { FLT_MAX, FLT_MAX, FLT_MAX };
		this->bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

		for ( const auto& m : this->meshes )
		{
			for ( const auto& v : m.vertices )
			{
				this->bounds_min.x = std::min( this->bounds_min.x, v.position.x );
				this->bounds_min.y = std::min( this->bounds_min.y, v.position.y );
				this->bounds_min.z = std::min( this->bounds_min.z, v.position.z );

				this->bounds_max.x = std::max( this->bounds_max.x, v.position.x );
				this->bounds_max.y = std::max( this->bounds_max.y, v.position.y );
				this->bounds_max.z = std::max( this->bounds_max.z, v.position.z );
			}
		}

		this->center.x = ( this->bounds_min.x + this->bounds_max.x ) * 0.5f;
		this->center.y = ( this->bounds_min.y + this->bounds_max.y ) * 0.5f;
		this->center.z = ( this->bounds_min.z + this->bounds_max.z ) * 0.5f;

		float dx = this->bounds_max.x - this->bounds_min.x;
		float dy = this->bounds_max.y - this->bounds_min.y;
		float dz = this->bounds_max.z - this->bounds_min.z;

		this->bounding_radius = std::sqrt( dx * dx + dy * dy + dz * dz ) * 0.5f;
	}

	bool model::create_buffers( ID3D11Device* device )
	{
		for ( auto& m : this->meshes )
		{
			if ( !m.create_buffers( device ) )
			{
				return false;
			}
		}

		this->bone_matrices.resize( this->skel.bones.size( ), XMMatrixIdentity( ) );
		return true;
	}

	void model::update_animation( int animation_index, float time )
	{
		if ( this->skel.bones.empty( ) )
		{
			return;
		}

		std::vector<XMMATRIX> local_transforms( this->skel.bones.size( ) );

		for ( std::size_t i = 0; i < this->skel.bones.size( ); ++i )
		{
			local_transforms[ i ] = this->skel.bones[ i ].local_transform;
		}

		if ( animation_index >= 0 && animation_index < static_cast< int >( this->animations.size( ) ) )
		{
			this->animations[ animation_index ].sample( time, local_transforms );
		}

		std::vector<XMMATRIX> global_transforms( this->skel.bones.size( ) );

		for ( std::size_t i = 0; i < this->skel.bones.size( ); ++i )
		{
			const auto& bone = this->skel.bones[ i ];

			if ( bone.parent_index >= 0 )
			{
				global_transforms[ i ] = local_transforms[ i ] * global_transforms[ bone.parent_index ];
			}
			else
			{
				global_transforms[ i ] = local_transforms[ i ];
			}
		}

		this->bone_matrices.resize( this->skel.bones.size( ) );

		for ( std::size_t i = 0; i < this->skel.bones.size( ); ++i )
		{
			this->bone_matrices[ i ] = this->skel.bones[ i ].inverse_bind_matrix * global_transforms[ i ];
		}
	}

	static std::string get_directory( const std::string& filepath )
	{
		std::filesystem::path p{ filepath };
		return p.parent_path( ).string( );
	}

	static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> load_texture_from_gltf_image( cgltf_image* image, const std::string& base_path )
	{
		if ( !image )
		{
			return nullptr;
		}

		if ( image->uri && !image->buffer_view )
		{
			std::string uri{ image->uri };

			if ( uri.find( "data:" ) == 0 )
			{
				return nullptr;
			}

			return zdraw::load_texture_from_file( base_path.empty( ) ? uri : base_path + "/" + uri );
		}
		else if ( image->buffer_view )
		{
			const auto buffer_data = static_cast< const std::uint8_t* >( image->buffer_view->buffer->data );
			const auto image_data = buffer_data + image->buffer_view->offset;
			const auto image_size = image->buffer_view->size;

			return zdraw::load_texture_from_memory( std::span<const std::byte>{ reinterpret_cast< const std::byte* >( image_data ), image_size } );
		}

		return nullptr;
	}

	static void load_materials( cgltf_data* data, model& out_model, const std::string& base_path )
	{
		if ( data->materials_count == 0 )
		{
			return;
		}

		out_model.materials.resize( data->materials_count );

		for ( cgltf_size i = 0; i < data->materials_count; ++i )
		{
			const auto gltf_mat = &data->materials[ i ];
			auto& mat = out_model.materials[ i ];

			mat.name = gltf_mat->name ? gltf_mat->name : "";

			if ( gltf_mat->has_pbr_metallic_roughness )
			{
				auto& pbr = gltf_mat->pbr_metallic_roughness;

				mat.base_color.x = pbr.base_color_factor[ 0 ];
				mat.base_color.y = pbr.base_color_factor[ 1 ];
				mat.base_color.z = pbr.base_color_factor[ 2 ];
				mat.base_color.w = pbr.base_color_factor[ 3 ];

				if ( pbr.base_color_texture.texture )
				{
					auto img = pbr.base_color_texture.texture->image;
					if ( !img )
					{
						auto tex = pbr.base_color_texture.texture;
						auto tex_index = tex - data->textures;

						if ( tex_index < data->images_count )
						{
							img = &data->images[ tex_index ];
						}
					}

					if ( img )
					{
						mat.albedo_texture = load_texture_from_gltf_image( img, base_path );
					}
				}
			}
		}
	}

	static void* get_accessor_data( const cgltf_accessor* accessor, cgltf_size* out_count )
	{
		if ( !accessor || !accessor->buffer_view || !accessor->buffer_view->buffer->data )
		{
			return nullptr;
		}

		*out_count = accessor->count;
		const auto buffer_data = static_cast< const std::uint8_t* >( accessor->buffer_view->buffer->data );
		return const_cast< void* >( static_cast< const void* >( buffer_data + accessor->buffer_view->offset + accessor->offset ) );
	}

	static float read_float( const cgltf_accessor* accessor, cgltf_size index, cgltf_size component )
	{
		float out{ 0.0f };
		cgltf_accessor_read_float( accessor, index, &out + component, 1 );
		return out;
	}

	static void read_floats( const cgltf_accessor* accessor, cgltf_size index, float* out, cgltf_size count )
	{
		cgltf_accessor_read_float( accessor, index, out, count );
	}

	static std::uint32_t read_uint( const cgltf_accessor* accessor, cgltf_size index )
	{
		cgltf_uint out{ 0 };
		cgltf_accessor_read_uint( accessor, index, &out, 1 );
		return out;
	}

	static int find_node_index( cgltf_data* data, cgltf_node* node )
	{
		for ( cgltf_size i = 0; i < data->nodes_count; ++i )
		{
			if ( &data->nodes[ i ] == node )
			{
				return static_cast< int >( i );
			}
		}

		return -1;
	}

	static XMMATRIX node_local_transform( cgltf_node* node )
	{
		if ( node->has_matrix )
		{
			return XMMATRIX
			(
				node->matrix[ 0 ], node->matrix[ 4 ], node->matrix[ 8 ], node->matrix[ 12 ],
				node->matrix[ 1 ], node->matrix[ 5 ], node->matrix[ 9 ], node->matrix[ 13 ],
				node->matrix[ 2 ], node->matrix[ 6 ], node->matrix[ 10 ], node->matrix[ 14 ],
				node->matrix[ 3 ], node->matrix[ 7 ], node->matrix[ 11 ], node->matrix[ 15 ]
			);
		}

		auto t = XMMatrixIdentity( );
		auto r = XMMatrixIdentity( );
		auto s = XMMatrixIdentity( );

		if ( node->has_translation )
		{
			t = XMMatrixTranslation( node->translation[ 0 ], node->translation[ 1 ], node->translation[ 2 ] );
		}

		if ( node->has_rotation )
		{
			auto q = XMVectorSet( node->rotation[ 0 ], node->rotation[ 1 ], node->rotation[ 2 ], node->rotation[ 3 ] );
			r = XMMatrixRotationQuaternion( q );
		}

		if ( node->has_scale )
		{
			s = XMMatrixScaling( node->scale[ 0 ], node->scale[ 1 ], node->scale[ 2 ] );
		}

		return s * r * t;
	}

	static bool load_mesh( cgltf_mesh* gltf_mesh, cgltf_data* data, model& out_model, const std::unordered_map<cgltf_node*, int>& joint_map )
	{
		for ( cgltf_size p = 0; p < gltf_mesh->primitives_count; ++p )
		{
			auto prim = &gltf_mesh->primitives[ p ];
			if ( prim->type != cgltf_primitive_type_triangles )
			{
				continue;
			}

			mesh m{};

			if ( prim->material )
			{
				for ( cgltf_size i = 0; i < data->materials_count; ++i )
				{
					if ( &data->materials[ i ] == prim->material )
					{
						m.material_index = static_cast< int >( i );
						break;
					}
				}
			}

			cgltf_accessor* pos_accessor{ nullptr };
			cgltf_accessor* norm_accessor{ nullptr };
			cgltf_accessor* uv_accessor{ nullptr };
			cgltf_accessor* joints_accessor{ nullptr };
			cgltf_accessor* weights_accessor{ nullptr };

			for ( cgltf_size a = 0; a < prim->attributes_count; ++a )
			{
				auto attr = &prim->attributes[ a ];
				switch ( attr->type )
				{
				case cgltf_attribute_type_position: pos_accessor = attr->data; break;
				case cgltf_attribute_type_normal: norm_accessor = attr->data; break;
				case cgltf_attribute_type_texcoord: if ( !uv_accessor ) uv_accessor = attr->data; break;
				case cgltf_attribute_type_joints: if ( !joints_accessor ) joints_accessor = attr->data; break;
				case cgltf_attribute_type_weights: if ( !weights_accessor ) weights_accessor = attr->data; break;
				default: break;
				}
			}

			if ( !pos_accessor )
			{
				continue;
			}

			const auto vertex_count = pos_accessor->count;
			m.vertices.resize( vertex_count );

			for ( cgltf_size v = 0; v < vertex_count; ++v )
			{
				auto& vert = m.vertices[ v ];

				float pos[ 3 ]{ 0.0f, 0.0f, 0.0f };
				cgltf_accessor_read_float( pos_accessor, v, pos, 3 );

				vert.position = { pos[ 0 ], pos[ 1 ], pos[ 2 ] };

				if ( norm_accessor )
				{
					float norm[ 3 ]{ 0.0f, 0.0f, 0.0f };
					cgltf_accessor_read_float( norm_accessor, v, norm, 3 );

					vert.normal = { norm[ 0 ], norm[ 1 ], norm[ 2 ] };
				}

				if ( uv_accessor )
				{
					float uv[ 2 ]{ 0.0f, 0.0f };
					cgltf_accessor_read_float( uv_accessor, v, uv, 2 );

					vert.uv = { uv[ 0 ], uv[ 1 ] };
				}

				if ( joints_accessor )
				{
					cgltf_uint joints[ 4 ]{ 0, 0, 0, 0 };
					cgltf_accessor_read_uint( joints_accessor, v, joints, 4 );

					for ( int i = 0; i < 4; ++i )
					{
						vert.bone_indices[ i ] = joints[ i ];
					}
				}

				if ( weights_accessor )
				{
					float weights[ 4 ]{ 0.0f, 0.0f, 0.0f, 0.0f };
					cgltf_accessor_read_float( weights_accessor, v, weights, 4 );

					for ( int i = 0; i < 4; ++i )
					{
						vert.bone_weights[ i ] = weights[ i ];
					}
				}
				else if ( joints_accessor )
				{
					vert.bone_weights[ 0 ] = 1.0f;
				}
			}

			if ( prim->indices )
			{
				auto index_count = prim->indices->count;
				m.indices.resize( index_count );

				for ( cgltf_size i = 0; i < index_count; ++i )
				{
					m.indices[ i ] = static_cast< std::uint32_t >( cgltf_accessor_read_index( prim->indices, i ) );
				}
			}
			else
			{
				m.indices.resize( vertex_count );

				for ( cgltf_size i = 0; i < vertex_count; ++i )
				{
					m.indices[ i ] = static_cast< std::uint32_t >( i );
				}
			}

			out_model.meshes.push_back( std::move( m ) );
		}

		return true;
	}

	static void load_skeleton( cgltf_data* data, cgltf_skin* skin, model& out_model, std::unordered_map<cgltf_node*, int>& joint_map )
	{
		if ( !skin )
		{
			return;
		}

		out_model.skel.bones.resize( skin->joints_count );

		for ( cgltf_size i = 0; i < skin->joints_count; ++i )
		{
			joint_map[ skin->joints[ i ] ] = static_cast< int >( i );
		}

		for ( cgltf_size i = 0; i < skin->joints_count; ++i )
		{
			auto joint = skin->joints[ i ];
			auto& b = out_model.skel.bones[ i ];

			b.name = joint->name ? joint->name : "";
			b.local_transform = node_local_transform( joint );

			if ( joint->parent )
			{
				auto it = joint_map.find( joint->parent );
				if ( it != joint_map.end( ) )
				{
					b.parent_index = it->second;
				}
			}

			if ( skin->inverse_bind_matrices )
			{
				float ibm[ 16 ];
				cgltf_accessor_read_float( skin->inverse_bind_matrices, i, ibm, 16 );

				b.inverse_bind_matrix = XMMATRIX
				(
					ibm[ 0 ], ibm[ 1 ], ibm[ 2 ], ibm[ 3 ],
					ibm[ 4 ], ibm[ 5 ], ibm[ 6 ], ibm[ 7 ],
					ibm[ 8 ], ibm[ 9 ], ibm[ 10 ], ibm[ 11 ],
					ibm[ 12 ], ibm[ 13 ], ibm[ 14 ], ibm[ 15 ]
				);
			}
		}

		if ( out_model.skel.bones.size( ) > 2 )
		{
			XMFLOAT4X4 test_ibm;
			XMStoreFloat4x4( &test_ibm, out_model.skel.bones[ 2 ].inverse_bind_matrix );

			auto row0 = XMVectorSet( test_ibm._11, test_ibm._12, test_ibm._13, 0.0f );
			auto row1 = XMVectorSet( test_ibm._21, test_ibm._22, test_ibm._23, 0.0f );
			auto row2 = XMVectorSet( test_ibm._31, test_ibm._32, test_ibm._33, 0.0f );

			auto scale_x = XMVectorGetX( XMVector3Length( row0 ) );
			auto scale_y = XMVectorGetX( XMVector3Length( row1 ) );
			auto scale_z = XMVectorGetX( XMVector3Length( row2 ) );
			auto avg_scale = ( scale_x + scale_y + scale_z ) / 3.0f;

			if ( avg_scale > 0.005f && avg_scale < 0.02f )
			{
				auto scale_fix = XMMatrixScaling( 100.0f, 100.0f, 100.0f );

				for ( auto& bone : out_model.skel.bones )
				{
					bone.inverse_bind_matrix = bone.inverse_bind_matrix * scale_fix;
				}
			}
		}
	}

	static void load_animations( cgltf_data* data, model& out_model, const std::unordered_map<cgltf_node*, int>& joint_map )
	{
		for ( cgltf_size a = 0; a < data->animations_count; ++a )
		{
			auto gltf_anim = &data->animations[ a ];

			animation_clip clip{};
			clip.name = gltf_anim->name ? gltf_anim->name : "";

			std::unordered_map<int, animation_channel*> channel_map{};

			for ( cgltf_size c = 0; c < gltf_anim->channels_count; ++c )
			{
				auto gltf_channel = &gltf_anim->channels[ c ];
				auto sampler = gltf_channel->sampler;

				if ( !gltf_channel->target_node || !sampler )
				{
					continue;
				}

				auto it = joint_map.find( gltf_channel->target_node );
				if ( it == joint_map.end( ) )
				{
					continue;
				}

				auto bone_index = it->second;
				animation_channel* channel{ nullptr };
				auto chan_it = channel_map.find( bone_index );

				if ( chan_it != channel_map.end( ) )
				{
					channel = chan_it->second;
				}
				else
				{
					clip.channels.emplace_back( );
					channel = &clip.channels.back( );
					channel->bone_index = bone_index;
					channel_map[ bone_index ] = channel;
				}

				auto input = sampler->input;
				std::vector<float> times( input->count );

				for ( cgltf_size i = 0; i < input->count; ++i )
				{
					cgltf_accessor_read_float( input, i, &times[ i ], 1 );
					clip.duration = std::max( clip.duration, times[ i ] );
				}

				auto output = sampler->output;

				switch ( gltf_channel->target_path )
				{
				case cgltf_animation_path_type_translation:
				{
					channel->translation_times = times;
					channel->translations.resize( output->count );

					for ( cgltf_size i = 0; i < output->count; ++i )
					{
						float v[ 3 ];
						cgltf_accessor_read_float( output, i, v, 3 );
						channel->translations[ i ] = { v[ 0 ], v[ 1 ], v[ 2 ] };
					}

					break;
				}
				case cgltf_animation_path_type_rotation:
				{
					channel->rotation_times = times;
					channel->rotations.resize( output->count );

					for ( cgltf_size i = 0; i < output->count; ++i )
					{
						float v[ 4 ];
						cgltf_accessor_read_float( output, i, v, 4 );
						channel->rotations[ i ] = { v[ 0 ], v[ 1 ], v[ 2 ], v[ 3 ] };
					}

					break;
				}
				case cgltf_animation_path_type_scale:
				{
					channel->scale_times = times;
					channel->scales.resize( output->count );

					for ( cgltf_size i = 0; i < output->count; ++i )
					{
						float v[ 3 ];
						cgltf_accessor_read_float( output, i, v, 3 );
						channel->scales[ i ] = { v[ 0 ], v[ 1 ], v[ 2 ] };
					}

					break;
				}
				default:
					break;
				}
			}

			if ( !clip.channels.empty( ) )
			{
				out_model.animations.push_back( std::move( clip ) );
			}
		}
	}

	bool load_gltf( const std::string& filepath, model& out_model )
	{
		cgltf_options options{};
		cgltf_data* data{ nullptr };

		auto result = cgltf_parse_file( &options, filepath.c_str( ), &data );
		if ( result != cgltf_result_success )
		{
			return false;
		}

		result = cgltf_load_buffers( &options, data, filepath.c_str( ) );
		if ( result != cgltf_result_success )
		{
			cgltf_free( data );
			return false;
		}

		auto base_path = get_directory( filepath );

		load_materials( data, out_model, base_path );

		auto skin = data->skins_count > 0 ? &data->skins[ 0 ] : nullptr;

		std::unordered_map<cgltf_node*, int> joint_map{};
		load_skeleton( data, skin, out_model, joint_map );

		for ( cgltf_size i = 0; i < data->meshes_count; ++i )
		{
			load_mesh( &data->meshes[ i ], data, out_model, joint_map );
		}

		load_animations( data, out_model, joint_map );

		cgltf_free( data );
		return true;
	}

	bool load_gltf_from_memory( const void* data, std::size_t size, model& out_model, const std::string& base_path )
	{
		cgltf_options options{};
		cgltf_data* gltf_data{ nullptr };

		auto result = cgltf_parse( &options, data, size, &gltf_data );
		if ( result != cgltf_result_success )
		{
			return false;
		}

		load_materials( gltf_data, out_model, base_path );

		auto skin = gltf_data->skins_count > 0 ? &gltf_data->skins[ 0 ] : nullptr;

		std::unordered_map<cgltf_node*, int> joint_map{};
		load_skeleton( gltf_data, skin, out_model, joint_map );

		for ( cgltf_size i = 0; i < gltf_data->meshes_count; ++i )
		{
			load_mesh( &gltf_data->meshes[ i ], gltf_data, out_model, joint_map );
		}

		load_animations( gltf_data, out_model, joint_map );

		cgltf_free( gltf_data );
		return true;
	}

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

	bool scene::initialize( ID3D11Device* device, ID3D11DeviceContext* context, int viewport_width, int viewport_height )
	{
		this->m_device = device;
		this->m_context = context;

		if ( !this->m_viewport.create( device, viewport_width, viewport_height ) )
		{
			return false;
		}

		if ( !this->m_renderer.initialize( device, context ) )
		{
			return false;
		}

		return true;
	}

	void scene::shutdown( )
	{
		this->m_renderer.shutdown( );
		this->m_viewport.destroy( );
	}

	void scene::update( float delta_time )
	{
		if ( this->m_playing && !this->m_model.animations.empty( ) )
		{
			this->m_animation_time += delta_time * this->m_playback_speed;
		}

		this->m_model.update_animation( this->m_current_animation, this->m_animation_time );

		if ( this->m_auto_rotate && this->m_playing )
		{
			this->m_rotation_angle += this->m_rotation_speed * delta_time;
			this->update_world_transform( );
		}
	}

	void scene::render( )
	{
		this->m_viewport.begin( this->m_context, this->m_clear_color[ 0 ], this->m_clear_color[ 1 ], this->m_clear_color[ 2 ], this->m_clear_color[ 3 ] );
		this->m_renderer.render( this->m_model, this->m_world_transform, this->m_camera, this->m_viewport.aspect_ratio( ) );
		this->m_viewport.end( this->m_context );
	}

	bool scene::load_model( const std::string& filepath, bool auto_fit, bool auto_camera )
	{
		if ( !load_gltf( filepath, this->m_model ) )
		{
			return false;
		}

		if ( !this->m_model.create_buffers( this->m_device ) )
		{
			return false;
		}

		this->m_model.calculate_bounds( );

		this->m_original_bounds_min = this->m_model.bounds_min;
		this->m_original_bounds_max = this->m_model.bounds_max;
		this->m_original_center = this->m_model.center;
		this->m_original_bounding_radius = this->m_model.bounding_radius;

		this->m_orientation = orientation::none;
		this->m_orientation_correction = XMMatrixIdentity( );

		if ( auto_fit )
		{
			this->auto_fit_model( );
		}

		if ( auto_camera )
		{
			this->auto_position_camera( );
		}

		return true;
	}

	void scene::set_orientation( orientation orient )
	{
		if ( this->m_orientation == orient )
		{
			return;
		}

		this->m_model.bounds_min = this->m_original_bounds_min;
		this->m_model.bounds_max = this->m_original_bounds_max;
		this->m_model.center = this->m_original_center;
		this->m_model.bounding_radius = this->m_original_bounding_radius;

		this->m_orientation = orient;

		this->apply_orientation( );
		this->auto_fit_model( );
		this->auto_position_camera( );
	}

	void scene::apply_orientation( )
	{
		switch ( this->m_orientation )
		{
		case orientation::z_up:
			this->m_orientation_correction = XMMatrixRotationX( -XM_PIDIV2 );
			this->recalculate_bounds_after_orientation( );
			break;

		case orientation::x_up:
			this->m_orientation_correction = XMMatrixRotationZ( XM_PIDIV2 );
			this->recalculate_bounds_after_orientation( );
			break;

		case orientation::none:
		default:
			this->m_orientation_correction = XMMatrixIdentity( );
			break;
		}
	}

	void scene::auto_fit_model( float target_size )
	{
		const auto dx = this->m_model.bounds_max.x - this->m_model.bounds_min.x;
		const auto dy = this->m_model.bounds_max.y - this->m_model.bounds_min.y;
		const auto dz = this->m_model.bounds_max.z - this->m_model.bounds_min.z;
		const auto max_extent = std::max( { dx, dy, dz } );

		this->m_model_scale = max_extent > 0.0f ? target_size / max_extent : 1.0f;
		this->update_world_transform( );
	}

	void scene::auto_position_camera( float distance_multiplier )
	{
		const auto dx = ( this->m_model.bounds_max.x - this->m_model.bounds_min.x ) * this->m_model_scale;
		const auto dy = ( this->m_model.bounds_max.y - this->m_model.bounds_min.y ) * this->m_model_scale;
		const auto dz = ( this->m_model.bounds_max.z - this->m_model.bounds_min.z ) * this->m_model_scale;

		const auto max_dim = std::max( { dx, dy, dz } );
		const auto fov_tan = std::tan( this->m_camera.fov * 0.5f );
		const auto calculated_distance = fov_tan > 0.0f ? ( max_dim * 0.5f / fov_tan ) * distance_multiplier : 5.0f;
		const auto distance = std::max( this->m_camera.near_z * 1.5f, calculated_distance );
		const auto look_at_y = dy * 0.5f;

		this->m_camera.position = XMFLOAT3{ 0.0f, look_at_y, distance };
		this->m_camera.target = XMFLOAT3{ 0.0f, look_at_y, 0.0f };
	}

	void scene::play( ) { this->m_playing = true; }
	void scene::pause( ) { this->m_playing = false; }
	void scene::stop( ) { this->m_playing = false; this->m_animation_time = 0.0f; }

	void scene::set_animation( int index )
	{
		this->m_current_animation = index;
		this->m_animation_time = 0.0f;
	}

	void scene::set_animation( const std::string& name )
	{
		const auto idx = this->m_model.find_animation( name );
		if ( idx >= 0 )
		{
			this->set_animation( idx );
		}
	}

	void scene::set_animation_time( float time ) { this->m_animation_time = time; }
	void scene::set_playback_speed( float speed ) { this->m_playback_speed = speed; }

	void scene::enable_auto_rotate( bool enabled, float speed )
	{
		this->m_auto_rotate = enabled;
		this->m_rotation_speed = speed;
	}

	void scene::set_rotation_speed( float speed ) { this->m_rotation_speed = speed; }

	void scene::reset_rotation( )
	{
		this->m_rotation_angle = 0.0f;
		this->update_world_transform( );
	}

	void scene::set_clear_color( float r, float g, float b, float a )
	{
		this->m_clear_color[ 0 ] = r;
		this->m_clear_color[ 1 ] = g;
		this->m_clear_color[ 2 ] = b;
		this->m_clear_color[ 3 ] = a;
	}

	void scene::set_world_transform( const XMMATRIX& transform ) { this->m_world_transform = transform; }
	bool scene::resize_viewport( int width, int height ) { return this->m_viewport.resize( this->m_device, width, height ); }

	ID3D11ShaderResourceView* scene::get_texture( ) const { return this->m_viewport.get_srv( ); }
	int scene::get_viewport_width( ) const { return this->m_viewport.m_width; }
	int scene::get_viewport_height( ) const { return this->m_viewport.m_height; }
	bool scene::is_playing( ) const { return this->m_playing; }
	float scene::get_animation_time( ) const { return this->m_animation_time; }
	float scene::get_playback_speed( ) const { return this->m_playback_speed; }
	float scene::get_rotation_angle( ) const { return this->m_rotation_angle; }
	orientation scene::get_orientation( ) const { return this->m_orientation; }
	const XMMATRIX& scene::get_world_transform( ) const { return this->m_world_transform; }
	const model& scene::get_model( ) const { return this->m_model; }
	const camera& scene::get_camera( ) const { return this->m_camera; }
	model& scene::get_model( ) { return this->m_model; }
	camera& scene::get_camera( ) { return this->m_camera; }

	void scene::recalculate_bounds_after_orientation( )
	{
		const XMFLOAT3 corners[ 8 ] =
		{
			{ this->m_model.bounds_min.x, this->m_model.bounds_min.y, this->m_model.bounds_min.z },
			{ this->m_model.bounds_max.x, this->m_model.bounds_min.y, this->m_model.bounds_min.z },
			{ this->m_model.bounds_min.x, this->m_model.bounds_max.y, this->m_model.bounds_min.z },
			{ this->m_model.bounds_max.x, this->m_model.bounds_max.y, this->m_model.bounds_min.z },
			{ this->m_model.bounds_min.x, this->m_model.bounds_min.y, this->m_model.bounds_max.z },
			{ this->m_model.bounds_max.x, this->m_model.bounds_min.y, this->m_model.bounds_max.z },
			{ this->m_model.bounds_min.x, this->m_model.bounds_max.y, this->m_model.bounds_max.z },
			{ this->m_model.bounds_max.x, this->m_model.bounds_max.y, this->m_model.bounds_max.z },
		};

		this->m_model.bounds_min = { FLT_MAX, FLT_MAX, FLT_MAX };
		this->m_model.bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

		for ( const auto& corner : corners )
		{
			auto v = XMVectorSet( corner.x, corner.y, corner.z, 1.0f );
			v = XMVector3Transform( v, this->m_orientation_correction );

			XMFLOAT3 transformed;
			XMStoreFloat3( &transformed, v );

			this->m_model.bounds_min.x = std::min( this->m_model.bounds_min.x, transformed.x );
			this->m_model.bounds_min.y = std::min( this->m_model.bounds_min.y, transformed.y );
			this->m_model.bounds_min.z = std::min( this->m_model.bounds_min.z, transformed.z );

			this->m_model.bounds_max.x = std::max( this->m_model.bounds_max.x, transformed.x );
			this->m_model.bounds_max.y = std::max( this->m_model.bounds_max.y, transformed.y );
			this->m_model.bounds_max.z = std::max( this->m_model.bounds_max.z, transformed.z );
		}

		this->m_model.center.x = ( this->m_model.bounds_min.x + this->m_model.bounds_max.x ) * 0.5f;
		this->m_model.center.y = ( this->m_model.bounds_min.y + this->m_model.bounds_max.y ) * 0.5f;
		this->m_model.center.z = ( this->m_model.bounds_min.z + this->m_model.bounds_max.z ) * 0.5f;

		const auto new_dx = this->m_model.bounds_max.x - this->m_model.bounds_min.x;
		const auto new_dy = this->m_model.bounds_max.y - this->m_model.bounds_min.y;
		const auto new_dz = this->m_model.bounds_max.z - this->m_model.bounds_min.z;
		this->m_model.bounding_radius = std::sqrt( new_dx * new_dx + new_dy * new_dy + new_dz * new_dz ) * 0.5f;
	}

	void scene::update_world_transform( )
	{
		const auto center_offset = XMMatrixTranslation( -this->m_model.center.x, -this->m_model.bounds_min.y, -this->m_model.center.z );
		const auto scale = XMMatrixScaling( this->m_model_scale, this->m_model_scale, this->m_model_scale );
		const auto rotation = XMMatrixRotationY( this->m_rotation_angle );

		this->m_world_transform = this->m_orientation_correction * center_offset * scale * rotation;
	}

	std::vector<bone_screen_pos> scene::get_skeleton_screen_positions( ) const
	{
		std::vector<bone_screen_pos> result;

		if ( this->m_model.skel.bones.empty( ) )
		{
			return result;
		}

		result.resize( this->m_model.skel.bones.size( ) );

		std::vector<XMMATRIX> global_transforms( this->m_model.skel.bones.size( ) );

		for ( std::size_t i = 0; i < this->m_model.skel.bones.size( ); ++i )
		{
			const auto& bone = this->m_model.skel.bones[ i ];

			if ( bone.parent_index >= 0 )
			{
				global_transforms[ i ] = bone.local_transform * global_transforms[ bone.parent_index ];
			}
			else
			{
				global_transforms[ i ] = bone.local_transform;
			}
		}

		const auto view = this->m_camera.view_matrix( );
		const auto proj = this->m_camera.projection_matrix( this->m_viewport.aspect_ratio( ) );
		const auto world_view_proj = this->m_world_transform * view * proj;

		const auto vp_w = static_cast< float >( this->m_viewport.m_width );
		const auto vp_h = static_cast< float >( this->m_viewport.m_height );

		for ( std::size_t i = 0; i < this->m_model.skel.bones.size( ); ++i )
		{
			const auto& bone = this->m_model.skel.bones[ i ];

			XMFLOAT4X4 mat;
			XMStoreFloat4x4( &mat, global_transforms[ i ] );

			const auto bone_pos = XMVectorSet( mat._41, mat._42, mat._43, 1.0f );
			const auto clip_pos = XMVector4Transform( bone_pos, world_view_proj );

			XMFLOAT4 clip;
			XMStoreFloat4( &clip, clip_pos );

			auto& out = result[ i ];
			out.parent_index = bone.parent_index;
			out.name = bone.name;

			if ( clip.w <= 0.0f )
			{
				out.visible = false;
				continue;
			}

			const auto ndc_x = clip.x / clip.w;
			const auto ndc_y = clip.y / clip.w;
			const auto ndc_z = clip.z / clip.w;

			if ( ndc_x < -1.0f || ndc_x > 1.0f || ndc_y < -1.0f || ndc_y > 1.0f || ndc_z < 0.0f || ndc_z > 1.0f )
			{
				out.visible = false;
				continue;
			}

			out.x = ( ndc_x * 0.5f + 0.5f ) * vp_w;
			out.y = ( -ndc_y * 0.5f + 0.5f ) * vp_h;
			out.visible = true;
		}

		return result;
	}

} // namespace zscene
