#ifndef STYLE_HPP
#define STYLE_HPP

#include <include/zdraw/zdraw.hpp>

namespace zui {

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
        float checkbox_size{ 14.0f };
        float slider_height{ 10.0f };
        float keybind_width{ 80.0f };
        float keybind_height{ 20.0f };
        float combo_height{ 22.0f };
        float combo_item_height{ 20.0f };
        float color_picker_swatch_size{ 14.0f };
        float color_picker_popup_width{ 180.0f };
        float color_picker_popup_height{ 220.0f };
        float text_input_height{ 22.0f };

        float scrollbar_width{ 6.0f };
        float scrollbar_padding{ 2.0f };
        float scrollbar_min_thumb_height{ 20.0f };

        zdraw::rgba window_bg{ 18, 18, 18, 255 };
        zdraw::rgba window_border{ 50, 50, 50, 255 };

        zdraw::rgba nested_bg{ 10, 10, 10, 255 };
        zdraw::rgba nested_border{ 50, 50, 50, 255 };

        zdraw::rgba group_box_bg{ 10, 10, 10, 255 };
        zdraw::rgba group_box_border{ 40, 40, 40, 255 };
        zdraw::rgba group_box_title_text{ 180, 180, 180, 255 };

        zdraw::rgba checkbox_bg{ 32, 32, 32, 255 };
        zdraw::rgba checkbox_border{ 60, 60, 60, 255 };
        zdraw::rgba checkbox_check{ 220, 130, 160, 255 };

        zdraw::rgba slider_bg{ 32, 32, 32, 255 };
        zdraw::rgba slider_border{ 60, 60, 60, 255 };
        zdraw::rgba slider_fill{ 220, 130, 160, 255 };
        zdraw::rgba slider_grab{ 220, 130, 160, 255 };
        zdraw::rgba slider_grab_active{ 240, 155, 185, 255 };

        zdraw::rgba button_bg{ 32, 32, 32, 255 };
        zdraw::rgba button_border{ 60, 60, 60, 255 };
        zdraw::rgba button_hovered{ 42, 42, 42, 255 };
        zdraw::rgba button_active{ 28, 28, 28, 255 };

        zdraw::rgba keybind_bg{ 32, 32, 32, 255 };
        zdraw::rgba keybind_border{ 60, 60, 60, 255 };
        zdraw::rgba keybind_waiting{ 220, 130, 160, 255 };

        zdraw::rgba combo_bg{ 32, 32, 32, 255 };
        zdraw::rgba combo_border{ 60, 60, 60, 255 };
        zdraw::rgba combo_arrow{ 150, 150, 150, 255 };
        zdraw::rgba combo_hovered{ 42, 42, 42, 255 };
        zdraw::rgba combo_popup_bg{ 24, 24, 24, 255 };
        zdraw::rgba combo_popup_border{ 50, 50, 50, 255 };
        zdraw::rgba combo_item_hovered{ 42, 42, 42, 255 };
        zdraw::rgba combo_item_selected{ 220, 130, 160, 40 };

        zdraw::rgba color_picker_bg{ 32, 32, 32, 255 };
        zdraw::rgba color_picker_border{ 60, 60, 60, 255 };

        zdraw::rgba text_input_bg{ 32, 32, 32, 255 };
        zdraw::rgba text_input_border{ 60, 60, 60, 255 };

        zdraw::rgba scrollbar_bg{ 24, 24, 24, 100 };
        zdraw::rgba scrollbar_thumb{ 80, 80, 80, 200 };
        zdraw::rgba scrollbar_thumb_hovered{ 100, 100, 100, 220 };
        zdraw::rgba scrollbar_thumb_active{ 220, 130, 160, 255 };

        zdraw::rgba text{ 225, 225, 225, 255 };
        zdraw::rgba accent{ 220, 130, 160, 255 };
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
		color_picker_bg,
		color_picker_border,
		text_input_bg,
		text_input_border,
		text,
		accent
	};

} // namespace zui

#endif // !STYLE_HPP