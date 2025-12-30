#include <include/global.hpp>

namespace zscene {

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

	bool scene::load_model( const std::string& filepath, bool auto_fit, bool auto_camera, bool auto_orient )
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

		if ( auto_orient )
		{
			this->detect_and_correct_orientation( );
		}

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
	const XMMATRIX& scene::get_world_transform( ) const { return this->m_world_transform; }
	const model& scene::get_model( ) const { return this->m_model; }
	const camera& scene::get_camera( ) const { return this->m_camera; }
	model& scene::get_model( ) { return this->m_model; }
	camera& scene::get_camera( ) { return this->m_camera; }

	void scene::detect_and_correct_orientation( )
	{
		const auto dx = this->m_model.bounds_max.x - this->m_model.bounds_min.x;
		const auto dy = this->m_model.bounds_max.y - this->m_model.bounds_min.y;
		const auto dz = this->m_model.bounds_max.z - this->m_model.bounds_min.z;

		if ( dz > dy )
		{
			this->m_orientation_correction = XMMatrixRotationX( XM_PIDIV2 );
			this->recalculate_bounds_after_orientation( );
		}
		else if ( dx > dy )
		{
			this->m_orientation_correction = XMMatrixRotationZ( -XM_PIDIV2 );
			this->recalculate_bounds_after_orientation( );
		}
		else
		{
			this->m_orientation_correction = XMMatrixIdentity( );
		}
	}

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