#include <include/zdraw/zdraw.hpp>
#include <include/zdraw/zui.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zui {

	namespace detail {

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

			zui::style m_style{};
			std::vector<style_var_backup> m_style_var_stack{};
			std::vector<style_color_backup> m_style_color_stack{};
		};

		static context g_ctx{};

		static std::uintptr_t fnv1a64( const void* p, std::size_t n ) noexcept
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

		static std::uintptr_t hash_str( std::string_view s ) noexcept
		{
			return fnv1a64( s.data( ), s.size( ) );
		}

		static void update_input( ) noexcept
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

		static bool mouse_in_rect( const rect& r ) noexcept
		{
			return g_ctx.m_input.m_x >= r.m_x && g_ctx.m_input.m_x <= r.m_x + r.m_w && g_ctx.m_input.m_y >= r.m_y && g_ctx.m_input.m_y <= r.m_y + r.m_h;
		}

		static window_state* current_window( ) noexcept
		{
			if ( g_ctx.m_windows.empty( ) )
			{
				return nullptr;
			}

			return &g_ctx.m_windows.back( );
		}

		static rect item_add( float w, float h ) noexcept
		{
			auto win = current_window( );
			if ( !win )
			{
				return { 0,0,0,0 };
			}

			if ( win->m_line_h > 0.0f )
			{
				win->m_cursor_y += win->m_line_h + g_ctx.m_style.ItemSpacingY;
			}

			rect r{ win->m_cursor_x, win->m_cursor_y, w, h };
			win->m_last_item = r;
			win->m_line_h = h;

			return r;
		}

		static rect item_rect_abs( const rect& local ) noexcept
		{
			auto win = current_window( );
			if ( !win )
			{
				return local;
			}

			return { win->m_rect.m_x + local.m_x, win->m_rect.m_y + local.m_y, local.m_w, local.m_h };
		}

		static zdraw::rgba& style_color_ref( style_color idx ) noexcept
		{
			switch ( idx )
			{
			case style_color::WindowBg: return g_ctx.m_style.WindowBg;
			case style_color::WindowBorder: return g_ctx.m_style.WindowBorder;
			case style_color::NestedBg: return g_ctx.m_style.NestedBg;
			case style_color::NestedBorder: return g_ctx.m_style.NestedBorder;
			default: return g_ctx.m_style.WindowBg;
			}
		}

		static float* style_var_ref( style_var var ) noexcept
		{
			switch ( var )
			{
			case style_var::WindowPaddingX: return &g_ctx.m_style.WindowPaddingX;
			case style_var::WindowPaddingY: return &g_ctx.m_style.WindowPaddingY;
			case style_var::ItemSpacingX: return &g_ctx.m_style.ItemSpacingX;
			case style_var::ItemSpacingY: return &g_ctx.m_style.ItemSpacingY;
			case style_var::FramePaddingX: return &g_ctx.m_style.FramePaddingX;
			case style_var::FramePaddingY: return &g_ctx.m_style.FramePaddingY;
			case style_var::WindowRounding: return &g_ctx.m_style.WindowRounding;
			case style_var::BorderThickness: return &g_ctx.m_style.BorderThickness;
			default: return nullptr;
			}
		}

	} // namespace detail

	bool initialize( HWND hwnd ) noexcept
	{
		detail::g_ctx.m_hwnd = hwnd;
		return hwnd != nullptr;
	}

	void shutdown( ) noexcept
	{
		detail::g_ctx.m_hwnd = nullptr;
		detail::g_ctx.m_windows.clear( );
		detail::g_ctx.m_id_stack.clear( );
		detail::g_ctx.m_active_window_id = 0;
		detail::g_ctx.m_input = {};
		detail::g_ctx.m_prev_input = {};
		detail::g_ctx.m_style_var_stack.clear( );
		detail::g_ctx.m_style_color_stack.clear( );
	}

	void begin_frame( ) noexcept
	{
		detail::g_ctx.m_prev_input = detail::g_ctx.m_input;
		detail::update_input( );

		if ( detail::g_ctx.m_input.m_released )
		{
			detail::g_ctx.m_active_window_id = 0;
		}

		detail::g_ctx.m_windows.clear( );
		detail::g_ctx.m_id_stack.clear( );
	}

	void end_frame( ) noexcept { }

	bool begin_window( std::string_view title, float& x, float& y, float w, float h ) noexcept
	{
		const auto id = detail::hash_str( title );
		auto abs = rect{ x, y, w, h };

		const auto hovered = detail::mouse_in_rect( abs );
		if ( hovered && detail::g_ctx.m_input.m_clicked && detail::g_ctx.m_active_window_id == 0 )
		{
			detail::g_ctx.m_active_window_id = id;
		}

		if ( detail::g_ctx.m_active_window_id == id && detail::g_ctx.m_input.m_down )
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
		win->m_cursor_x = detail::g_ctx.m_style.WindowPaddingX;
		win->m_cursor_y = detail::g_ctx.m_style.WindowPaddingY;
		win->m_line_h = 0.0f;
		win->m_last_item = { 0,0,0,0 };
		win->m_is_child = false;

		zdraw::rect_filled( abs.m_x, abs.m_y, abs.m_w, abs.m_h, detail::g_ctx.m_style.WindowBg );

		if ( detail::g_ctx.m_style.BorderThickness > 0.0f )
		{
			zdraw::rect( abs.m_x, abs.m_y, abs.m_w, abs.m_h, detail::g_ctx.m_style.WindowBorder, detail::g_ctx.m_style.BorderThickness );
		}

		zdraw::push_clip_rect( abs.m_x, abs.m_y, abs.m_x + abs.m_w, abs.m_y + abs.m_h );

		detail::g_ctx.m_id_stack.push_back( id );
		return true;
	}

	void end_window( ) noexcept
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

	bool begin_nested_window( std::string_view title, float w, float h ) noexcept
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
		win->m_cursor_x = detail::g_ctx.m_style.WindowPaddingX;
		win->m_cursor_y = detail::g_ctx.m_style.WindowPaddingY;
		win->m_line_h = 0.0f;
		win->m_last_item = { 0,0,0,0 };
		win->m_is_child = false;

		zdraw::rect_filled( abs.m_x, abs.m_y, abs.m_w, abs.m_h, detail::g_ctx.m_style.NestedBg );

		if ( detail::g_ctx.m_style.BorderThickness > 0.0f )
		{
			zdraw::rect( abs.m_x, abs.m_y, abs.m_w, abs.m_h, detail::g_ctx.m_style.NestedBorder, detail::g_ctx.m_style.BorderThickness );
		}

		zdraw::push_clip_rect( abs.m_x, abs.m_y, abs.m_x + abs.m_w, abs.m_y + abs.m_h );

		detail::g_ctx.m_id_stack.push_back( detail::hash_str( title ) );
		return true;
	}

	void end_nested_window( ) noexcept
	{
		end_window( );
	}

	std::pair<float, float> get_cursor_pos( ) noexcept
	{
		auto win = detail::current_window( );
		if ( !win )
		{
			return { 0.0f,0.0f };
		}

		return { win->m_cursor_x, win->m_cursor_y };
	}

	void set_cursor_pos( float x, float y ) noexcept
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

	std::pair<float, float> get_content_region_avail( ) noexcept
	{
		auto win = detail::current_window( );
		if ( !win )
		{
			return { 0.0f, 0.0f };
		}

		const auto pad_x = detail::g_ctx.m_style.WindowPaddingX;
		const auto pad_y = detail::g_ctx.m_style.WindowPaddingY;

		const auto work_min_x = pad_x;
		const auto work_min_y = pad_y;
		const auto work_max_x = win->m_rect.m_w - pad_x;
		const auto work_max_y = win->m_rect.m_h - pad_y;

		auto next_x = win->m_cursor_x;
		auto next_y = win->m_cursor_y;

		if ( win->m_line_h > 0.0f )
		{
			next_y += win->m_line_h + detail::g_ctx.m_style.ItemSpacingY;
		}

		auto avail_w = std::max( 0.0f, work_max_x - next_x );
		auto avail_h = std::max( 0.0f, work_max_y - next_y );
		return { avail_w, avail_h };
	}

	style& get_style( ) noexcept
	{
		return detail::g_ctx.m_style;
	}

	void push_style_var( style_var var, float value ) noexcept
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

	void pop_style_var( int count ) noexcept
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

	void push_style_color( style_color idx, const zdraw::rgba& col ) noexcept
	{
		auto& ref = detail::style_color_ref( idx );
		detail::style_color_backup backup{ idx, ref };
		detail::g_ctx.m_style_color_stack.push_back( backup );
		ref = col;
	}

	void pop_style_color( int count ) noexcept
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

} // namespace zui