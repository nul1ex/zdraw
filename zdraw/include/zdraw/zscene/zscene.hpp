#pragma once

#include "core/viewport.hpp"
#include "core/model.hpp"
#include "core/loader.hpp"
#include "core/renderer.hpp"

namespace zscene {

	enum class orientation
	{
		none,
		z_up,
		x_up
	};

	struct bone_screen_pos
	{
		float x{ 0.0f };
		float y{ 0.0f };
		bool visible{ false };
		int parent_index{ -1 };
		std::string name{};
	};

	class scene
	{
	public:
		bool initialize( ID3D11Device* device, ID3D11DeviceContext* context, int viewport_width, int viewport_height );
		void shutdown( );
		void update( float delta_time );
		void render( );

		bool load_model( const std::string& filepath, bool auto_fit = true, bool auto_camera = true );
		void auto_fit_model( float target_size = 1.8f );
		void auto_position_camera( float distance_multiplier = 1.5f );

		void set_orientation( orientation orient );

		void play( );
		void pause( );
		void stop( );
		void set_animation( int index );
		void set_animation( const std::string& name );
		void set_animation_time( float time );
		void set_playback_speed( float speed );

		void enable_auto_rotate( bool enabled, float speed = 0.5f );
		void set_rotation_speed( float speed );
		void reset_rotation( );

		void set_clear_color( float r, float g, float b, float a = 1.0f );
		void set_world_transform( const XMMATRIX& transform );

		bool resize_viewport( int width, int height );

		[[nodiscard]] std::vector<bone_screen_pos> get_skeleton_screen_positions( ) const;
		[[nodiscard]] ID3D11ShaderResourceView* get_texture( ) const;
		[[nodiscard]] int get_viewport_width( ) const;
		[[nodiscard]] int get_viewport_height( ) const;
		[[nodiscard]] bool is_playing( ) const;
		[[nodiscard]] float get_animation_time( ) const;
		[[nodiscard]] float get_playback_speed( ) const;
		[[nodiscard]] float get_rotation_angle( ) const;
		[[nodiscard]] orientation get_orientation( ) const;
		[[nodiscard]] const XMMATRIX& get_world_transform( ) const;
		[[nodiscard]] const model& get_model( ) const;
		[[nodiscard]] const camera& get_camera( ) const;

		[[nodiscard]] model& get_model( );
		[[nodiscard]] camera& get_camera( );

	private:
		void apply_orientation( );
		void recalculate_bounds_after_orientation( );
		void update_world_transform( );

		ID3D11Device* m_device{ nullptr };
		ID3D11DeviceContext* m_context{ nullptr };

		viewport m_viewport{};
		renderer m_renderer{};
		model m_model{};
		camera m_camera{};

		XMMATRIX m_world_transform{ XMMatrixIdentity( ) };
		XMMATRIX m_orientation_correction{ XMMatrixIdentity( ) };

		int m_current_animation{ 0 };
		float m_animation_time{ 0.0f };
		float m_playback_speed{ 1.0f };
		bool m_playing{ true };

		float m_clear_color[ 4 ]{ 0.1f, 0.1f, 0.1f, 1.0f };
		float m_model_scale{ 1.0f };

		bool m_auto_rotate{ false };
		float m_rotation_speed{ 0.5f };
		float m_rotation_angle{ 0.0f };

		orientation m_orientation{ orientation::none };
		XMFLOAT3 m_original_bounds_min{};
		XMFLOAT3 m_original_bounds_max{};
		XMFLOAT3 m_original_center{};
		float m_original_bounding_radius{ 0.0f };
	};

} // namespace zscene