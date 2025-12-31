#ifndef CHECKBOX_HPP
#define CHECKBOX_HPP

#include "../context.hpp"
#include "../layout.hpp"
#include "../color.hpp"

#include <include/zdraw/zdraw.hpp>

namespace zui {

	inline bool checkbox( std::string_view label, bool& v )
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

		const auto check_size = style.checkbox_size;
		auto local = item_add( check_size, check_size );
		auto abs = to_absolute( local );

		auto [label_w, label_h] = zdraw::measure_text( display_label );
		const auto full_width = !label.empty( ) ? ( abs.w + style.item_spacing_x + label_w ) : abs.w;
		const auto extended = rect{ abs.x, abs.y, full_width, abs.h };

		const auto can_interact = !ctx( ).overlay_blocking_input( );
		const auto hovered = can_interact && input.hovered( extended );

		auto changed{ false };

		if ( hovered && input.mouse_clicked( ) )
		{
			v = !v;
			changed = true;
		}

		const auto check_anim = anims.animate( id, v ? 1.0f : 0.0f, 8.0f );
		const auto hover_anim = anims.animate( id + 1, hovered ? 1.0f : 0.0f, 10.0f );
		const auto ease_t = ease::smoothstep( check_anim );

		auto border_col = style.checkbox_border;
		if ( hover_anim > 0.01f )
		{
			border_col = lerp( border_col, style.checkbox_check, hover_anim * 0.3f );
		}

		zdraw::rect_filled( abs.x, abs.y, abs.w, abs.h, style.checkbox_bg );
		zdraw::rect( abs.x, abs.y, abs.w, abs.h, border_col );

		if ( ease_t > 0.01f )
		{
			constexpr auto pad{ 2.0f };
			const auto inner_w = abs.w - pad * 2.0f;
			const auto inner_h = abs.h - pad * 2.0f;

			const auto scale = 0.6f + ease_t * 0.4f;
			const auto scaled_w = inner_w * scale;
			const auto scaled_h = inner_h * scale;
			const auto fill_x = abs.x + pad + ( inner_w - scaled_w ) * 0.5f;
			const auto fill_y = abs.y + pad + ( inner_h - scaled_h ) * 0.5f;

			const auto check_col = style.checkbox_check;
			const auto col_top = zdraw::rgba
			{
				static_cast< std::uint8_t >( std::min( check_col.r * ( 1.1f + ease_t * 0.15f ), 255.0f ) ),
				static_cast< std::uint8_t >( std::min( check_col.g * ( 1.1f + ease_t * 0.15f ), 255.0f ) ),
				static_cast< std::uint8_t >( std::min( check_col.b * ( 1.1f + ease_t * 0.15f ), 255.0f ) ),
				static_cast< std::uint8_t >( check_col.a * ease_t )
			};

			const auto col_bottom = zdraw::rgba
			{
				static_cast< std::uint8_t >( check_col.r * 0.75f ),
				static_cast< std::uint8_t >( check_col.g * 0.75f ),
				static_cast< std::uint8_t >( check_col.b * 0.75f ),
				static_cast< std::uint8_t >( check_col.a * ease_t )
			};

			zdraw::rect_filled_multi_color( fill_x, fill_y, scaled_w, scaled_h, col_top, col_top, col_bottom, col_bottom );

			if ( ease_t < 0.4f && ease_t > 0.0f )
			{
				const auto pulse_t = ease_t / 0.4f;
				const auto ring_expand = 3.0f * pulse_t;
				const auto ring_alpha = static_cast< std::uint8_t >( 60 * ( 1.0f - pulse_t ) * ease_t );

				auto ring_col = check_col;
				ring_col.a = ring_alpha;

				zdraw::rect( abs.x - ring_expand, abs.y - ring_expand, abs.w + ring_expand * 2.0f, abs.h + ring_expand * 2.0f, ring_col, 1.5f );
			}
		}

		if ( !display_label.empty( ) )
		{
			const auto text_x = abs.x + abs.w + style.item_spacing_x;
			const auto text_y = abs.y + ( check_size - label_h ) * 0.5f;

			zdraw::text( text_x, text_y, display_label, style.text );
		}

		return changed;
	}

} // namespace zui

#endif // !CHECKBOX_HPP