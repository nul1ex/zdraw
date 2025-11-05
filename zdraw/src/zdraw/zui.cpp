#include <include/zdraw/zdraw.hpp>
#include <include/zdraw/zui.hpp>

#include <algorithm>
#include <chrono>

namespace zui {

	namespace detail {

		template<typename>
		class callable;

		template<typename ret_, typename... args_>
		class callable<ret_( args_... )>
		{
		private:
			using invoke_stub_t = ret_( * )( void*, args_... );
			using destroy_stub_t = void( * )( void* );

			void* m_context{ nullptr };
			invoke_stub_t m_invoke_stub{ nullptr };
			destroy_stub_t m_destroy_stub{ nullptr };

			template<typename F>
			static ret_ invoke_impl( void* context, args_... args )
			{
				return ( *static_cast< F* >( context ) )( std::forward<args_>( args )... );
			}

			template<typename F>
			static void destroy_impl( void* context )
			{
				static_cast< F* >( context )->~F( );
				::operator delete( context );
			}

		public:
			callable( ) = default;

			template<typename F>
			callable( F&& func )
			{
				m_context = ::operator new( sizeof( F ) );
				if ( m_context )
				{
					new ( m_context ) F( std::move( func ) );
					m_invoke_stub = &invoke_impl<F>;
					m_destroy_stub = &destroy_impl<F>;
				}
			}

			~callable( )
			{
				if ( m_context && m_destroy_stub )
				{
					m_destroy_stub( m_context );
				}
			}

			callable( callable&& other ) noexcept : m_context( other.m_context ), m_invoke_stub( other.m_invoke_stub ), m_destroy_stub( other.m_destroy_stub )
			{
				other.m_context = nullptr;
				other.m_invoke_stub = nullptr;
				other.m_destroy_stub = nullptr;
			}

			callable& operator=( callable&& other ) noexcept
			{
				if ( this != &other )
				{
					if ( m_context && m_destroy_stub )
					{
						m_destroy_stub( m_context );
					}

					m_context = other.m_context;
					m_invoke_stub = other.m_invoke_stub;
					m_destroy_stub = other.m_destroy_stub;

					other.m_context = nullptr;
					other.m_invoke_stub = nullptr;
					other.m_destroy_stub = nullptr;
				}

				return *this;
			}

			callable( const callable& ) = delete;
			callable& operator=( const callable& ) = delete;

			ret_ operator()( args_... args ) const
			{
				if ( m_invoke_stub )
				{
					return m_invoke_stub( m_context, std::forward<args_>( args )... );
				}
				if constexpr ( !std::is_void_v<ret_> )
				{
					return ret_{};
				}
			}

			explicit operator bool( ) const { return m_invoke_stub != nullptr; }
		};

		struct input_state
		{
			float m_x{ 0.0f };
			float m_y{ 0.0f };
			bool m_down{ false };
			bool m_clicked{ false };
			bool m_released{ false };
		};

		struct window_state
		{
			std::string m_title{};
			rect m_rect{};
			float m_cursor_x{ 0.0f };
			float m_cursor_y{ 0.0f };
			float m_line_h{ 0.0f };
			rect m_last_item{};
			bool m_is_child{ false };
		};

		struct style_var_backup
		{
			style_var var{};
			float prev{};
		};

		struct style_color_backup
		{
			style_color idx{};
			zdraw::rgba prev{};
		};

		struct context
		{
			HWND m_hwnd{ nullptr };
			input_state m_input{};
			input_state m_prev_input{};
			std::vector<window_state> m_windows{};
			std::vector<std::uintptr_t> m_id_stack{};
			std::uintptr_t m_active_window_id{ 0 };
			std::uintptr_t m_active_slider_id{ 0 };
			std::uintptr_t m_active_keybind_id{ 0 };
			zui::style m_style{};
			std::vector<style_var_backup> m_style_var_stack{};
			std::vector<style_color_backup> m_style_color_stack{};
			std::uintptr_t m_active_combo_id{ 0 };
			std::vector<callable<void( )>> m_combo_overlay_list{};
			bool m_overlay_consuming_input{ false };

			float m_delta_time{ 0.0f };
			std::chrono::high_resolution_clock::time_point m_last_frame_time{};
		};

		static context g_ctx{};

		static std::uintptr_t fnv1a64( const void* p, std::size_t n )
		{
			const auto* b = static_cast< const unsigned char* >( p );
			auto h = 14695981039346656037ull;

			while ( n-- )
			{
				h ^= *b++;
				h *= 1099511628211ull;
			}

			return h;
		}

		static std::uintptr_t hash_str( std::string_view s )
		{
			return fnv1a64( s.data( ), s.size( ) );
		}

		static void update_input( )
		{
			POINT pt{};
			GetCursorPos( &pt );

			if ( g_ctx.m_hwnd )
			{
				ScreenToClient( g_ctx.m_hwnd, &pt );
			}

			const auto down = ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) != 0;
			g_ctx.m_input.m_x = static_cast< float >( pt.x );
			g_ctx.m_input.m_y = static_cast< float >( pt.y );
			g_ctx.m_input.m_clicked = down && !g_ctx.m_prev_input.m_down;
			g_ctx.m_input.m_released = !down && g_ctx.m_prev_input.m_down;
			g_ctx.m_input.m_down = down;
		}

		static bool mouse_in_rect( const rect& r )
		{
			return g_ctx.m_input.m_x >= r.m_x && g_ctx.m_input.m_x <= r.m_x + r.m_w && g_ctx.m_input.m_y >= r.m_y && g_ctx.m_input.m_y <= r.m_y + r.m_h;
		}

		static window_state* current_window( )
		{
			if ( g_ctx.m_windows.empty( ) )
			{
				return nullptr;
			}

			return &g_ctx.m_windows.back( );
		}

		static rect item_add( float w, float h )
		{
			auto win = current_window( );
			if ( !win )
			{
				return { 0,0,0,0 };
			}

			if ( win->m_line_h > 0.0f )
			{
				win->m_cursor_y += win->m_line_h + g_ctx.m_style.item_spacing_y;
			}

			rect r{ win->m_cursor_x, win->m_cursor_y, w, h };
			win->m_last_item = r;
			win->m_line_h = h;
			return r;
		}

		static rect item_rect_abs( const rect& local )
		{
			auto win = current_window( );
			if ( !win )
			{
				return local;
			}

			return { win->m_rect.m_x + local.m_x, win->m_rect.m_y + local.m_y, local.m_w, local.m_h };
		}

		static zdraw::rgba& style_color_ref( style_color idx )
		{
			switch ( idx )
			{
			case style_color::window_bg: return g_ctx.m_style.window_bg;
			case style_color::window_border: return g_ctx.m_style.window_border;
			case style_color::nested_bg: return g_ctx.m_style.nested_bg;
			case style_color::nested_border: return g_ctx.m_style.nested_border;
			case style_color::group_box_bg: return g_ctx.m_style.group_box_bg;
			case style_color::group_box_border: return g_ctx.m_style.group_box_border;
			case style_color::group_box_title_text: return g_ctx.m_style.group_box_title_text;
			case style_color::checkbox_bg: return g_ctx.m_style.checkbox_bg;
			case style_color::checkbox_border: return g_ctx.m_style.checkbox_border;
			case style_color::checkbox_check: return g_ctx.m_style.checkbox_check;
			case style_color::slider_bg: return g_ctx.m_style.slider_bg;
			case style_color::slider_border: return g_ctx.m_style.slider_border;
			case style_color::slider_fill: return g_ctx.m_style.slider_fill;
			case style_color::slider_grab: return g_ctx.m_style.slider_grab;
			case style_color::slider_grab_active: return g_ctx.m_style.slider_grab_active;
			case style_color::button_bg: return g_ctx.m_style.button_bg;
			case style_color::button_border: return g_ctx.m_style.button_border;
			case style_color::button_hovered: return g_ctx.m_style.button_hovered;
			case style_color::button_active: return g_ctx.m_style.button_active;
			case style_color::keybind_bg: return g_ctx.m_style.keybind_bg;
			case style_color::keybind_border: return g_ctx.m_style.keybind_border;
			case style_color::keybind_waiting: return g_ctx.m_style.keybind_waiting;
			case style_color::combo_bg: return g_ctx.m_style.combo_bg;
			case style_color::combo_border: return g_ctx.m_style.combo_border;
			case style_color::combo_arrow: return g_ctx.m_style.combo_arrow;
			case style_color::combo_hovered: return g_ctx.m_style.combo_hovered;
			case style_color::combo_popup_bg: return g_ctx.m_style.combo_popup_bg;
			case style_color::combo_popup_border: return g_ctx.m_style.combo_popup_border;
			case style_color::combo_item_hovered: return g_ctx.m_style.combo_item_hovered;
			case style_color::combo_item_selected: return g_ctx.m_style.combo_item_selected;
			case style_color::text: return g_ctx.m_style.text;
			default: return g_ctx.m_style.window_bg;
			}
		}

		static float* style_var_ref( style_var var )
		{
			switch ( var )
			{
			case style_var::window_padding_x: return &g_ctx.m_style.window_padding_x;
			case style_var::window_padding_y: return &g_ctx.m_style.window_padding_y;
			case style_var::item_spacing_x: return &g_ctx.m_style.item_spacing_x;
			case style_var::item_spacing_y: return &g_ctx.m_style.item_spacing_y;
			case style_var::frame_padding_x: return &g_ctx.m_style.frame_padding_x;
			case style_var::frame_padding_y: return &g_ctx.m_style.frame_padding_y;
			case style_var::window_rounding: return &g_ctx.m_style.window_rounding;
			case style_var::border_thickness: return &g_ctx.m_style.border_thickness;
			case style_var::group_box_title_height: return &g_ctx.m_style.group_box_title_height;
			case style_var::checkbox_size: return &g_ctx.m_style.checkbox_size;
			case style_var::slider_height: return &g_ctx.m_style.slider_height;
			case style_var::keybind_height: return &g_ctx.m_style.keybind_height;
			case style_var::combo_height: return &g_ctx.m_style.combo_height;
			case style_var::combo_item_height: return &g_ctx.m_style.combo_item_height;
			default: return nullptr;
			}
		}

		static zdraw::rgba lighten_color( const zdraw::rgba& color, float factor )
		{
			return zdraw::rgba
			{
				static_cast< std::uint8_t >( std::min( color.r * factor, 255.0f ) ),
				static_cast< std::uint8_t >( std::min( color.g * factor, 255.0f ) ),
				static_cast< std::uint8_t >( std::min( color.b * factor, 255.0f ) ),
				color.a
			};
		}

		static zdraw::rgba darken_color( const zdraw::rgba& color, float factor )
		{
			return zdraw::rgba
			{
				static_cast< std::uint8_t >( color.r * factor ),
				static_cast< std::uint8_t >( color.g * factor ),
				static_cast< std::uint8_t >( color.b * factor ),
				color.a
			};
		}

	} // namespace detail

	bool initialize( HWND hwnd )
	{
		detail::g_ctx.m_hwnd = hwnd;
		return hwnd != nullptr;
	}

	void begin( )
	{
		detail::g_ctx.m_prev_input = detail::g_ctx.m_input;
		detail::update_input( );

		const auto current_time = std::chrono::high_resolution_clock::now( );
		if ( detail::g_ctx.m_last_frame_time.time_since_epoch( ).count( ) != 0 )
		{
			detail::g_ctx.m_delta_time = std::chrono::duration<float>( current_time - detail::g_ctx.m_last_frame_time ).count( );
		}
		else
		{
			detail::g_ctx.m_delta_time = 1.0f / 60.0f;
		}

		detail::g_ctx.m_last_frame_time = current_time;

		detail::g_ctx.m_overlay_consuming_input = false;
		detail::g_ctx.m_windows.clear( );
		detail::g_ctx.m_id_stack.clear( );
	}

	void end( )
	{
		for ( auto& overlay : detail::g_ctx.m_combo_overlay_list )
		{
			overlay( );
		}

		detail::g_ctx.m_combo_overlay_list.clear( );

		if ( detail::g_ctx.m_input.m_released )
		{
			detail::g_ctx.m_active_window_id = 0;
			detail::g_ctx.m_active_slider_id = 0;
		}
	}

	bool begin_window( std::string_view title, float& x, float& y, float w, float h )
	{
		const auto id = detail::hash_str( title );
		auto abs = rect{ x, y, w, h };
		const auto hovered = detail::mouse_in_rect( abs );

		if ( hovered && detail::g_ctx.m_input.m_clicked && detail::g_ctx.m_active_window_id == 0 && detail::g_ctx.m_active_slider_id == 0 )
		{
			detail::g_ctx.m_active_window_id = id;
		}

		if ( detail::g_ctx.m_active_window_id == id && detail::g_ctx.m_input.m_down && detail::g_ctx.m_active_slider_id == 0 )
		{
			const auto dx = detail::g_ctx.m_input.m_x - detail::g_ctx.m_prev_input.m_x;
			const auto dy = detail::g_ctx.m_input.m_y - detail::g_ctx.m_prev_input.m_y;
			x += dx;
			y += dy;
			abs.m_x = x;
			abs.m_y = y;
		}

		detail::g_ctx.m_windows.emplace_back( );
		auto win = detail::current_window( );

		win->m_title.assign( title.begin( ), title.end( ) );
		win->m_rect = abs;
		win->m_cursor_x = detail::g_ctx.m_style.window_padding_x;
		win->m_cursor_y = detail::g_ctx.m_style.window_padding_y;
		win->m_line_h = 0.0f;
		win->m_last_item = { 0,0,0,0 };
		win->m_is_child = false;

		const auto color_tl = detail::g_ctx.m_style.window_bg;
		const auto color_br = detail::darken_color( detail::g_ctx.m_style.window_bg, 0.7f );
		zdraw::rect_filled_multi_color( abs.m_x, abs.m_y, abs.m_w, abs.m_h, color_tl, color_tl, color_br, color_br );

		if ( detail::g_ctx.m_style.border_thickness > 0.0f )
		{
			zdraw::rect( abs.m_x, abs.m_y, abs.m_w, abs.m_h, detail::g_ctx.m_style.window_border, detail::g_ctx.m_style.border_thickness );
		}

		zdraw::push_clip_rect( abs.m_x, abs.m_y, abs.m_x + abs.m_w, abs.m_y + abs.m_h );
		detail::g_ctx.m_id_stack.push_back( id );
		return true;
	}

	void end_window( )
	{
		if ( !detail::g_ctx.m_windows.empty( ) )
		{
			zdraw::pop_clip_rect( );
			detail::g_ctx.m_windows.pop_back( );
		}

		if ( !detail::g_ctx.m_id_stack.empty( ) )
		{
			detail::g_ctx.m_id_stack.pop_back( );
		}
	}

	bool begin_nested_window( std::string_view title, float w, float h )
	{
		auto parent = detail::current_window( );
		if ( !parent )
		{
			return false;
		}

		auto local = detail::item_add( w, h );
		auto abs = rect{ parent->m_rect.m_x + local.m_x, parent->m_rect.m_y + local.m_y, w, h };

		detail::g_ctx.m_windows.emplace_back( );

		auto win = detail::current_window( );

		win->m_title.assign( title.begin( ), title.end( ) );
		win->m_rect = abs;
		win->m_cursor_x = detail::g_ctx.m_style.window_padding_x;
		win->m_cursor_y = detail::g_ctx.m_style.window_padding_y;
		win->m_line_h = 0.0f;
		win->m_last_item = { 0,0,0,0 };
		win->m_is_child = false;

		const auto color_tl = detail::g_ctx.m_style.nested_bg;
		const auto color_br = detail::darken_color( detail::g_ctx.m_style.nested_bg, 0.8f );
		zdraw::rect_filled_multi_color( abs.m_x, abs.m_y, abs.m_w, abs.m_h, color_tl, color_tl, color_br, color_br );

		if ( detail::g_ctx.m_style.border_thickness > 0.0f )
		{
			zdraw::rect( abs.m_x, abs.m_y, abs.m_w, abs.m_h, detail::g_ctx.m_style.nested_border, detail::g_ctx.m_style.border_thickness );
		}

		zdraw::push_clip_rect( abs.m_x, abs.m_y, abs.m_x + abs.m_w, abs.m_y + abs.m_h );

		detail::g_ctx.m_id_stack.push_back( detail::hash_str( title ) );
		return true;
	}

	void end_nested_window( )
	{
		end_window( );
	}

	bool begin_group_box( std::string_view title, float w, float h )
	{
		auto parent = detail::current_window( );
		if ( !parent )
		{
			return false;
		}

		const auto title_h = detail::g_ctx.m_style.group_box_title_height;
		auto local = detail::item_add( w, h );
		auto abs = rect{ parent->m_rect.m_x + local.m_x, parent->m_rect.m_y + local.m_y, w, h };

		const auto bg_col = detail::g_ctx.m_style.group_box_bg;
		const auto border_col = detail::g_ctx.m_style.group_box_border;
		const auto thickness = detail::g_ctx.m_style.border_thickness;

		const auto border_y = abs.m_y + title_h * 0.5f;

		zdraw::rect_filled( abs.m_x, border_y, abs.m_w, abs.m_h - title_h * 0.5f, bg_col );
		zdraw::rect( abs.m_x, border_y, abs.m_w, abs.m_h - title_h * 0.5f, border_col, thickness );

		if ( !title.empty( ) )
		{
			auto [title_w, title_h_measured] = zdraw::measure_text( title );
			const auto text_x = abs.m_x + detail::g_ctx.m_style.window_padding_x;
			const auto text_y = abs.m_y;
			const auto pad = 4.0f;

			const auto gap_start = text_x - pad;
			const auto gap_end = text_x + title_w + pad;

			zdraw::line( gap_start, border_y, gap_end, border_y, bg_col, thickness );

			std::string title_str( title.begin( ), title.end( ) );
			zdraw::text( text_x, text_y + 2.0f, title_str, detail::g_ctx.m_style.group_box_title_text );
		}

		detail::g_ctx.m_windows.emplace_back( );
		auto win = detail::current_window( );

		win->m_title.assign( title.begin( ), title.end( ) );
		win->m_rect = abs;
		win->m_cursor_x = detail::g_ctx.m_style.window_padding_x;
		win->m_cursor_y = title_h + detail::g_ctx.m_style.window_padding_y;
		win->m_line_h = 0.0f;
		win->m_last_item = { 0, 0, 0, 0 };
		win->m_is_child = true;

		zdraw::push_clip_rect( abs.m_x, abs.m_y + title_h * 0.5f, abs.m_x + abs.m_w, abs.m_y + abs.m_h );
		detail::g_ctx.m_id_stack.push_back( detail::hash_str( title ) );

		return true;
	}

	void end_group_box( )
	{
		end_window( );
	}

	std::pair<float, float> get_cursor_pos( )
	{
		auto win = detail::current_window( );
		if ( !win )
		{
			return { 0.0f,0.0f };
		}

		return { win->m_cursor_x, win->m_cursor_y };
	}

	void set_cursor_pos( float x, float y )
	{
		auto win = detail::current_window( );
		if ( !win )
		{
			return;
		}

		win->m_cursor_x = x;
		win->m_cursor_y = y;
		win->m_line_h = 0.0f;
	}

	std::pair<float, float> get_content_region_avail( )
	{
		auto win = detail::current_window( );
		if ( !win )
		{
			return { 0.0f, 0.0f };
		}

		const auto pad_x = detail::g_ctx.m_style.window_padding_x;
		const auto pad_y = detail::g_ctx.m_style.window_padding_y;
		const auto work_max_x = win->m_rect.m_w - pad_x;
		const auto work_max_y = win->m_rect.m_h - pad_y;

		auto next_x = win->m_cursor_x;
		auto next_y = win->m_cursor_y;

		if ( win->m_line_h > 0.0f )
		{
			next_y += win->m_line_h + detail::g_ctx.m_style.item_spacing_y;
		}

		auto avail_w = std::max( 0.0f, work_max_x - next_x );
		auto avail_h = std::max( 0.0f, work_max_y - next_y );
		return { avail_w, avail_h };
	}

	float calc_item_width( int count )
	{
		auto [avail_w, avail_h] = get_content_region_avail( );

		if ( count <= 0 )
		{
			return avail_w;
		}

		const auto spacing = detail::g_ctx.m_style.item_spacing_x;
		return ( avail_w - spacing * ( count - 1 ) ) / static_cast< float >( count );
	}

	style& get_style( )
	{
		return detail::g_ctx.m_style;
	}

	void push_style_var( style_var var, float value )
	{
		if ( auto* p = detail::style_var_ref( var ) )
		{
			detail::style_var_backup backup{};
			backup.var = var;
			backup.prev = *p;
			detail::g_ctx.m_style_var_stack.push_back( backup );
			*p = value;
		}
	}

	void pop_style_var( int count )
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( detail::g_ctx.m_style_var_stack.empty( ) )
			{
				break;
			}

			auto backup = detail::g_ctx.m_style_var_stack.back( );

			detail::g_ctx.m_style_var_stack.pop_back( );

			if ( auto* p = detail::style_var_ref( backup.var ) )
			{
				*p = backup.prev;
			}
		}
	}

	void push_style_color( style_color idx, const zdraw::rgba& col )
	{
		auto& ref = detail::style_color_ref( idx );
		detail::style_color_backup backup{ idx, ref };
		detail::g_ctx.m_style_color_stack.push_back( backup );
		ref = col;
	}

	void pop_style_color( int count )
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( detail::g_ctx.m_style_color_stack.empty( ) )
			{
				break;
			}

			auto backup = detail::g_ctx.m_style_color_stack.back( );
			detail::g_ctx.m_style_color_stack.pop_back( );
			auto& ref = detail::style_color_ref( backup.idx );
			ref = backup.prev;
		}
	}

	void same_line( float offset_x )
	{
		auto win = detail::current_window( );
		if ( !win )
		{
			return;
		}

		if ( win->m_line_h > 0.0f )
		{
			const auto spacing = ( offset_x == 0.0f ) ? detail::g_ctx.m_style.item_spacing_x : offset_x;
			win->m_cursor_x = win->m_last_item.m_x + win->m_last_item.m_w + spacing;
			win->m_cursor_y = win->m_last_item.m_y;
			win->m_line_h = 0.0f;
		}
	}

	bool checkbox( std::string_view label, bool& v )
	{
		auto win = detail::current_window( );
		if ( !win )
		{
			return false;
		}

		static std::unordered_map<std::uintptr_t, float> anim_states{};

		const auto id = detail::hash_str( label );
		auto& anim = anim_states[ id ];

		constexpr auto check_size = 14.0f;
		auto local = detail::item_add( check_size, check_size );
		auto abs = detail::item_rect_abs( local );
		const auto hovered = detail::mouse_in_rect( abs );
		auto changed = false;

		if ( hovered && detail::g_ctx.m_input.m_clicked )
		{
			v = !v;
			changed = true;
		}

		const auto anim_speed = 12.0f * detail::g_ctx.m_delta_time;
		const auto target = v ? 1.0f : 0.0f;
		anim = anim + ( target - anim ) * anim_speed;

		const auto bg_col = detail::g_ctx.m_style.checkbox_bg;
		const auto border_col = hovered ? detail::lighten_color( detail::g_ctx.m_style.checkbox_border, 1.15f ) : detail::g_ctx.m_style.checkbox_border;
		const auto check_col = detail::g_ctx.m_style.checkbox_check;

		zdraw::rect_filled( abs.m_x, abs.m_y, abs.m_w, abs.m_h, bg_col );
		zdraw::rect( abs.m_x, abs.m_y, abs.m_w, abs.m_h, border_col, 1.0f );

		if ( anim > 0.01f )
		{
			constexpr auto pad = 2.0f;
			const auto fill_x = abs.m_x + pad;
			const auto fill_y = abs.m_y + pad;
			const auto fill_w = abs.m_w - pad * 2.0f;
			const auto fill_h = abs.m_h - pad * 2.0f;

			const auto col_top = zdraw::rgba
			{
				static_cast< std::uint8_t >( std::min( check_col.r * 1.2f, 255.0f ) ),
				static_cast< std::uint8_t >( std::min( check_col.g * 1.2f, 255.0f ) ),
				static_cast< std::uint8_t >( std::min( check_col.b * 1.2f, 255.0f ) ),
				static_cast< std::uint8_t >( check_col.a * anim )
			};

			const auto col_bottom = zdraw::rgba
			{
				static_cast< std::uint8_t >( check_col.r * 0.8f ),
				static_cast< std::uint8_t >( check_col.g * 0.8f ),
				static_cast< std::uint8_t >( check_col.b * 0.8f ),
				static_cast< std::uint8_t >( check_col.a * anim )
			};

			zdraw::rect_filled_multi_color( fill_x, fill_y, fill_w, fill_h, col_top, col_top, col_bottom, col_bottom );
		}

		if ( !label.empty( ) )
		{
			const auto text_x = abs.m_x + abs.m_w + detail::g_ctx.m_style.item_spacing_x;
			const auto text_y = abs.m_y + ( check_size - 12.0f ) * 0.5f;

			std::string s( label.begin( ), label.end( ) );
			zdraw::text_outlined( text_x, text_y, s, detail::g_ctx.m_style.text );
		}

		return changed;
	}

	bool button( std::string_view label, float w, float h )
	{
		auto win = detail::current_window( );
		if ( !win )
		{
			return false;
		}

		auto local = detail::item_add( w, h );
		auto abs = detail::item_rect_abs( local );

		const auto hovered = detail::mouse_in_rect( abs );
		const auto held = hovered && detail::g_ctx.m_input.m_down;
		const auto pressed = hovered && detail::g_ctx.m_input.m_clicked;

		const auto bg_col = held ? detail::g_ctx.m_style.button_active : ( hovered ? detail::g_ctx.m_style.button_hovered : detail::g_ctx.m_style.button_bg );
		const auto border_col = hovered ? detail::lighten_color( detail::g_ctx.m_style.button_border, 1.2f ) : detail::g_ctx.m_style.button_border;

		const auto col_top = bg_col;
		const auto col_bottom = detail::darken_color( bg_col, 0.85f );
		zdraw::rect_filled_multi_color( abs.m_x, abs.m_y, abs.m_w, abs.m_h, col_top, col_top, col_bottom, col_bottom );
		zdraw::rect( abs.m_x, abs.m_y, abs.m_w, abs.m_h, border_col, 1.0f );

		if ( !label.empty( ) )
		{
			auto [label_w, label_h] = zdraw::measure_text( label );
			const auto text_x = abs.m_x + ( abs.m_w - label_w ) * 0.5f;
			const auto text_y = abs.m_y + ( abs.m_h - label_h ) * 0.5f + 3.0f;

			std::string label_str( label.begin( ), label.end( ) );
			zdraw::text( text_x, text_y, label_str, detail::g_ctx.m_style.text );
		}

		return pressed;
	}

	bool keybind( std::string_view label, int& key )
	{
		auto win = detail::current_window( );
		if ( !win )
		{
			return false;
		}

		const auto id = detail::hash_str( label );
		const auto is_waiting = ( detail::g_ctx.m_active_keybind_id == id );

		char button_text[ 64 ]{};
		if ( is_waiting )
		{
			std::snprintf( button_text, sizeof( button_text ), "..." );
		}
		else if ( key == 0 )
		{
			std::snprintf( button_text, sizeof( button_text ), "none" );
		}
		else
		{
			const char* key_name = "unknown";

			if ( key >= 0x41 && key <= 0x5A )
			{
				std::snprintf( button_text, sizeof( button_text ), "%c", key );
			}
			else if ( key >= 0x30 && key <= 0x39 )
			{
				std::snprintf( button_text, sizeof( button_text ), "%c", key );
			}
			else
			{
				if ( key == VK_LBUTTON ) key_name = "lmb";
				else if ( key == VK_RBUTTON ) key_name = "rmb";
				else if ( key == VK_MBUTTON ) key_name = "mmb";
				else if ( key == VK_XBUTTON1 ) key_name = "mb4";
				else if ( key == VK_XBUTTON2 ) key_name = "mb5";
				else if ( key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT ) key_name = "shift";
				else if ( key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL ) key_name = "ctrl";
				else if ( key == VK_MENU || key == VK_LMENU || key == VK_RMENU ) key_name = "alt";
				else if ( key == VK_SPACE ) key_name = "space";
				else if ( key == VK_RETURN ) key_name = "enter";
				else if ( key == VK_ESCAPE ) key_name = "esc";
				else if ( key == VK_TAB ) key_name = "tab";
				else if ( key == VK_CAPITAL ) key_name = "caps";
				else if ( key == VK_INSERT ) key_name = "insert";
				else if ( key == VK_DELETE ) key_name = "delete";
				else if ( key == VK_HOME ) key_name = "home";
				else if ( key == VK_END ) key_name = "end";
				else if ( key == VK_PRIOR ) key_name = "pgup";
				else if ( key == VK_NEXT ) key_name = "pgdn";
				else if ( key == VK_LEFT ) key_name = "left";
				else if ( key == VK_RIGHT ) key_name = "right";
				else if ( key == VK_UP ) key_name = "up";
				else if ( key == VK_DOWN ) key_name = "down";

				if ( key_name != "unknown" )
				{
					std::snprintf( button_text, sizeof( button_text ), "%s", key_name );
				}
			}
		}

		auto [label_w, label_h] = zdraw::measure_text( label );
		auto [button_text_w, button_text_h] = zdraw::measure_text( button_text );

		const auto button_width = std::max( 60.0f, button_text_w + detail::g_ctx.m_style.frame_padding_x * 2.0f );
		const auto button_height = detail::g_ctx.m_style.keybind_height;

		const auto total_w = button_width + detail::g_ctx.m_style.item_spacing_x + label_w;
		const auto total_h = std::max( button_height, label_h );

		auto local = detail::item_add( total_w, total_h );
		auto abs = detail::item_rect_abs( local );

		const auto button_rect = rect{ abs.m_x, abs.m_y, button_width, button_height };
		const auto hovered = detail::mouse_in_rect( button_rect );
		const auto held = hovered && detail::g_ctx.m_input.m_down;
		const auto pressed = hovered && detail::g_ctx.m_input.m_clicked;

		if ( pressed )
		{
			detail::g_ctx.m_active_keybind_id = id;
		}

		const auto bg_col = detail::g_ctx.m_style.keybind_bg;
		const auto border_col = held && hovered ? detail::lighten_color( detail::g_ctx.m_style.keybind_border, 1.3f ) : ( hovered ? detail::lighten_color( detail::g_ctx.m_style.keybind_border, 1.15f ) : detail::g_ctx.m_style.keybind_border );

		zdraw::rect_filled( button_rect.m_x, button_rect.m_y, button_rect.m_w, button_rect.m_h, bg_col );
		zdraw::rect( button_rect.m_x, button_rect.m_y, button_rect.m_w, button_rect.m_h, border_col, 1.0f );

		if ( is_waiting )
		{
			constexpr auto pad = 2.0f;
			const auto fill_x = button_rect.m_x + pad;
			const auto fill_y = button_rect.m_y + pad;
			const auto fill_w = button_rect.m_w - pad * 2.0f;
			const auto fill_h = button_rect.m_h - pad * 2.0f;

			const auto wait_col = detail::g_ctx.m_style.keybind_waiting;
			const auto col_left = detail::lighten_color( wait_col, 1.2f );
			const auto col_right = detail::darken_color( wait_col, 0.8f );

			auto col_left_alpha = col_left;
			auto col_right_alpha = col_right;
			col_left_alpha.a = static_cast< std::uint8_t >( col_left.a * 0.3f );
			col_right_alpha.a = static_cast< std::uint8_t >( col_right.a * 0.3f );

			zdraw::rect_filled_multi_color( fill_x, fill_y, fill_w, fill_h, col_left_alpha, col_right_alpha, col_right_alpha, col_left_alpha );
		}

		const auto text_x = button_rect.m_x + ( button_rect.m_w - button_text_w ) * 0.5f;
		const auto text_y = button_rect.m_y + ( button_rect.m_h - button_text_h ) * 0.5f;
		zdraw::text( text_x, text_y + 3.5f, button_text, detail::g_ctx.m_style.text );

		if ( !label.empty( ) )
		{
			const auto label_x = button_rect.m_x + button_rect.m_w + detail::g_ctx.m_style.item_spacing_x;
			const auto label_y = button_rect.m_y + ( button_height - label_h ) * 0.5f;

			std::string label_str( label.begin( ), label.end( ) );
			zdraw::text( label_x, label_y + 3.5f, label_str, detail::g_ctx.m_style.text );
		}

		if ( is_waiting )
		{
			if ( detail::g_ctx.m_input.m_clicked && !hovered )
			{
				key = VK_LBUTTON;
				detail::g_ctx.m_active_keybind_id = 0;
				return true;
			}

			static const int modifier_keys[ ] = { VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU };
			for ( const auto mod_key : modifier_keys )
			{
				if ( GetAsyncKeyState( mod_key ) & 0x8000 )
				{
					key = mod_key;
					detail::g_ctx.m_active_keybind_id = 0;
					return true;
				}
			}

			for ( int vk = 0x01; vk < 0xFF; ++vk )
			{
				if ( vk == VK_LBUTTON )
				{
					continue;
				}

				if ( 
					vk == VK_SHIFT || vk == VK_CONTROL || 
					vk == VK_MENU ||
					vk == VK_LSHIFT || vk == VK_RSHIFT ||
					vk == VK_LCONTROL || vk == VK_RCONTROL ||
					vk == VK_LMENU || vk == VK_RMENU
					)
				{
					continue;
				}

				if ( GetAsyncKeyState( vk ) & 0x8000 )
				{
					if ( vk == VK_ESCAPE )
					{
						key = 0;
					}
					else
					{
						key = vk;
					}

					detail::g_ctx.m_active_keybind_id = 0;
					return true;
				}
			}
		}

		return false;
	}

	template<typename T>
	bool slider_scalar( std::string_view label, T& v, T v_min, T v_max, std::string_view format )
	{
		auto win = detail::current_window( );
		if ( !win )
		{
			return false;
		}

		static std::unordered_map<std::uintptr_t, float> value_anim_states{};

		const auto id = detail::hash_str( label );
		auto& value_anim = value_anim_states[ id ];

		auto [avail_w, avail_h] = get_content_region_avail( );
		const auto slider_width = avail_w;

		char value_buf[ 64 ]{ };
		if constexpr ( std::is_same_v<T, float> || std::is_same_v<T, double> )
		{
			std::snprintf( value_buf, sizeof( value_buf ), format.data( ), v );
		}
		else
		{
			std::snprintf( value_buf, sizeof( value_buf ), "%d", static_cast< int >( v ) );
		}

		auto [label_w, label_h] = zdraw::measure_text( label );
		auto [value_w, value_h] = zdraw::measure_text( value_buf );

		const auto text_height = std::max( label_h, value_h );
		const auto slider_height = detail::g_ctx.m_style.slider_height;
		const auto total_height = text_height + slider_height - 2.0f;

		auto local = detail::item_add( slider_width, total_height );
		auto abs = detail::item_rect_abs( local );

		const auto frame_y = abs.m_y + text_height - 2.0f;
		const auto frame_rect = rect{ abs.m_x, frame_y, slider_width, slider_height };

		const auto hovered = detail::mouse_in_rect( frame_rect );
		auto changed = false;

		if ( hovered && detail::g_ctx.m_input.m_clicked && detail::g_ctx.m_active_slider_id == 0 )
		{
			detail::g_ctx.m_active_slider_id = id;
		}

		const auto is_active = detail::g_ctx.m_active_slider_id == id;

		if ( is_active && detail::g_ctx.m_input.m_down )
		{
			const auto normalized = std::clamp( ( detail::g_ctx.m_input.m_x - frame_rect.m_x ) / frame_rect.m_w, 0.0f, 1.0f );

			if constexpr ( std::is_integral_v<T> )
			{
				v = static_cast< T >( v_min + normalized * ( v_max - v_min ) );
			}
			else
			{
				v = v_min + normalized * ( v_max - v_min );
			}

			changed = true;
		}

		const auto value_lerp_speed = 20.0f * detail::g_ctx.m_delta_time;
		value_anim = value_anim + ( static_cast< float >( v ) - value_anim ) * value_lerp_speed;

		if ( !label.empty( ) )
		{
			std::string label_str( label.begin( ), label.end( ) );
			zdraw::text( abs.m_x, abs.m_y, label_str, detail::g_ctx.m_style.text );
		}

		zdraw::text( abs.m_x + slider_width - value_w, abs.m_y, value_buf, detail::g_ctx.m_style.text );

		const auto bg_col = detail::g_ctx.m_style.slider_bg;
		const auto border_col = is_active ? detail::lighten_color( detail::g_ctx.m_style.slider_border, 1.3f ) : ( hovered ? detail::lighten_color( detail::g_ctx.m_style.slider_border, 1.15f ) : detail::g_ctx.m_style.slider_border );

		zdraw::rect_filled( frame_rect.m_x, frame_rect.m_y, frame_rect.m_w, frame_rect.m_h, bg_col );
		zdraw::rect( frame_rect.m_x, frame_rect.m_y, frame_rect.m_w, frame_rect.m_h, border_col, 1.0f );

		const auto normalized_pos = ( value_anim - static_cast< float >( v_min ) ) / static_cast< float >( v_max - v_min );
		const auto fill_width = ( frame_rect.m_w - 4.0f ) * normalized_pos;

		if ( fill_width > 0.0f )
		{
			constexpr auto pad = 2.0f;
			const auto fill_x = frame_rect.m_x + pad;
			const auto fill_y = frame_rect.m_y + pad;
			const auto fill_h = frame_rect.m_h - pad * 2.0f;

			const auto fill_col = detail::g_ctx.m_style.slider_fill;
			const auto col_left = detail::lighten_color( fill_col, 1.2f );
			const auto col_right = detail::darken_color( fill_col, 0.8f );

			zdraw::rect_filled_multi_color( fill_x, fill_y, fill_width, fill_h, col_left, col_right, col_right, col_left );
		}

		return changed;
	}

	bool slider_float( std::string_view label, float& v, float v_min, float v_max, std::string_view format )
	{
		return slider_scalar( label, v, v_min, v_max, format );
	}

	bool slider_int( std::string_view label, int& v, int v_min, int v_max )
	{
		return slider_scalar( label, v, v_min, v_max, "%d" );
	}

	bool combo( std::string_view label, int& current_item, const char* const items[ ], int items_count, float width )
	{
		auto win = detail::current_window( );
		if ( !win || items_count == 0 )
		{
			return false;
		}

		const auto id = detail::hash_str( label );
		const auto is_open = ( detail::g_ctx.m_active_combo_id == id );

		auto [label_w, label_h] = zdraw::measure_text( label );

		if ( width <= 0.0f )
		{
			width = calc_item_width( 0 );
		}

		const auto combo_height = detail::g_ctx.m_style.combo_height;
		const auto total_height = label_h + combo_height - 2.0f;

		auto local = detail::item_add( width, total_height );
		auto abs = detail::item_rect_abs( local );

		if ( !label.empty( ) )
		{
			std::string label_str( label.begin( ), label.end( ) );
			zdraw::text( abs.m_x, abs.m_y, label_str, detail::g_ctx.m_style.text );
		}

		const auto button_y = abs.m_y + label_h - 2.0f;
		const auto button_rect = rect{ abs.m_x, button_y, width, combo_height };
		const auto hovered = detail::mouse_in_rect( button_rect );

		static std::unordered_map<std::uintptr_t, float> hover_anims{};
		auto& hover_anim = hover_anims[ id ];

		const auto hover_target = hovered ? 1.0f : 0.0f;
		hover_anim = hover_anim + ( hover_target - hover_anim ) * std::min( 15.0f * detail::g_ctx.m_delta_time, 1.0f );

		auto changed = false;

		if ( hovered && detail::g_ctx.m_input.m_clicked && !detail::g_ctx.m_overlay_consuming_input )
		{
			detail::g_ctx.m_active_combo_id = is_open ? 0 : id;
		}

		const auto base_bg = detail::g_ctx.m_style.combo_bg;
		const auto hover_bg = detail::g_ctx.m_style.combo_hovered;
		const auto bg_col = zdraw::rgba
		{
			static_cast< std::uint8_t >( base_bg.r + ( hover_bg.r - base_bg.r ) * hover_anim ),
			static_cast< std::uint8_t >( base_bg.g + ( hover_bg.g - base_bg.g ) * hover_anim ),
			static_cast< std::uint8_t >( base_bg.b + ( hover_bg.b - base_bg.b ) * hover_anim ),
			base_bg.a
		};

		const auto border_col = is_open ? detail::lighten_color( detail::g_ctx.m_style.combo_border, 1.3f ) : ( hovered ? detail::lighten_color( detail::g_ctx.m_style.combo_border, 1.15f ) : detail::g_ctx.m_style.combo_border );

		const auto col_top = detail::lighten_color( bg_col, 1.1f );
		const auto col_bottom = detail::darken_color( bg_col, 0.85f );
		zdraw::rect_filled_multi_color( button_rect.m_x, button_rect.m_y, button_rect.m_w, button_rect.m_h, col_top, col_top, col_bottom, col_bottom );
		zdraw::rect( button_rect.m_x, button_rect.m_y, button_rect.m_w, button_rect.m_h, border_col, 1.0f );

		const auto current_text = ( current_item >= 0 && current_item < items_count ) ? items[ current_item ] : "";
		const auto text_padding = detail::g_ctx.m_style.frame_padding_x;
		zdraw::text( button_rect.m_x + text_padding, button_rect.m_y + 3.0f, current_text, detail::g_ctx.m_style.text );

		const auto arrow_size = 6.0f;
		const auto arrow_x = button_rect.m_x + button_rect.m_w - arrow_size - detail::g_ctx.m_style.frame_padding_x - 4.0f;
		const auto arrow_y = button_rect.m_y + ( combo_height - arrow_size * 0.5f ) * 0.5f;

		const auto arrow_col = detail::g_ctx.m_style.combo_arrow;

		if ( is_open )
		{
			zdraw::line( arrow_x, arrow_y + 3.0f, arrow_x + arrow_size * 0.5f, arrow_y, arrow_col, 1.5f );
			zdraw::line( arrow_x + arrow_size * 0.5f, arrow_y, arrow_x + arrow_size, arrow_y + 3.0f, arrow_col, 1.5f );
		}
		else
		{
			zdraw::line( arrow_x, arrow_y, arrow_x + arrow_size * 0.5f, arrow_y + 3.0f, arrow_col, 1.5f );
			zdraw::line( arrow_x + arrow_size * 0.5f, arrow_y + 3.0f, arrow_x + arrow_size, arrow_y, arrow_col, 1.5f );
		}

		if ( is_open )
		{
			struct combo_overlay
			{
				rect button_rect;
				const char* const* items;
				int items_count;
				int* current_item;
				bool* changed;
				float width;
				bool hovered;
				std::uintptr_t combo_id;

				void operator()( )
				{
					const auto& style = detail::g_ctx.m_style;
					const auto dropdown_y = button_rect.m_y + button_rect.m_h + 2.0f;
					const auto item_height = style.combo_item_height;
					const auto dropdown_h = items_count * item_height + 4.0f;

					const auto dropdown_rect = rect{ button_rect.m_x, dropdown_y, width, dropdown_h };

					const auto bg_top = style.combo_popup_bg;
					const auto bg_bottom = detail::darken_color( bg_top, 0.9f );

					zdraw::rect_filled( dropdown_rect.m_x + 2.0f, dropdown_rect.m_y + 2.0f, width, dropdown_h, zdraw::rgba{ 0, 0, 0, 60 } );
					zdraw::rect_filled_multi_color( dropdown_rect.m_x, dropdown_rect.m_y, width, dropdown_h, bg_top, bg_top, bg_bottom, bg_bottom );
					zdraw::rect( dropdown_rect.m_x, dropdown_rect.m_y, width, dropdown_h, detail::lighten_color( style.combo_popup_border, 1.1f ), 1.0f );

					zdraw::push_clip_rect( dropdown_rect.m_x, dropdown_rect.m_y, dropdown_rect.m_x + dropdown_rect.m_w, dropdown_rect.m_y + dropdown_rect.m_h );

					static std::unordered_map<std::uintptr_t, float> item_hover_anims{};

					for ( int i = 0; i < items_count; ++i )
					{
						const auto item_y_offset = i * item_height;

						const auto item_rect = rect
						{
							dropdown_rect.m_x + 2.0f,
							dropdown_rect.m_y + item_y_offset + 2.0f,
							width - 4.0f,
							item_height - 2.0f
						};

						const auto item_hovered = detail::mouse_in_rect( item_rect );
						const auto item_anim_id = ( combo_id ^ static_cast< std::uintptr_t >( i ) ) ^ 0x4954454D;
						auto& item_hover_anim = item_hover_anims[ item_anim_id ];

						const auto item_hover_target = item_hovered ? 1.0f : 0.0f;
						item_hover_anim = item_hover_anim + ( item_hover_target - item_hover_anim ) * 0.3f;

						if ( i == *current_item )
						{
							const auto selected_col = style.combo_item_selected;
							zdraw::rect_filled( item_rect.m_x, item_rect.m_y, item_rect.m_w, item_rect.m_h, selected_col );

							const auto accent_bar_col = detail::lighten_color( style.combo_arrow, 1.2f );
							zdraw::rect_filled( item_rect.m_x, item_rect.m_y, 2.0f, item_rect.m_h, accent_bar_col );
						}

						if ( item_hover_anim > 0.01f )
						{
							const auto hover_col = style.combo_item_hovered;
							auto hover_alpha = hover_col;
							hover_alpha.a = static_cast< std::uint8_t >( hover_col.a * item_hover_anim );
							zdraw::rect_filled( item_rect.m_x, item_rect.m_y, item_rect.m_w, item_rect.m_h, hover_alpha );
						}

						zdraw::text( dropdown_rect.m_x + style.frame_padding_x + 4.0f, dropdown_rect.m_y + item_y_offset + 3.0f, items[ i ], style.text );

						if ( item_hovered && detail::g_ctx.m_input.m_clicked )
						{
							*current_item = i;
							*changed = true;
							detail::g_ctx.m_active_combo_id = 0;
						}
					}

					zdraw::pop_clip_rect( );

					if ( detail::g_ctx.m_input.m_clicked && !hovered && !detail::mouse_in_rect( dropdown_rect ) )
					{
						detail::g_ctx.m_active_combo_id = 0;
					}
				}
			};

			detail::g_ctx.m_combo_overlay_list.emplace_back( combo_overlay{ button_rect, items, items_count, &current_item, &changed, width, hovered, id } );
			detail::g_ctx.m_overlay_consuming_input = true;
		}

		return changed;
	}

} // namespace zui