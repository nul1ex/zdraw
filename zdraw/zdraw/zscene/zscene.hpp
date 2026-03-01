#pragma once

#define NOMINMAX

#include <d3d11.h>
#include <wrl/client.h>
#include <directxmath.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>
#include <cfloat>

namespace zscene {

	using namespace DirectX;
	using Microsoft::WRL::ComPtr;

	struct viewport
	{
		ComPtr<ID3D11Texture2D> m_render_target{};
		ComPtr<ID3D11RenderTargetView> m_rtv{};
		ComPtr<ID3D11ShaderResourceView> m_srv{};
		ComPtr<ID3D11Texture2D> m_depth_texture{};
		ComPtr<ID3D11DepthStencilView> m_dsv{};

		ComPtr<ID3D11RenderTargetView> m_old_rtv{};
		ComPtr<ID3D11DepthStencilView> m_old_dsv{};
		D3D11_VIEWPORT m_old_viewport{};

		int m_width{ 0 };
		int m_height{ 0 };

		bool create( ID3D11Device* device, int width, int height );
		void destroy( );
		bool resize( ID3D11Device* device, int width, int height );

		void begin( ID3D11DeviceContext* ctx, float clear_r = 0.1f, float clear_g = 0.1f, float clear_b = 0.1f, float clear_a = 1.0f );
		void end( ID3D11DeviceContext* ctx );

		[[nodiscard]] ID3D11ShaderResourceView* get_srv( ) const noexcept { return this->m_srv.Get( ); }
		[[nodiscard]] float aspect_ratio( ) const noexcept { return this->m_height > 0 ? static_cast< float >( this->m_width ) / static_cast< float >( this->m_height ) : 1.0f; }
	};

	constexpr auto k_max_bones_per_vertex{ 4 };
	constexpr auto k_max_bones{ 600 };

	struct skinned_vertex
	{
		XMFLOAT3 position{};
		XMFLOAT3 normal{};
		XMFLOAT2 uv{};
		std::uint32_t bone_indices[ k_max_bones_per_vertex ]{ 0, 0, 0, 0 };
		float bone_weights[ k_max_bones_per_vertex ]{ 0.0f, 0.0f, 0.0f, 0.0f };
	};

	struct mesh
	{
		std::vector<skinned_vertex> vertices{};
		std::vector<std::uint32_t> indices{};

		ComPtr<ID3D11Buffer> vertex_buffer{};
		ComPtr<ID3D11Buffer> index_buffer{};

		int material_index{ -1 };

		bool create_buffers( ID3D11Device* device );
	};

	struct material
	{
		XMFLOAT4 base_color{ 1.0f, 1.0f, 1.0f, 1.0f };
		ComPtr<ID3D11ShaderResourceView> albedo_texture{};
		std::string name{};
	};

	struct bone
	{
		std::string name{};
		int parent_index{ -1 };
		XMMATRIX inverse_bind_matrix{ XMMatrixIdentity( ) };
		XMMATRIX local_transform{ XMMatrixIdentity( ) };
	};

	struct skeleton
	{
		std::vector<bone> bones{};

		[[nodiscard]] int find_bone( const std::string& name ) const
		{
			for ( std::size_t i = 0; i < this->bones.size( ); ++i )
			{
				if ( this->bones[ i ].name == name )
				{
					return static_cast< int >( i );
				}
			}

			return -1;
		}
	};

	struct animation_channel
	{
		int bone_index{ -1 };
		std::vector<float> translation_times{};
		std::vector<XMFLOAT3> translations{};
		std::vector<float> rotation_times{};
		std::vector<XMFLOAT4> rotations{};
		std::vector<float> scale_times{};
		std::vector<XMFLOAT3> scales{};
	};

	struct animation_clip
	{
		std::string name{};
		float duration{ 0.0f };
		std::vector<animation_channel> channels{};

		void sample( float time, std::vector<XMMATRIX>& local_transforms ) const;
	};

	struct model
	{
		std::vector<mesh> meshes{};
		std::vector<material> materials{};
		skeleton skel{};
		std::vector<animation_clip> animations{};
		std::vector<XMMATRIX> bone_matrices{};

		XMFLOAT3 bounds_min{ FLT_MAX, FLT_MAX, FLT_MAX };
		XMFLOAT3 bounds_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
		XMFLOAT3 center{ 0.0f, 0.0f, 0.0f };
		float bounding_radius{ 1.0f };

		void calculate_bounds( );
		bool create_buffers( ID3D11Device* device );
		void update_animation( int animation_index, float time );

		[[nodiscard]] int find_animation( const std::string& name ) const
		{
			for ( std::size_t i = 0; i < this->animations.size( ); ++i )
			{
				if ( this->animations[ i ].name == name )
				{
					return static_cast< int >( i );
				}
			}

			return -1;
		}
	};

	[[nodiscard]] bool load_gltf( const std::string& filepath, model& out_model );
	[[nodiscard]] bool load_gltf_from_memory( const void* data, std::size_t size, model& out_model, const std::string& base_path = "" );

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
