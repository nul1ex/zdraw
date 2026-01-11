#pragma once

#include <include/zdraw/zdraw.hpp>

namespace zui {

	struct rect
	{
		float x{ 0.0f };
		float y{ 0.0f };
		float w{ 0.0f };
		float h{ 0.0f };

		[[nodiscard]] constexpr bool contains( float px, float py ) const noexcept
		{
			return px >= this->x && px <= this->x + this->w && py >= this->y && py <= this->y + this->h;
		}

		[[nodiscard]] constexpr rect offset( float dx, float dy ) const noexcept
		{
			return { this->x + dx, this->y + dy, this->w, this->h };
		}

		[[nodiscard]] constexpr rect expand( float amount ) const noexcept
		{
			return { this->x - amount, this->y - amount, this->w + amount * 2.0f, this->h + amount * 2.0f };
		}

		[[nodiscard]] constexpr float right( ) const noexcept { return this->x + this->w; }
		[[nodiscard]] constexpr float bottom( ) const noexcept { return this->y + this->h; }
		[[nodiscard]] constexpr float center_x( ) const noexcept { return this->x + this->w * 0.5f; }
		[[nodiscard]] constexpr float center_y( ) const noexcept { return this->y + this->h * 0.5f; }
	};

	using widget_id = std::uintptr_t;
	constexpr widget_id invalid_id{ 0 };

	struct window_state
	{
		std::string title{};
		rect bounds{};
		float cursor_x{ 0.0f };
		float cursor_y{ 0.0f };
		float line_height{ 0.0f };
		rect last_item{};
		bool is_child{ false };

		float scroll_y{ 0.0f };
		float content_height{ 0.0f };
		float visible_start_y{ 0.0f };
		widget_id scroll_id{ invalid_id };
	};

	namespace detail {

		float mouse_x( );
		float mouse_y( );
		bool mouse_down( );
		bool mouse_clicked( );
		bool mouse_released( );
		bool mouse_hovered( const rect& r );
		window_state* get_current_window( );

	} // namespace detail

	struct hsv
	{
		float h{ 0.0f };
		float s{ 0.0f };
		float v{ 0.0f };
		float a{ 1.0f };
	};

	struct style
	{
		float window_padding_x{ 10.0f };
		float window_padding_y{ 10.0f };
		float item_spacing_x{ 8.0f };
		float item_spacing_y{ 6.0f };
		float frame_padding_x{ 6.0f };
		float frame_padding_y{ 3.0f };
		float border_thickness{ 1.0f };

		float group_box_title_height{ 18.0f };
		float checkbox_size{ 12.0f };
		float slider_height{ 10.0f };
		float keybind_width{ 80.0f };
		float keybind_height{ 20.0f };
		float combo_height{ 22.0f };
		float combo_item_height{ 20.0f };
		float color_picker_swatch_width{ 24.0f };
		float color_picker_swatch_height{ 12.0f };
		float color_picker_popup_width{ 180.0f };
		float color_picker_popup_height{ 220.0f };
		float text_input_height{ 22.0f };

		zdraw::rgba window_bg{ 18, 18, 18, 255 };
		zdraw::rgba window_border{ 45, 45, 45, 255 };

		zdraw::rgba nested_bg{ 12, 12, 12, 255 };
		zdraw::rgba nested_border{ 40, 40, 40, 255 };

		zdraw::rgba group_box_bg{ 14, 14, 14, 255 };
		zdraw::rgba group_box_border{ 38, 38, 38, 255 };
		zdraw::rgba group_box_title_text{ 170, 170, 170, 255 };

		zdraw::rgba checkbox_bg{ 24, 24, 24, 255 };
		zdraw::rgba checkbox_border{ 50, 50, 50, 255 };
		zdraw::rgba checkbox_check{ 160, 170, 220, 255 };

		zdraw::rgba slider_bg{ 24, 24, 24, 255 };
		zdraw::rgba slider_border{ 50, 50, 50, 255 };
		zdraw::rgba slider_fill{ 145, 155, 205, 255 };
		zdraw::rgba slider_grab{ 160, 170, 220, 255 };
		zdraw::rgba slider_grab_active{ 180, 190, 235, 255 };

		zdraw::rgba button_bg{ 28, 28, 28, 255 };
		zdraw::rgba button_border{ 52, 52, 52, 255 };
		zdraw::rgba button_hovered{ 38, 38, 38, 255 };
		zdraw::rgba button_active{ 22, 22, 22, 255 };

		zdraw::rgba keybind_bg{ 24, 24, 24, 255 };
		zdraw::rgba keybind_border{ 50, 50, 50, 255 };
		zdraw::rgba keybind_waiting{ 160, 170, 220, 255 };

		zdraw::rgba combo_bg{ 24, 24, 24, 255 };
		zdraw::rgba combo_border{ 50, 50, 50, 255 };
		zdraw::rgba combo_arrow{ 140, 140, 140, 255 };
		zdraw::rgba combo_hovered{ 34, 34, 34, 255 };
		zdraw::rgba combo_popup_bg{ 16, 16, 16, 255 };
		zdraw::rgba combo_popup_border{ 45, 45, 45, 255 };
		zdraw::rgba combo_item_hovered{ 36, 36, 36, 255 };
		zdraw::rgba combo_item_selected{ 160, 170, 220, 35 };

		zdraw::rgba color_picker_bg{ 24, 24, 24, 255 };
		zdraw::rgba color_picker_border{ 50, 50, 50, 255 };

		zdraw::rgba text_input_bg{ 24, 24, 24, 255 };
		zdraw::rgba text_input_border{ 50, 50, 50, 255 };

		zdraw::rgba text{ 215, 215, 215, 255 };
		zdraw::rgba accent{ 160, 170, 220, 255 };
	};

	enum class style_var
	{
		window_padding_x,
		window_padding_y,
		item_spacing_x,
		item_spacing_y,
		frame_padding_x,
		frame_padding_y,
		border_thickness,
		group_box_title_height,
		checkbox_size,
		slider_height,
		keybind_height,
		combo_height,
		combo_item_height,
		color_picker_swatch_width,
		color_picker_swatch_height
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
		color_picker_bg,
		color_picker_border,
		text_input_bg,
		text_input_border,
		text,
		accent
	};

	[[nodiscard]] zdraw::rgba lighten( const zdraw::rgba& color, float factor ) noexcept;
	[[nodiscard]] zdraw::rgba darken( const zdraw::rgba& color, float factor ) noexcept;
	[[nodiscard]] zdraw::rgba alpha( const zdraw::rgba& color, float a ) noexcept;
	[[nodiscard]] zdraw::rgba alpha( const zdraw::rgba& color, std::uint8_t a ) noexcept;
	[[nodiscard]] zdraw::rgba lerp( const zdraw::rgba& a, const zdraw::rgba& b, float t ) noexcept;
	[[nodiscard]] hsv rgb_to_hsv( const zdraw::rgba& color ) noexcept;
	[[nodiscard]] zdraw::rgba hsv_to_rgb( const hsv& color ) noexcept;
	[[nodiscard]] zdraw::rgba hsv_to_rgb( float h, float s, float v, float a = 1.0f ) noexcept;

	namespace ease {

		[[nodiscard]] float linear( float t ) noexcept;
		[[nodiscard]] float in_quad( float t ) noexcept;
		[[nodiscard]] float out_quad( float t ) noexcept;
		[[nodiscard]] float in_out_quad( float t ) noexcept;
		[[nodiscard]] float in_cubic( float t ) noexcept;
		[[nodiscard]] float out_cubic( float t ) noexcept;
		[[nodiscard]] float in_out_cubic( float t ) noexcept;
		[[nodiscard]] float smoothstep( float t ) noexcept;

	} // namespace ease

	bool initialize( HWND hwnd );
	void begin( );
	void end( );

	bool process_wndproc_message( UINT msg, WPARAM wparam, LPARAM lparam );

	style& get_style( );
	zdraw::rgba get_accent_color( );

	void push_style_var( style_var var, float value );
	void pop_style_var( int count = 1 );
	void push_style_color( style_color idx, const zdraw::rgba& col );
	void pop_style_color( int count = 1 );

	bool begin_window( std::string_view title, float& x, float& y, float& w, float& h, bool resizable = false, float min_w = 200.0f, float min_h = 200.0f );
	void end_window( );

	bool begin_nested_window( std::string_view title, float w, float h );
	void end_nested_window( );

	bool begin_group_box( std::string_view title, float w, float h = 0.0f );
	void end_group_box( );

	void same_line( float offset_x = 0.0f );
	void new_line( );
	void spacing( float amount = 0.0f );
	void indent( float amount = 0.0f );
	void unindent( float amount = 0.0f );
	void separator( );

	std::pair<float, float> get_content_region_avail( );
	float calc_item_width( int count = 0 );
	void set_cursor_pos( float x, float y );
	std::pair<float, float> get_cursor_pos( );

	void text( std::string_view label );
	void text_colored( std::string_view label, const zdraw::rgba& color );
	void text_gradient( std::string_view label, const zdraw::rgba& color_left, const zdraw::rgba& color_right );
	void text_gradient_vertical( std::string_view label, const zdraw::rgba& color_top, const zdraw::rgba& color_bottom );
	void text_gradient_four( std::string_view label, const zdraw::rgba& color_tl, const zdraw::rgba& color_tr, const zdraw::rgba& color_br, const zdraw::rgba& color_bl );

	bool button( std::string_view label, float w, float h );
	bool checkbox( std::string_view label, bool& v );

	bool slider_float( std::string_view label, float& v, float v_min, float v_max, std::string_view format = "%.2f" );
	bool slider_int( std::string_view label, int& v, int v_min, int v_max, std::string_view format = "%d" );

	bool keybind( std::string_view label, int& key );

	bool combo( std::string_view label, int& current_item, const char* const items[ ], int items_count, float width = 0.0f );
	bool multicombo( std::string_view label, bool* selected_items, const char* const items[ ], int items_count, float width = 0.0f );

	bool color_picker( std::string_view label, zdraw::rgba& color, float width = 0.0f, bool show_alpha = true );

	bool text_input( std::string_view label, std::string& text, std::size_t max_length = 256, std::string_view hint = "" );

} // namespace zui