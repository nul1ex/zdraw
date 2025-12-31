#ifndef SLIDER_HPP
#define SLIDER_HPP

#include "../context.hpp"
#include "../layout.hpp"
#include "../color.hpp"

#include <include/zdraw/zdraw.hpp>

namespace zui {

	namespace detail {

		template<typename T>
		inline bool slider_impl( std::string_view label, T& v, T v_min, T v_max, std::string_view format )
		{
			auto* win = ctx( ).current_window( );
			if ( !win )
			{
				return false;
			}

			const auto id = ctx( ).generate_id( label );
			const auto display_label = context::get_display_label( label );
			const auto& style = ctx( ).get_style( );
			auto& input = ctx( ).input( );
			auto& anims = ctx( ).anims( );

			auto [avail_w, avail_h] = get_content_region_avail( );
			const auto slider_width = avail_w;

			char value_buf[ 64 ]{};
			if constexpr ( std::is_floating_point_v<T> )
			{
				std::snprintf( value_buf, sizeof( value_buf ), format.data( ), v );
			}
			else
			{
				std::snprintf( value_buf, sizeof( value_buf ), "%d", static_cast< int >( v ) );
			}

			auto [label_w, label_h] = zdraw::measure_text( display_label );
			auto [value_w, value_h] = zdraw::measure_text( value_buf );

			const auto text_height = std::max( label_h, value_h );
			const auto track_height = style.slider_height;
			const auto knob_width = 10.0f;
			const auto knob_height = track_height + 6.0f;
			const auto spacing = style.item_spacing_y * 0.25f;
			const auto total_height = text_height + spacing + knob_height;

			auto local = item_add( slider_width, total_height );
			auto abs = to_absolute( local );

			const auto track_y = abs.y + text_height + spacing + ( knob_height - track_height ) * 0.5f;
			const auto track_rect = rect{ abs.x, track_y, slider_width, track_height };

			const auto knob_min_x = abs.x;
			const auto knob_max_x = abs.x + slider_width - knob_width;
			const auto knob_y = abs.y + text_height + spacing;

			const auto hit_rect = rect{ abs.x, knob_y, slider_width, knob_height };
			const auto can_interact = !ctx( ).overlay_blocking_input( );
			const auto hovered = can_interact && input.hovered( hit_rect );

			if ( hovered && input.mouse_clicked( ) && ctx( ).m_active_slider_id == invalid_id )
			{
				ctx( ).m_active_slider_id = id;
			}

			const auto is_active = ctx( ).m_active_slider_id == id;

			auto changed{ false };
			if ( is_active && input.mouse_down( ) && can_interact )
			{
				const auto mouse_normalized = std::clamp( ( input.mouse_x( ) - knob_min_x - knob_width * 0.5f ) / ( knob_max_x - knob_min_x ), 0.0f, 1.0f );

				if constexpr ( std::is_integral_v<T> )
				{
					v = static_cast< T >( v_min + mouse_normalized * ( v_max - v_min ) );
				}
				else
				{
					v = v_min + mouse_normalized * ( v_max - v_min );
				}

				changed = true;
			}

			auto& value_anim = anims.get( id );
			value_anim = value_anim + ( static_cast< float >( v ) - value_anim ) * std::min( 20.0f * zdraw::get_delta_time( ), 1.0f );

			auto& hover_anim = anims.get( id + 1000000 );
			const auto hover_target = ( hovered || is_active ) ? 1.0f : 0.0f;
			hover_anim = hover_anim + ( hover_target - hover_anim ) * std::min( 12.0f * zdraw::get_delta_time( ), 1.0f );

			auto& active_anim = anims.get( id + 2000000 );
			const auto active_target = is_active ? 1.0f : 0.0f;
			active_anim = active_anim + ( active_target - active_anim ) * std::min( 15.0f * zdraw::get_delta_time( ), 1.0f );

			const auto range = static_cast< float >( v_max - v_min );
			const auto normalized_pos = ( value_anim - static_cast< float >( v_min ) ) / range;
			const auto knob_x = knob_min_x + ( knob_max_x - knob_min_x ) * normalized_pos;

			if ( !display_label.empty( ) )
			{
				zdraw::text( abs.x, abs.y, display_label, style.text );
			}

			const auto value_col = lerp( style.text, lighten( style.slider_fill, 1.2f ), hover_anim * 0.35f );
			zdraw::text( abs.x + slider_width - value_w, abs.y, value_buf, value_col );

			const auto track_bg = darken( style.slider_bg, 0.7f );
			zdraw::rect_filled( track_rect.x, track_rect.y, track_rect.w, track_rect.h, track_bg );

			const auto shadow_col = zdraw::rgba{ 0, 0, 0, static_cast< std::uint8_t >( 40 + hover_anim * 10 ) };
			zdraw::rect_filled_multi_color( track_rect.x + 1.0f, track_rect.y + 1.0f, track_rect.w - 2.0f, track_rect.h * 0.4f, shadow_col, shadow_col, alpha( shadow_col, 0.0f ), alpha( shadow_col, 0.0f ) );

			const auto border_col = lerp( style.slider_border, lighten( style.slider_fill, 0.6f ), hover_anim * 0.25f );
			zdraw::rect( track_rect.x, track_rect.y, track_rect.w, track_rect.h, border_col );

			constexpr auto fill_pad = 2.0f;
			const auto fill_w = ( knob_x + knob_width * 0.5f ) - track_rect.x - fill_pad;

			if ( fill_w > 0.5f )
			{
				const auto fill_x = track_rect.x + fill_pad;
				const auto fill_y = track_rect.y + fill_pad;
				const auto fill_h = track_rect.h - fill_pad * 2.0f;

				const auto fill_left = lighten( style.slider_fill, 1.1f + hover_anim * 0.1f );
				const auto fill_right = darken( style.slider_fill, 0.85f );
				zdraw::rect_filled_multi_color( fill_x, fill_y, fill_w, fill_h, fill_left, fill_right, fill_right, fill_left );

				const auto shine = zdraw::rgba{ 255, 255, 255, static_cast< std::uint8_t >( 20 + hover_anim * 15 ) };
				zdraw::rect_filled_multi_color( fill_x, fill_y, fill_w, fill_h * 0.4f, shine, shine, alpha( shine, 0.0f ), alpha( shine, 0.0f ) );
			}

			const auto knob_shadow_offset = 1.0f + active_anim * 0.5f;
			const auto knob_shadow = zdraw::rgba{ 0, 0, 0, static_cast< std::uint8_t >( 50 + active_anim * 20 ) };
			zdraw::rect_filled( knob_x + 1.0f, knob_y + knob_shadow_offset, knob_width, knob_height, knob_shadow );

			const auto knob_bg = lerp( style.slider_bg, lighten( style.slider_bg, 1.2f ), hover_anim * 0.4f );
			zdraw::rect_filled( knob_x, knob_y, knob_width, knob_height, knob_bg );

			const auto knob_highlight = zdraw::rgba{ 255, 255, 255, static_cast< std::uint8_t >( 15 + hover_anim * 20 ) };
			zdraw::rect_filled_multi_color( knob_x + 1.0f, knob_y + 1.0f, knob_width - 2.0f, knob_height * 0.35f, knob_highlight, knob_highlight, alpha( knob_highlight, 0.0f ), alpha( knob_highlight, 0.0f ) );

			const auto knob_border = lerp( style.slider_border, lighten( style.slider_fill, 0.8f ), hover_anim * 0.5f );
			zdraw::rect( knob_x, knob_y, knob_width, knob_height, knob_border );

			return changed;
		}

	} // namespace detail

	inline bool slider_float( std::string_view label, float& v, float v_min, float v_max, std::string_view format = "%.1f" )
	{
		return detail::slider_impl( label, v, v_min, v_max, format );
	}

	inline bool slider_int( std::string_view label, int& v, int v_min, int v_max )
	{
		return detail::slider_impl( label, v, v_min, v_max, "%d" );
	}

} // namespace zui

#endif // !SLIDER_HPP