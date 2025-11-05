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

		zdraw::rgba window_bg{ 18, 18, 22, 255 };
		zdraw::rgba window_border{ 65, 75, 95, 255 };
		zdraw::rgba nested_bg{ 24, 24, 30, 255 };
		zdraw::rgba nested_border{ 75, 85, 105, 255 };

		zdraw::rgba group_box_bg{ 26, 28, 34, 255 };
		zdraw::rgba group_box_border{ 70, 80, 100, 255 };
		zdraw::rgba group_box_title_text{ 200, 210, 220, 255 };

		zdraw::rgba checkbox_bg{ 30, 32, 38, 255 };
		zdraw::rgba checkbox_border{ 70, 80, 100, 255 };
		zdraw::rgba checkbox_check{ 145, 160, 210, 255 };

		zdraw::rgba slider_bg{ 30, 32, 38, 255 };
		zdraw::rgba slider_border{ 70, 80, 100, 255 };
		zdraw::rgba slider_fill{ 145, 160, 210, 255 };
		zdraw::rgba slider_grab{ 165, 180, 230, 255 };
		zdraw::rgba slider_grab_active{ 185, 200, 240, 255 };

		zdraw::rgba button_bg{ 40, 45, 60, 255 };
		zdraw::rgba button_border{ 85, 95, 120, 255 };
		zdraw::rgba button_hovered{ 55, 60, 80, 255 };
		zdraw::rgba button_active{ 30, 35, 50, 255 };

		zdraw::rgba keybind_bg{ 30, 32, 38, 255 };
		zdraw::rgba keybind_border{ 70, 80, 100, 255 };
		zdraw::rgba keybind_waiting{ 145, 160, 210, 255 };

		zdraw::rgba combo_bg{ 30, 32, 38, 255 };
		zdraw::rgba combo_border{ 70, 80, 100, 255 };
		zdraw::rgba combo_arrow{ 145, 160, 210, 255 };
		zdraw::rgba combo_hovered{ 55, 60, 80, 255 };
		zdraw::rgba combo_popup_bg{ 24, 24, 30, 255 };
		zdraw::rgba combo_popup_border{ 85, 95, 120, 255 };
		zdraw::rgba combo_item_hovered{ 55, 60, 80, 255 };
		zdraw::rgba combo_item_selected{ 145, 160, 210, 50 };

		zdraw::rgba text{ 235, 240, 245, 255 };
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

	bool begin_window( std::string_view title, float& x, float& y, float w, float h );
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

	bool checkbox( std::string_view label, bool& v );
	bool button( std::string_view label, float w, float h );
	bool keybind( std::string_view label, int& key );

	bool slider_float( std::string_view label, float& v, float v_min, float v_max, std::string_view format = "%.3f" );
	bool slider_int( std::string_view label, int& v, int v_min, int v_max );

	bool combo( std::string_view label, int& current_item, const char* const items[ ], int items_count, float width = 0.0f );

} // namespace zui

#endif // !ZUI_HPP