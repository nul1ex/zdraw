#ifndef BUTTON_HPP
#define BUTTON_HPP

#include "../context.hpp"
#include "../layout.hpp"
#include "../color.hpp"

#include <include/zdraw/zdraw.hpp>

namespace zui {

	inline bool button( std::string_view label, float w, float h )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return false;
		}

		const auto& style = ctx( ).get_style( );
		auto& input = ctx( ).input( );

		auto local = item_add( w, h );
		auto abs = to_absolute( local );

		const auto can_interact = !ctx( ).overlay_blocking_input( );
		const auto hovered = can_interact && input.hovered( abs );
		const auto held = hovered && input.mouse_down( );
		const auto pressed = hovered && input.mouse_clicked( );

		const auto bg_col = held ? style.button_active : ( hovered ? style.button_hovered : style.button_bg );
		const auto border_col = hovered ? lighten( style.button_border, 1.2f ) : style.button_border;

		const auto col_top = bg_col;
		const auto col_bottom = darken( bg_col, 0.85f );

		zdraw::rect_filled_multi_color( abs.x, abs.y, abs.w, abs.h, col_top, col_top, col_bottom, col_bottom );
		zdraw::rect( abs.x, abs.y, abs.w, abs.h, border_col );

		const auto display_label = context::get_display_label( label );
		if ( !display_label.empty( ) )
		{
			auto [label_w, label_h] = zdraw::measure_text( display_label );
			const auto text_x = abs.x + ( abs.w - label_w ) * 0.5f;
			const auto text_y = abs.y + ( abs.h - label_h ) * 0.5f;

			zdraw::text( text_x, text_y, display_label, style.text );
		}

		return pressed;
	}

} // namespace zui

#endif // !BUTTON_HPP