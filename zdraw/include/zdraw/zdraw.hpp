#ifndef ZDRAW_HPP
#define ZDRAW_HPP

#define NOMINMAX

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <array>
#include <vector>
#include <span>
#include <string>
#include <string_view>
#include <memory>
#include <utility>

#include "external/stb/truetype.hpp"
#include "external/unordered_dense.hpp"

namespace zdraw
{
	struct font;
	struct font_atlas;

	struct rgba
	{
		union
		{
			std::uint32_t val;
			struct
			{
				std::uint8_t r;
				std::uint8_t g;
				std::uint8_t b;
				std::uint8_t a;
			};
		};

		constexpr rgba( ) : r{ 0 }, g{ 0 }, b{ 0 }, a{ 0 } { }
		constexpr rgba( std::uint32_t v ) : val{ v } { }
		constexpr rgba( std::uint8_t r_, std::uint8_t g_, std::uint8_t b_, std::uint8_t a_ ) : r{ r_ }, g{ g_ }, b{ b_ }, a{ a_ } { }

		constexpr operator std::uint32_t( ) const
		{
			return this->val;
		}

		constexpr auto to_float( ) const
		{
			return std::array
			{
				static_cast< float >( this->r ) / 255.0f,
				static_cast< float >( this->g ) / 255.0f,
				static_cast< float >( this->b ) / 255.0f,
				static_cast< float >( this->a ) / 255.0f
			};
		}
	};

	struct vertex
	{
		float m_pos[ 2 ];
		float m_uv[ 2 ];
		rgba  m_col;
	};

	struct draw_cmd
	{
		std::uint32_t m_idx_offset{ 0 };
		std::uint32_t m_idx_count{ 0 };
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture;

		bool m_has_clip{ false };
		D3D11_RECT m_clip_rect{};

		draw_cmd( ) = default;
		draw_cmd( std::uint32_t idx_off, std::uint32_t count, ID3D11ShaderResourceView* tex ) : m_idx_offset{ idx_off }, m_idx_count{ count }, m_texture{ tex } { }
	};

	template<typename T>
	class nvec
	{
	private:
		std::vector<T> m_data{};
		std::size_t m_size{ 0 };

	public:
		void clear( ) noexcept
		{
			this->m_size = 0;
		}

		void reserve( std::size_t capacity )
		{
			if ( capacity > this->m_data.size( ) )
			{
				this->m_data.resize( capacity );
			}
		}

		[[nodiscard]] T* allocate( std::size_t count )
		{
			if ( this->m_size + count > this->m_data.size( ) )
			{
				this->m_data.resize( ( this->m_size + count ) * 2 );
			}

			T* result{ &this->m_data[ this->m_size ] };
			this->m_size += count;
			return result;
		}

		[[nodiscard]] std::size_t size( ) const noexcept
		{
			return this->m_size;
		}

		[[nodiscard]] const T* data( ) const noexcept
		{
			return this->m_data.data( );
		}

		[[nodiscard]] T* data( ) noexcept
		{
			return this->m_data.data( );
		}

		[[nodiscard]] std::span<const T> span( ) const noexcept
		{
			return std::span{ this->m_data.data( ), this->m_size };
		}
	};

	struct draw_list
	{
		nvec<vertex> m_vertices{};
		nvec<std::uint32_t> m_indices{};
		nvec<draw_cmd> m_commands{};
		std::vector<D3D11_RECT> m_clip_stack{};

		void clear( ) noexcept
		{
			this->m_vertices.clear( );
			this->m_indices.clear( );
			this->m_commands.clear( );
			this->m_clip_stack.clear( );
		}

		void reserve( std::uint32_t vtx_count, std::uint32_t idx_count, std::uint32_t cmd_count = 0 )
		{
			this->m_vertices.reserve( vtx_count );
			this->m_indices.reserve( idx_count );
			if ( cmd_count > 0 ) this->m_commands.reserve( cmd_count );
		}

		void push_vertex( float x, float y, float u, float v, rgba color )
		{
			vertex* vtx{ this->m_vertices.allocate( 1 ) };
			vtx->m_pos[ 0 ] = x;
			vtx->m_pos[ 1 ] = y;
			vtx->m_uv[ 0 ] = u;
			vtx->m_uv[ 1 ] = v;
			vtx->m_col = color;
		}

		void push_clip_rect( float x0, float y0, float x1, float y1 );
		void pop_clip_rect( );

		void ensure_draw_cmd( ID3D11ShaderResourceView* texture );

		void add_line( float x0, float y0, float x1, float y1, rgba color, float thickness = 1.0f );
		void add_rect( float x, float y, float w, float h, rgba color, float thickness = 1.0f );
		void add_rect_cornered( float x, float y, float w, float h, rgba color, float corner_length, float thickness );
		void add_rect_filled( float x, float y, float w, float h, rgba color );
		void add_rect_filled_multi_color( float x, float y, float w, float h, rgba color_tl, rgba color_tr, rgba color_br, rgba color_bl );
		void add_rect_textured( float x, float y, float w, float h, ID3D11ShaderResourceView* tex, float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f );
		void add_convex_poly_filled( std::span<const float> points, rgba color );
		void add_polyline( std::span<const float> points, rgba color, bool closed = false, float thickness = 1.0f );
		void add_circle( float x, float y, float radius, rgba color, int segments = 32, float thickness = 1.0f );
		void add_circle_filled( float x, float y, float radius, rgba color, int segments = 32 );
		void add_text( float x, float y, std::string_view text, const font* font, rgba color );
	};

	struct font_atlas
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture{};
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture_srv{};
		int m_width{ 0 };
		int m_height{ 0 };
	};

	struct glyph_cache_entry
	{
		float m_advance_x{ 0.0f };
		float m_quad_x0{ 0.0f };
		float m_quad_y0{ 0.0f };
		float m_quad_x1{ 0.0f };
		float m_quad_y1{ 0.0f };
		float m_uv_x0{ 0.0f };
		float m_uv_y0{ 0.0f };
		float m_uv_x1{ 0.0f };
		float m_uv_y1{ 0.0f };
		bool  m_valid{ false };
	};

	struct font
	{
		std::shared_ptr<font_atlas> m_atlas{};
		float m_font_size{ 12.0f };
		float m_ascent{ 0.0f };
		float m_descent{ 0.0f };
		float m_line_gap{ 0.0f };
		float m_line_height{ 0.0f };
		std::unique_ptr<stbtt_packedchar[ ]> m_packed_char_data{};

		mutable ankerl::unordered_dense::map<char, glyph_cache_entry> m_glyph_cache{};
		mutable ankerl::unordered_dense::map<std::string, std::pair<float, float>> m_text_size_cache{};

		font( ) = default;
		font( const font& ) = delete;
		font& operator=( const font& ) = delete;
		font( font&& ) = default;
		font& operator=( font&& ) = default;

		[[nodiscard]] const glyph_cache_entry& get_glyph( char c ) const;
		void calc_text_size( std::string_view text, float& width, float& height ) const;
		void clear_caches( ) const noexcept;
	};

	namespace text_styles
	{
		struct normal { };
		struct outlined { };
		struct shadowed { };
	}

	[[nodiscard]] bool initialize( ID3D11Device* device, ID3D11DeviceContext* context );

	void begin_frame( );
	void end_frame( );

	[[nodiscard]] draw_list& get_draw_list( ) noexcept;
	[[nodiscard]] std::pair<int, int> get_display_size( ) noexcept;

	[[nodiscard]] Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> load_texture_from_memory( std::span<const std::byte> data, int* out_width = nullptr, int* out_height = nullptr );
	[[nodiscard]] Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> load_texture_from_file( std::string_view filepath, int* out_width = nullptr, int* out_height = nullptr );

	[[nodiscard]] font* load_font_from_memory( std::span<const std::byte> font_data, float size_pixels, int atlas_width = 512, int atlas_height = 512 );
	[[nodiscard]] font* load_font_from_file( std::string_view filepath, float size_pixels, int atlas_width = 512, int atlas_height = 512 );
	[[nodiscard]] font* get_normal_font( ) noexcept;

	void push_clip_rect( float x0, float y0, float x1, float y1 );
	void pop_clip_rect( );

	void line( float x0, float y0, float x1, float y1, rgba color, float thickness = 1.0f );
	void rect( float x, float y, float w, float h, rgba color, float thickness = 1.0f );
	void rect_cornered( float x, float y, float w, float h, rgba color, float corner_length = 15.0f, float thickness = 1.0f );
	void rect_filled( float x, float y, float w, float h, rgba color );
	void rect_filled_multi_color( float x, float y, float w, float h, rgba color_tl, rgba color_tr, rgba color_br, rgba color_bl );
	void rect_textured( float x, float y, float w, float h, ID3D11ShaderResourceView* tex, float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f );
	void convex_poly_filled( std::span<const float> points, rgba color );
	void polyline( std::span<const float> points, rgba color, bool closed = false, float thickness = 1.0f );
	void circle( float x, float y, float radius, rgba color, int segments = 32, float thickness = 1.0f );
	void circle_filled( float x, float y, float radius, rgba color, int segments = 32 );

	template <typename Style = text_styles::normal>
	void text( float x, float y, std::string_view str, rgba color, const font* fnt = nullptr )
	{
		const font* f{ fnt != nullptr ? fnt : get_normal_font( ) };

		if constexpr ( std::is_same_v<Style, text_styles::normal> )
		{
			get_draw_list( ).add_text( x, y, str, f, color );
		}
		else if constexpr ( std::is_same_v<Style, text_styles::outlined> )
		{
			constexpr float offsets[ 8 ][ 2 ]{ {-1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f} };
			constexpr rgba shadow_col{ 0, 0, 0, 235 };

			for ( int i{ 0 }; i < 8; ++i )
			{
				get_draw_list( ).add_text( x + offsets[ i ][ 0 ], y + offsets[ i ][ 1 ], str, f, shadow_col );
			}

			get_draw_list( ).add_text( x, y, str, f, color );
		}
		else if constexpr ( std::is_same_v<Style, text_styles::shadowed> )
		{
			constexpr auto shadow_offset{ 1.0f };
			constexpr rgba shadow_col{ 0, 0, 0, 235 };

			get_draw_list( ).add_text( x + shadow_offset, y + shadow_offset, str, f, shadow_col );
			get_draw_list( ).add_text( x, y, str, f, color );
		}
		else
		{
			get_draw_list( ).add_text( x, y, str, f, color );
		}
	}

	inline void text_outlined( float x, float y, std::string_view str, rgba color, const font* fnt = nullptr )
	{
		text<text_styles::outlined>( x, y, str, color, fnt );
	}

	inline void text_shadowed( float x, float y, std::string_view str, rgba color, const font* fnt = nullptr )
	{
		text<text_styles::shadowed>( x, y, str, color, fnt );
	}

	[[nodiscard]] std::pair<float, float> measure_text( std::string_view text, const font* fnt = nullptr );

} // namespace zdraw

#endif // ZDRAW_HPP