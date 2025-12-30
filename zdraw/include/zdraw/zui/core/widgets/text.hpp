#ifndef TEXT_HPP
#define TEXT_HPP

#include "../context.hpp"
#include "../layout.hpp"

#include <include/zdraw/zdraw.hpp>

namespace zui {

	inline void text( std::string_view label )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		auto [label_w, label_h] = zdraw::measure_text( label );
		auto local = item_add( label_w, label_h );
		auto abs = to_absolute( local );

		std::string label_str( label.begin( ), label.end( ) );
		zdraw::text( abs.x, abs.y, label_str, ctx( ).get_style( ).text );
	}

	inline void text_colored( std::string_view label, const zdraw::rgba& color )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		auto [label_w, label_h] = zdraw::measure_text( label );
		auto local = item_add( label_w, label_h );
		auto abs = to_absolute( local );

		std::string label_str( label.begin( ), label.end( ) );
		zdraw::text( abs.x, abs.y, label_str, color );
	}

	inline void text_gradient( std::string_view label, const zdraw::rgba& color_left, const zdraw::rgba& color_right )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		auto [label_w, label_h] = zdraw::measure_text( label );
		auto local = item_add( label_w, label_h );
		auto abs = to_absolute( local );

		std::string label_str( label.begin( ), label.end( ) );
		zdraw::text_multi_color( abs.x, abs.y, label_str, color_left, color_right, color_right, color_left );
	}

	inline void text_gradient_vertical( std::string_view label, const zdraw::rgba& color_top, const zdraw::rgba& color_bottom )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		auto [label_w, label_h] = zdraw::measure_text( label );
		auto local = item_add( label_w, label_h );
		auto abs = to_absolute( local );

		std::string label_str( label.begin( ), label.end( ) );
		zdraw::text_multi_color( abs.x, abs.y, label_str, color_top, color_top, color_bottom, color_bottom );
	}

	inline void text_gradient_four( std::string_view label, const zdraw::rgba& color_tl, const zdraw::rgba& color_tr, const zdraw::rgba& color_br, const zdraw::rgba& color_bl )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		auto [label_w, label_h] = zdraw::measure_text( label );
		auto local = item_add( label_w, label_h );
		auto abs = to_absolute( local );

		std::string label_str( label.begin( ), label.end( ) );
		zdraw::text_multi_color( abs.x, abs.y, label_str, color_tl, color_tr, color_br, color_bl );
	}

} // namespace zui

#endif // !TEXT_HPP