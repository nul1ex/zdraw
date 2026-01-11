#include <include/global.hpp>

#define CGLTF_IMPLEMENTATION
#include <include/zdraw/external/cgltf/cgltf.h>

namespace zscene {

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

} // namespace zscene