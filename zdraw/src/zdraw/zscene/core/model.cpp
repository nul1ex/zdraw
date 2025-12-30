#include <include/global.hpp>

namespace zscene {

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

} // namespace zscene