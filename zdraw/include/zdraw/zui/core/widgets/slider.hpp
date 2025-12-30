#ifndef SLIDER_HPP
#define SLIDER_HPP

#include "../context.hpp"
#include "../layout.hpp"
#include "../color.hpp"

#include <include/zdraw/zdraw.hpp>
#include <algorithm>
#include <cstdio>
#include <type_traits>

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
			const auto slider_height = style.slider_height;
			const auto spacing = style.item_spacing_y * 0.25f;
			const auto total_height = text_height + spacing + slider_height;

			auto local = item_add( slider_width, total_height );
			auto abs = to_absolute( local );

			const auto frame_y = abs.y + text_height + spacing;
			const auto frame_rect = rect{ abs.x, frame_y, slider_width, slider_height };

			const auto can_interact = !ctx( ).overlay_blocking_input( );
			const auto hovered = can_interact && input.hovered( frame_rect );

			if ( hovered && input.mouse_clicked( ) && ctx( ).m_active_slider_id == invalid_id )
			{
				ctx( ).m_active_slider_id = id;
			}

			const auto is_active = ctx( ).m_active_slider_id == id;

			auto changed = false;
			if ( is_active && input.mouse_down( ) && can_interact )
			{
				const auto normalized = std::clamp( ( input.mouse_x( ) - frame_rect.x ) / frame_rect.w, 0.0f, 1.0f );

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

			auto& value_anim = anims.get( id );
			value_anim = value_anim + ( static_cast< float >( v ) - value_anim ) * std::min( 35.0f * zdraw::get_delta_time( ), 1.0f );

			if ( !display_label.empty( ) )
			{
				zdraw::text( abs.x, abs.y, display_label, style.text );
			}

			zdraw::text( abs.x + slider_width - value_w, abs.y, value_buf, style.text );

			const auto border_col = is_active ? lighten( style.slider_border, 1.3f ) : ( hovered ? lighten( style.slider_border, 1.15f ) : style.slider_border );

			zdraw::rect_filled( frame_rect.x, frame_rect.y, frame_rect.w, frame_rect.h, style.slider_bg );
			zdraw::rect( frame_rect.x, frame_rect.y, frame_rect.w, frame_rect.h, border_col );

			const auto range = static_cast< float >( v_max - v_min );
			const auto normalized_pos = ( value_anim - static_cast< float >( v_min ) ) / range;
			const auto fill_width = ( frame_rect.w - 4.0f ) * normalized_pos;

			if ( fill_width > 0.0f )
			{
				constexpr auto pad = 2.0f;
				const auto fill_x = frame_rect.x + pad;
				const auto fill_y = frame_rect.y + pad;
				const auto fill_h = frame_rect.h - pad * 2.0f;

				const auto col_left = lighten( style.slider_fill, 1.2f );
				const auto col_right = darken( style.slider_fill, 0.8f );

				zdraw::rect_filled_multi_color( fill_x, fill_y, fill_width, fill_h, col_left, col_right, col_right, col_left );
			}

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