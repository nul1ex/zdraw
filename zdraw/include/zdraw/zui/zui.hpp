#ifndef ZUI_HPP
#define ZUI_HPP

#include "core/types.hpp"
#include "core/style.hpp"
#include "core/color.hpp"
#include "core/input.hpp"
#include "core/animation.hpp"
#include "core/context.hpp"
#include "core/layout.hpp"
#include "core/widgets.hpp"

namespace zui {

	inline bool initialize( HWND hwnd )
	{
		return ctx( ).initialize( hwnd );
	}

	inline void begin( )
	{
		ctx( ).begin_frame( );
	}

	inline void end( )
	{
		ctx( ).end_frame( );
	}

	inline style& get_style( )
	{
		return ctx( ).get_style( );
	}

	inline zdraw::rgba get_accent_color( )
	{
		return ctx( ).get_style( ).accent;
	}

	inline void push_style_var( style_var var, float value )
	{
		float* ptr = nullptr;
		auto& s = ctx( ).get_style( );

		switch ( var )
		{
		case style_var::window_padding_x: ptr = &s.window_padding_x; break;
		case style_var::window_padding_y: ptr = &s.window_padding_y; break;
		case style_var::item_spacing_x: ptr = &s.item_spacing_x; break;
		case style_var::item_spacing_y: ptr = &s.item_spacing_y; break;
		case style_var::frame_padding_x: ptr = &s.frame_padding_x; break;
		case style_var::frame_padding_y: ptr = &s.frame_padding_y; break;
		case style_var::border_thickness: ptr = &s.border_thickness; break;
		case style_var::group_box_title_height: ptr = &s.group_box_title_height; break;
		case style_var::checkbox_size: ptr = &s.checkbox_size; break;
		case style_var::slider_height: ptr = &s.slider_height; break;
		case style_var::keybind_height: ptr = &s.keybind_height; break;
		case style_var::combo_height: ptr = &s.combo_height; break;
		case style_var::combo_item_height: ptr = &s.combo_item_height; break;
		default: return;
		}

		if ( ptr )
		{
			ctx( ).m_style_var_stack.push_back( { var, *ptr } );
			*ptr = value;
		}
	}

	inline void pop_style_var( int count = 1 )
	{
		auto& s = ctx( ).get_style( );
		auto& stack = ctx( ).m_style_var_stack;

		for ( int i = 0; i < count && !stack.empty( ); ++i )
		{
			const auto& backup = stack.back( );
			float* ptr = nullptr;

			switch ( backup.var )
			{
			case style_var::window_padding_x: ptr = &s.window_padding_x; break;
			case style_var::window_padding_y: ptr = &s.window_padding_y; break;
			case style_var::item_spacing_x: ptr = &s.item_spacing_x; break;
			case style_var::item_spacing_y: ptr = &s.item_spacing_y; break;
			case style_var::frame_padding_x: ptr = &s.frame_padding_x; break;
			case style_var::frame_padding_y: ptr = &s.frame_padding_y; break;
			case style_var::border_thickness: ptr = &s.border_thickness; break;
			case style_var::group_box_title_height: ptr = &s.group_box_title_height; break;
			case style_var::checkbox_size: ptr = &s.checkbox_size; break;
			case style_var::slider_height: ptr = &s.slider_height; break;
			case style_var::keybind_height: ptr = &s.keybind_height; break;
			case style_var::combo_height: ptr = &s.combo_height; break;
			case style_var::combo_item_height: ptr = &s.combo_item_height; break;
			default: break;
			}

			if ( ptr )
			{
				*ptr = backup.prev;
			}

			stack.pop_back( );
		}
	}

	inline void push_style_color( style_color idx, const zdraw::rgba& col )
	{
		zdraw::rgba* ptr = nullptr;
		auto& s = ctx( ).get_style( );

		switch ( idx )
		{
		case style_color::window_bg: ptr = &s.window_bg; break;
		case style_color::window_border: ptr = &s.window_border; break;
		case style_color::nested_bg: ptr = &s.nested_bg; break;
		case style_color::nested_border: ptr = &s.nested_border; break;
		case style_color::group_box_bg: ptr = &s.group_box_bg; break;
		case style_color::group_box_border: ptr = &s.group_box_border; break;
		case style_color::group_box_title_text: ptr = &s.group_box_title_text; break;
		case style_color::checkbox_bg: ptr = &s.checkbox_bg; break;
		case style_color::checkbox_border: ptr = &s.checkbox_border; break;
		case style_color::checkbox_check: ptr = &s.checkbox_check; break;
		case style_color::slider_bg: ptr = &s.slider_bg; break;
		case style_color::slider_border: ptr = &s.slider_border; break;
		case style_color::slider_fill: ptr = &s.slider_fill; break;
		case style_color::slider_grab: ptr = &s.slider_grab; break;
		case style_color::slider_grab_active: ptr = &s.slider_grab_active; break;
		case style_color::button_bg: ptr = &s.button_bg; break;
		case style_color::button_border: ptr = &s.button_border; break;
		case style_color::button_hovered: ptr = &s.button_hovered; break;
		case style_color::button_active: ptr = &s.button_active; break;
		case style_color::keybind_bg: ptr = &s.keybind_bg; break;
		case style_color::keybind_border: ptr = &s.keybind_border; break;
		case style_color::keybind_waiting: ptr = &s.keybind_waiting; break;
		case style_color::combo_bg: ptr = &s.combo_bg; break;
		case style_color::combo_border: ptr = &s.combo_border; break;
		case style_color::combo_arrow: ptr = &s.combo_arrow; break;
		case style_color::combo_hovered: ptr = &s.combo_hovered; break;
		case style_color::combo_popup_bg: ptr = &s.combo_popup_bg; break;
		case style_color::combo_popup_border: ptr = &s.combo_popup_border; break;
		case style_color::combo_item_hovered: ptr = &s.combo_item_hovered; break;
		case style_color::combo_item_selected: ptr = &s.combo_item_selected; break;
		case style_color::color_picker_bg: ptr = &s.color_picker_bg; break;
		case style_color::color_picker_border: ptr = &s.color_picker_border; break;
		case style_color::text: ptr = &s.text; break;
		case style_color::accent: ptr = &s.accent; break;
		default: return;
		}

		if ( ptr )
		{
			ctx( ).m_style_color_stack.push_back( { idx, *ptr } );
			*ptr = col;
		}
	}

	inline void pop_style_color( int count = 1 )
	{
		auto& s = ctx( ).get_style( );
		auto& stack = ctx( ).m_style_color_stack;

		for ( int i = 0; i < count && !stack.empty( ); ++i )
		{
			const auto& backup = stack.back( );
			zdraw::rgba* ptr = nullptr;

			switch ( backup.idx )
			{
			case style_color::window_bg: ptr = &s.window_bg; break;
			case style_color::window_border: ptr = &s.window_border; break;
			case style_color::nested_bg: ptr = &s.nested_bg; break;
			case style_color::nested_border: ptr = &s.nested_border; break;
			case style_color::group_box_bg: ptr = &s.group_box_bg; break;
			case style_color::group_box_border: ptr = &s.group_box_border; break;
			case style_color::group_box_title_text: ptr = &s.group_box_title_text; break;
			case style_color::checkbox_bg: ptr = &s.checkbox_bg; break;
			case style_color::checkbox_border: ptr = &s.checkbox_border; break;
			case style_color::checkbox_check: ptr = &s.checkbox_check; break;
			case style_color::slider_bg: ptr = &s.slider_bg; break;
			case style_color::slider_border: ptr = &s.slider_border; break;
			case style_color::slider_fill: ptr = &s.slider_fill; break;
			case style_color::slider_grab: ptr = &s.slider_grab; break;
			case style_color::slider_grab_active: ptr = &s.slider_grab_active; break;
			case style_color::button_bg: ptr = &s.button_bg; break;
			case style_color::button_border: ptr = &s.button_border; break;
			case style_color::button_hovered: ptr = &s.button_hovered; break;
			case style_color::button_active: ptr = &s.button_active; break;
			case style_color::keybind_bg: ptr = &s.keybind_bg; break;
			case style_color::keybind_border: ptr = &s.keybind_border; break;
			case style_color::keybind_waiting: ptr = &s.keybind_waiting; break;
			case style_color::combo_bg: ptr = &s.combo_bg; break;
			case style_color::combo_border: ptr = &s.combo_border; break;
			case style_color::combo_arrow: ptr = &s.combo_arrow; break;
			case style_color::combo_hovered: ptr = &s.combo_hovered; break;
			case style_color::combo_popup_bg: ptr = &s.combo_popup_bg; break;
			case style_color::combo_popup_border: ptr = &s.combo_popup_border; break;
			case style_color::combo_item_hovered: ptr = &s.combo_item_hovered; break;
			case style_color::combo_item_selected: ptr = &s.combo_item_selected; break;
			case style_color::color_picker_bg: ptr = &s.color_picker_bg; break;
			case style_color::color_picker_border: ptr = &s.color_picker_border; break;
			case style_color::text: ptr = &s.text; break;
			case style_color::accent: ptr = &s.accent; break;
			default: break;
			}

			if ( ptr )
			{
				*ptr = backup.prev;
			}

			stack.pop_back( );
		}
	}

} // namespace zui

#endif // !ZUI_HPP