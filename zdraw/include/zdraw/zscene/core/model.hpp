#ifndef MODEL_HPP
#define MODEL_HPP

namespace zscene {

	using namespace DirectX;
	using Microsoft::WRL::ComPtr;

	constexpr auto k_max_bones_per_vertex{ 4 };
	constexpr auto k_max_bones{ 512 };

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

} // namespace zscene

#endif // !MODEL_HPP
