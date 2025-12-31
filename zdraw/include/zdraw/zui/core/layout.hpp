#ifndef LAYOUT_HPP
#define LAYOUT_HPP

#include "context.hpp"
#include "color.hpp"

#include <include/zdraw/zdraw.hpp>
#include <algorithm>

namespace zui {

	inline void process_scroll_wheel( const rect& bounds, float content_height, float visible_start_y, float& scroll_y )
	{
		const auto& style = ctx( ).get_style( );
		auto& input = ctx( ).input( );

		const auto visible_height = bounds.h - visible_start_y;
		const auto total_content_height = content_height + style.window_padding_y;
		const auto max_scroll = std::max( 0.0f, total_content_height - visible_height );

		if ( input.hovered( bounds ) && !ctx( ).overlay_blocking_input( ) )
		{
			const auto scroll_delta = input.scroll_delta( );
			if ( scroll_delta != 0.0f )
			{
				scroll_y -= scroll_delta * 30.0f;
			}
		}

		scroll_y = std::clamp( scroll_y, 0.0f, max_scroll );
	}

	inline bool process_scrollbar( widget_id scroll_id, const rect& bounds, float content_height, float visible_start_y, float& scroll_y )
	{
		const auto& style = ctx( ).get_style( );
		auto& input = ctx( ).input( );

		const auto visible_height = bounds.h - visible_start_y;
		const auto total_content_height = content_height + style.window_padding_y;
		const auto max_scroll = std::max( 0.0f, total_content_height - visible_height );

		if ( total_content_height <= visible_height )
		{
			return false;
		}

		const auto scrollbar_x = bounds.right( ) - style.scrollbar_width - style.scrollbar_padding;
		const auto scrollbar_y = bounds.y + visible_start_y + style.scrollbar_padding;
		const auto scrollbar_h = visible_height - style.scrollbar_padding * 2.0f;

		const auto visible_ratio = visible_height / total_content_height;
		const auto thumb_h = std::max( style.scrollbar_min_thumb_height, scrollbar_h * visible_ratio );
		const auto scroll_ratio = max_scroll > 0.0f ? scroll_y / max_scroll : 0.0f;
		const auto thumb_y = scrollbar_y + scroll_ratio * ( scrollbar_h - thumb_h );

		const auto thumb_rect = rect{ scrollbar_x, thumb_y, style.scrollbar_width, thumb_h };
		const auto track_rect = rect{ scrollbar_x, scrollbar_y, style.scrollbar_width, scrollbar_h };

		const auto thumb_hovered = input.hovered( thumb_rect );
		const auto track_hovered = input.hovered( track_rect );
		const auto is_active = ctx( ).m_active_scrollbar_id == scroll_id;

		if ( thumb_hovered && input.mouse_clicked( ) && ctx( ).m_active_scrollbar_id == invalid_id )
		{
			ctx( ).m_active_scrollbar_id = scroll_id;
		}

		if ( is_active && input.mouse_down( ) )
		{
			const auto delta_y = input.mouse_delta_y( );
			const auto scroll_per_pixel = max_scroll / ( scrollbar_h - thumb_h );
			scroll_y = std::clamp( scroll_y + delta_y * scroll_per_pixel, 0.0f, max_scroll );
		}

		if ( track_hovered && !thumb_hovered && input.mouse_clicked( ) && !is_active )
		{
			const auto click_ratio = ( input.mouse_y( ) - scrollbar_y - thumb_h * 0.5f ) / ( scrollbar_h - thumb_h );
			scroll_y = std::clamp( click_ratio * max_scroll, 0.0f, max_scroll );
		}

		zdraw::rect_filled( track_rect.x, track_rect.y, track_rect.w, track_rect.h, style.scrollbar_bg );

		auto thumb_color = style.scrollbar_thumb;
		if ( is_active )
		{
			thumb_color = style.scrollbar_thumb_active;
		}
		else if ( thumb_hovered )
		{
			thumb_color = style.scrollbar_thumb_hovered;
		}

		zdraw::rect_filled( thumb_rect.x, thumb_rect.y, thumb_rect.w, thumb_rect.h, thumb_color );

		return true;
	}

	inline rect item_add( float w, float h )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return { 0, 0, 0, 0 };
		}

		if ( win->line_height > 0.0f )
		{
			win->cursor_y += win->line_height + ctx( ).get_style( ).item_spacing_y;
		}

		rect r{ win->cursor_x, win->cursor_y, w, h };
		win->last_item = r;
		win->line_height = h;

		const auto item_bottom = win->cursor_y + h;
		win->content_height = std::max( win->content_height, item_bottom );

		return r;
	}

	[[nodiscard]] inline rect to_absolute( const rect& local )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return local;
		}

		auto scroll_offset = win->scroll_y;
		return rect{ local.x + win->bounds.x, local.y + win->bounds.y - scroll_offset, local.w, local.h };
	}

	[[nodiscard]] inline std::pair<float, float> get_content_region_avail( )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return { 0.0f, 0.0f };
		}

		const auto& style = ctx( ).get_style( );

		float scrollbar_space = 0.0f;
		if ( win->scrollbar_visible )
		{
			scrollbar_space = style.scrollbar_width + style.scrollbar_padding * 2.0f;
		}

		const auto work_max_x = win->bounds.w - style.window_padding_x - scrollbar_space;
		const auto work_max_y = win->bounds.h - style.window_padding_y;

		auto next_x = win->cursor_x;
		auto next_y = win->cursor_y;

		if ( win->line_height > 0.0f )
		{
			next_y += win->line_height + style.item_spacing_y;
		}

		auto avail_w = std::max( 0.0f, work_max_x - next_x );
		auto avail_h = std::max( 0.0f, work_max_y - next_y );
		return { avail_w, avail_h };
	}

	[[nodiscard]] inline float calc_item_width( int count = 0 )
	{
		auto [avail_w, avail_h] = get_content_region_avail( );

		if ( count <= 0 )
		{
			return avail_w;
		}

		const auto spacing = ctx( ).get_style( ).item_spacing_x;
		return ( avail_w - spacing * ( count - 1 ) ) / static_cast< float >( count );
	}

	[[nodiscard]] inline std::pair<float, float> get_cursor_pos( )
	{
		auto* win = ctx( ).current_window( );
		return win ? std::make_pair( win->cursor_x, win->cursor_y ) : std::make_pair( 0.0f, 0.0f );
	}

	inline void set_cursor_pos( float x, float y )
	{
		if ( auto* win = ctx( ).current_window( ) )
		{
			win->cursor_x = x;
			win->cursor_y = y;
			win->line_height = 0.0f;
		}
	}

	inline void same_line( float offset_x = 0.0f )
	{
		auto* win = ctx( ).current_window( );
		if ( !win || win->line_height <= 0.0f )
		{
			return;
		}

		const auto spacing = ( offset_x == 0.0f ) ? ctx( ).get_style( ).item_spacing_x : offset_x;
		win->cursor_x = win->last_item.x + win->last_item.w + spacing;
		win->cursor_y = win->last_item.y;
		win->line_height = 0.0f;
	}

	inline void new_line( )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		if ( win->line_height > 0.0f )
		{
			win->cursor_y += win->line_height + ctx( ).get_style( ).item_spacing_y;
		}

		win->cursor_x = ctx( ).get_style( ).window_padding_x;
		win->line_height = 0.0f;
	}

	inline void indent( float amount = 0.0f )
	{
		if ( auto* win = ctx( ).current_window( ) )
		{
			if ( amount <= 0.0f )
			{
				amount = ctx( ).get_style( ).window_padding_x;
			}

			win->cursor_x += amount;
		}
	}

	inline void unindent( float amount = 0.0f )
	{
		if ( auto* win = ctx( ).current_window( ) )
		{
			if ( amount <= 0.0f )
			{
				amount = ctx( ).get_style( ).window_padding_x;
			}

			win->cursor_x = std::max( ctx( ).get_style( ).window_padding_x, win->cursor_x - amount );
		}
	}

	inline void separator( )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		const auto& style = ctx( ).get_style( );
		const auto sep_h = 1.0f;
		const auto sep_pad = style.item_spacing_y * 0.5f;

		auto [avail_w, _] = get_content_region_avail( );

		auto local = item_add( avail_w, sep_h + sep_pad * 2.0f );
		auto abs = to_absolute( local );

		auto sep_col = style.nested_border;
		sep_col.a = static_cast< std::uint8_t >( sep_col.a * 0.5f );

		zdraw::line( abs.x, abs.y + sep_pad, abs.x + avail_w, abs.y + sep_pad, sep_col, sep_h );
	}

	inline bool begin_window( std::string_view title, float& x, float& y, float& w, float& h, bool resizable = false, float min_w = 200.0f, float min_h = 200.0f, bool no_scrollbar = false )
	{
		const auto id = ctx( ).generate_id( title );
		auto abs = rect{ x, y, w, h };

		const auto& style = ctx( ).get_style( );
		auto& input = ctx( ).input( );

		constexpr auto grip_size{ 16.0f };
		const auto grip_rect = rect{ abs.right( ) - grip_size, abs.bottom( ) - grip_size, grip_size, grip_size };

		const auto grip_hovered = resizable && input.hovered( grip_rect );
		const auto window_hovered = input.hovered( abs );

		if ( grip_hovered && input.mouse_clicked( ) && ctx( ).m_active_resize_id == invalid_id )
		{
			ctx( ).m_active_resize_id = id;
		}

		if ( ctx( ).m_active_resize_id == id && input.mouse_down( ) )
		{
			w = std::max( min_w, w + input.mouse_delta_x( ) );
			h = std::max( min_h, h + input.mouse_delta_y( ) );
			abs.w = w;
			abs.h = h;
		}
		else if ( window_hovered && !grip_hovered && input.mouse_clicked( ) && ctx( ).m_active_window_id == invalid_id && ctx( ).m_active_slider_id == invalid_id && ctx( ).m_active_resize_id == invalid_id && ctx( ).m_active_scrollbar_id == invalid_id && !ctx( ).overlay_blocking_input( ) )
		{
			ctx( ).m_active_window_id = id;
		}

		if ( ctx( ).m_active_window_id == id && input.mouse_down( ) && ctx( ).m_active_slider_id == invalid_id && ctx( ).m_active_resize_id == invalid_id && ctx( ).m_active_scrollbar_id == invalid_id )
		{
			x += input.mouse_delta_x( );
			y += input.mouse_delta_y( );
			abs.x = x;
			abs.y = y;
		}

		auto& scroll_states = ctx( ).m_scroll_states;
		if ( scroll_states.find( id ) == scroll_states.end( ) )
		{
			scroll_states[ id ] = 0.0f;
		}

		auto& scrollbar_visible = ctx( ).m_scrollbar_visible;
		const auto prev_scrollbar_visible = scrollbar_visible.find( id ) != scrollbar_visible.end( ) ? scrollbar_visible[ id ] : false;

		window_state state{};
		state.title.assign( title.begin( ), title.end( ) );
		state.bounds = abs;
		state.cursor_x = style.window_padding_x;
		state.cursor_y = style.window_padding_y;
		state.line_height = 0.0f;
		state.is_child = false;
		state.scroll_y = scroll_states[ id ];
		state.content_height = style.window_padding_y;
		state.visible_start_y = 0.0f;
		state.scroll_id = id;
		state.scrollbar_visible = prev_scrollbar_visible;
		state.scrollbar_disabled = no_scrollbar;

		ctx( ).push_window( std::move( state ) );

		const auto base_color = style.window_bg;
		const auto col_tl = lighten( base_color, 1.15f );
		const auto col_br = darken( base_color, 0.85f );

		zdraw::rect_filled_multi_color( abs.x, abs.y, abs.w, abs.h, col_tl, base_color, col_br, base_color );

		if ( style.border_thickness > 0.0f )
		{
			zdraw::rect( abs.x, abs.y, abs.w, abs.h, style.window_border, style.border_thickness );
		}

		zdraw::push_clip_rect( abs.x, abs.y, abs.right( ), abs.bottom( ) );
		ctx( ).push_id( id );

		return true;
	}

	inline void end_window( )
	{
		auto* win = ctx( ).current_window( );
		zdraw::pop_clip_rect( );

		if ( win && win->scroll_id != invalid_id )
		{
			auto& scroll_y = ctx( ).m_scroll_states[ win->scroll_id ];
			scroll_y = win->scroll_y;

			process_scroll_wheel( win->bounds, win->content_height, win->visible_start_y, scroll_y );

			if ( !win->scrollbar_disabled )
			{
				const auto scrollbar_visible = process_scrollbar( win->scroll_id, win->bounds, win->content_height, win->visible_start_y, scroll_y );
				ctx( ).m_scrollbar_visible[ win->scroll_id ] = scrollbar_visible;
			}
			else
			{
				ctx( ).m_scrollbar_visible[ win->scroll_id ] = false;
			}

			ctx( ).m_scroll_states[ win->scroll_id ] = scroll_y;
			win->scroll_y = scroll_y;
		}

		ctx( ).pop_window( );
		ctx( ).pop_id( );
	}

	inline bool begin_nested_window( std::string_view title, float w, float h )
	{
		auto* parent = ctx( ).current_window( );
		if ( !parent )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( title );
		const auto& style = ctx( ).get_style( );

		auto local = item_add( w, h );
		auto abs = rect{ parent->bounds.x + local.x, parent->bounds.y + local.y - parent->scroll_y, w, h };

		auto& scroll_states = ctx( ).m_scroll_states;
		if ( scroll_states.find( id ) == scroll_states.end( ) )
		{
			scroll_states[ id ] = 0.0f;
		}

		auto& scrollbar_visible = ctx( ).m_scrollbar_visible;
		const auto prev_scrollbar_visible = scrollbar_visible.find( id ) != scrollbar_visible.end( ) ? scrollbar_visible[ id ] : false;

		window_state state{};
		state.title.assign( title.begin( ), title.end( ) );
		state.bounds = abs;
		state.cursor_x = style.window_padding_x;
		state.cursor_y = style.window_padding_y;
		state.line_height = 0.0f;
		state.is_child = true;
		state.scroll_y = scroll_states[ id ];
		state.content_height = style.window_padding_y;
		state.visible_start_y = 0.0f;
		state.scroll_id = id;
		state.scrollbar_visible = prev_scrollbar_visible;
		state.scrollbar_disabled = parent->scrollbar_disabled;

		ctx( ).push_window( std::move( state ) );

		const auto base_color = style.nested_bg;
		const auto col_tl = lighten( base_color, 1.15f );
		const auto col_br = darken( base_color, 0.85f );

		zdraw::rect_filled_multi_color( abs.x, abs.y, abs.w, abs.h, col_tl, base_color, col_br, base_color );

		if ( style.border_thickness > 0.0f )
		{
			zdraw::rect( abs.x, abs.y, abs.w, abs.h, style.nested_border, style.border_thickness );
		}

		zdraw::push_clip_rect( abs.x, abs.y, abs.right( ), abs.bottom( ) );
		ctx( ).push_id( id );

		return true;
	}

	inline void end_nested_window( )
	{
		end_window( );
	}

	inline bool begin_group_box( std::string_view title, float w, float h = 0.0f )
	{
		auto* parent = ctx( ).current_window( );
		if ( !parent )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( title );
		const auto& style = ctx( ).get_style( );
		const auto title_h = style.group_box_title_height;

		auto actual_h = h;
		if ( h <= 0.0f )
		{
			auto& height_cache = ctx( ).m_group_box_heights;
			if ( height_cache.find( id ) != height_cache.end( ) )
			{
				actual_h = height_cache[ id ];
			}
			else
			{
				actual_h = 100.0f;
			}
		}

		auto local = item_add( w, actual_h );
		auto abs = rect{ parent->bounds.x + local.x, parent->bounds.y + local.y - parent->scroll_y, w, actual_h };

		const auto border_y = abs.y + title_h * 0.5f;
		const auto box_height = abs.h - title_h * 0.5f;

		zdraw::rect_filled( abs.x, border_y, abs.w, box_height, style.group_box_bg );
		zdraw::rect( abs.x, border_y, abs.w, box_height, style.group_box_border, style.border_thickness );

		if ( !title.empty( ) )
		{
			auto [title_w, title_h_measured] = zdraw::measure_text( title );
			const auto text_x = abs.x + style.window_padding_x;
			const auto pad = 4.0f;

			const auto gap_start = text_x - pad;
			const auto gap_end = text_x + title_w + pad;

			zdraw::rect_filled( gap_start, abs.y, gap_end - gap_start, title_h, style.group_box_bg );
			zdraw::rect( gap_start, abs.y, gap_end - gap_start, title_h, style.group_box_border, style.border_thickness );

			std::string title_str( title.begin( ), title.end( ) );
			const auto text_y = abs.y + ( title_h - title_h_measured ) * 0.5f;
			zdraw::text( text_x, text_y, title_str, style.group_box_title_text );
		}

		auto& scroll_states = ctx( ).m_scroll_states;
		if ( scroll_states.find( id ) == scroll_states.end( ) )
		{
			scroll_states[ id ] = 0.0f;
		}

		auto& scrollbar_visible_map = ctx( ).m_scrollbar_visible;
		const auto prev_scrollbar_visible = scrollbar_visible_map.find( id ) != scrollbar_visible_map.end( ) ? scrollbar_visible_map[ id ] : false;

		window_state state{};
		state.title.assign( title.begin( ), title.end( ) );
		state.bounds = abs;
		state.cursor_x = style.window_padding_x;
		state.cursor_y = title_h + style.window_padding_y;
		state.line_height = 0.0f;
		state.is_child = true;
		state.scroll_y = scroll_states[ id ];
		state.content_height = title_h + style.window_padding_y;
		state.visible_start_y = title_h * 0.5f;
		state.scroll_id = id;
		state.scrollbar_visible = prev_scrollbar_visible;
		state.scrollbar_disabled = parent->scrollbar_disabled;

		ctx( ).push_window( std::move( state ) );

		zdraw::push_clip_rect( abs.x, abs.y + title_h, abs.right( ), abs.bottom( ) );
		ctx( ).push_id( id );

		return true;
	}

	inline void end_group_box( )
	{
		auto* win = ctx( ).current_window( );
		if ( win && win->scroll_id != invalid_id )
		{
			const auto& style = ctx( ).get_style( );
			const auto final_height = win->content_height + style.window_padding_y;
			ctx( ).m_group_box_heights[ win->scroll_id ] = final_height;
		}

		end_window( );
	}

} // namespace zui

#endif // !LAYOUT_HPP
