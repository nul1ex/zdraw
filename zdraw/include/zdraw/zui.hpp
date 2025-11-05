#ifndef ZUI_HPP
#define ZUI_HPP

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
        float WindowPaddingX{ 8.0f };
        float WindowPaddingY{ 8.0f };
        float ItemSpacingX{ 8.0f };
        float ItemSpacingY{ 4.0f };
        float FramePaddingX{ 6.0f };
        float FramePaddingY{ 3.0f };
        float WindowRounding{ 0.0f };
        float BorderThickness{ 1.0f };

        zdraw::rgba WindowBg{ 22, 22, 22, 220 };
        zdraw::rgba WindowBorder{ 60, 60, 60, 255 };

        zdraw::rgba NestedBg{ 28, 28, 28, 220 };
        zdraw::rgba NestedBorder{ 70, 70, 70, 255 };
    };

    enum class style_var
    {
        WindowPaddingX,
        WindowPaddingY,
        ItemSpacingX,
        ItemSpacingY,
        FramePaddingX,
        FramePaddingY,
        WindowRounding,
        BorderThickness
    };

    enum class style_color
    {
        WindowBg,
        WindowBorder,
        ChildBg,
        ChildBorder,
        NestedBg,
        NestedBorder
    };

    bool initialize( HWND hwnd ) noexcept;
    void shutdown( ) noexcept;

    void begin_frame( ) noexcept;
    void end_frame( ) noexcept;

    bool begin_window( std::string_view title, float& x, float& y, float w, float h ) noexcept;
    void end_window( ) noexcept;

    bool begin_nested_window( std::string_view title, float w, float h ) noexcept;
    void end_nested_window( ) noexcept;

    std::pair<float, float> get_cursor_pos( ) noexcept;
    void set_cursor_pos( float x, float y ) noexcept;

    std::pair<float, float> get_content_region_avail( ) noexcept;
    style& get_style( ) noexcept;

    void push_style_var( style_var var, float value ) noexcept;
    void pop_style_var( int count = 1 ) noexcept;

    void push_style_color( style_color idx, const zdraw::rgba& col ) noexcept;
    void pop_style_color( int count = 1 ) noexcept;

} // namespace zui

#endif // !ZUI_HPP