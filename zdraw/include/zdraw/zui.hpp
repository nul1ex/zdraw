#ifndef ZUI_HPP
#define ZUI_HPP

#include <include/zdraw/zdraw.hpp>

namespace zui {

	struct rect
	{
		float m_x{ 0.0f };
		float m_y{ 0.0f };
		float m_w{ 0.0f };
		float m_h{ 0.0f };
	};

	struct style
	{
		float window_padding_x{ 8.0f };
		float window_padding_y{ 8.0f };
		float item_spacing_x{ 8.0f };
		float item_spacing_y{ 8.0f };
		float frame_padding_x{ 6.0f };
		float frame_padding_y{ 3.0f };
		float window_rounding{ 0.0f };
		float border_thickness{ 1.0f };
		float group_box_title_height{ 16.0f };
		float checkbox_size{ 16.0f };
		float slider_height{ 10.0f };
		float keybind_height{ 18.0f };
		float combo_height{ 20.0f };
		float combo_item_height{ 18.0f };

		zdraw::rgba window_bg{ 245, 242, 252, 255 };
		zdraw::rgba window_border{ 200, 190, 225, 255 };
		zdraw::rgba nested_bg{ 240, 237, 248, 255 };
		zdraw::rgba nested_border{ 195, 185, 220, 255 };

		zdraw::rgba group_box_bg{ 243, 240, 250, 255 };
		zdraw::rgba group_box_border{ 190, 180, 215, 255 };
		zdraw::rgba group_box_title_text{ 100, 85, 140, 255 };

		zdraw::rgba checkbox_bg{ 250, 248, 255, 255 };
		zdraw::rgba checkbox_border{ 190, 180, 215, 255 };
		zdraw::rgba checkbox_check{ 160, 140, 210, 255 };

		zdraw::rgba slider_bg{ 250, 248, 255, 255 };
		zdraw::rgba slider_border{ 190, 180, 215, 255 };
		zdraw::rgba slider_fill{ 160, 140, 210, 255 };
		zdraw::rgba slider_grab{ 180, 160, 225, 255 };
		zdraw::rgba slider_grab_active{ 140, 120, 195, 255 };

		zdraw::rgba button_bg{ 238, 235, 248, 255 };
		zdraw::rgba button_border{ 185, 175, 210, 255 };
		zdraw::rgba button_hovered{ 248, 243, 255, 255 };
		zdraw::rgba button_active{ 228, 223, 240, 255 };

		zdraw::rgba keybind_bg{ 250, 248, 255, 255 };
		zdraw::rgba keybind_border{ 190, 180, 215, 255 };
		zdraw::rgba keybind_waiting{ 160, 140, 210, 255 };

		zdraw::rgba combo_bg{ 250, 248, 255, 255 };
		zdraw::rgba combo_border{ 190, 180, 215, 255 };
		zdraw::rgba combo_arrow{ 160, 140, 210, 255 };
		zdraw::rgba combo_hovered{ 248, 243, 255, 255 };
		zdraw::rgba combo_popup_bg{ 245, 242, 252, 255 };
		zdraw::rgba combo_popup_border{ 185, 175, 210, 255 };
		zdraw::rgba combo_item_hovered{ 248, 243, 255, 255 };
		zdraw::rgba combo_item_selected{ 180, 160, 225, 80 };

		zdraw::rgba text{ 70, 60, 100, 255 };
	};

	enum class style_var
	{
		window_padding_x,
		window_padding_y,
		item_spacing_x,
		item_spacing_y,
		frame_padding_x,
		frame_padding_y,
		window_rounding,
		border_thickness,
		group_box_title_height,
		checkbox_size,
		slider_height,
		keybind_height,
		combo_height,
		combo_item_height
	};

	enum class style_color
	{
		window_bg,
		window_border,
		nested_bg,
		nested_border,
		group_box_bg,
		group_box_border,
		group_box_title_text,
		checkbox_bg,
		checkbox_border,
		checkbox_check,
		slider_bg,
		slider_border,
		slider_fill,
		slider_grab,
		slider_grab_active,
		button_bg,
		button_border,
		button_hovered,
		button_active,
		keybind_bg,
		keybind_border,
		keybind_waiting,
		combo_bg,
		combo_border,
		combo_arrow,
		combo_hovered,
		combo_popup_bg,
		combo_popup_border,
		combo_item_hovered,
		combo_item_selected,
		text
	};

	bool initialize( HWND hwnd );

	void begin( );
	void end( );

	bool begin_window( std::string_view title, float& x, float& y, float& w, float& h, bool resizable = false, float min_w = 200.0f, float min_h = 200.0f );
	void end_window( );

	bool begin_nested_window( std::string_view title, float w, float h );
	void end_nested_window( );

	bool begin_group_box( std::string_view title, float w, float h );
	void end_group_box( );

	std::pair<float, float> get_cursor_pos( );
	void set_cursor_pos( float x, float y );

	std::pair<float, float> get_content_region_avail( );
	float calc_item_width( int count );

	style& get_style( );

	void push_style_var( style_var var, float value );
	void pop_style_var( int count = 1 );

	void push_style_color( style_color idx, const zdraw::rgba& col );
	void pop_style_color( int count = 1 );

	void same_line( float offset_x = 0.0f );
	void new_line( );

	bool checkbox( std::string_view label, bool& v );
	bool button( std::string_view label, float w, float h );
	bool keybind( std::string_view label, int& key );

	bool slider_float( std::string_view label, float& v, float v_min, float v_max, std::string_view format = "%.3f" );
	bool slider_int( std::string_view label, int& v, int v_min, int v_max );

	bool combo( std::string_view label, int& current_item, const char* const items[ ], int items_count, float width = 0.0f );
	bool color_picker( std::string_view label, zdraw::rgba& color, float width = 0.0f );

	enum class color_preset
	{
		dark_blue,
		light_pink,
		mint_green,
		dark_white_accent,
		pastel_lavender,
		pastel_peach,
		pastel_sky
	};

	void apply_color_preset( color_preset preset );

} // namespace zui

#endif // !ZUI_HPP