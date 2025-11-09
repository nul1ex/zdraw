# zdraw & zui

a lightweight, modern C++ dx11 rendering library with an immediate mode gui system.

## features

### zdraw (rendering)
- 2d primitives (lines, rectangles, circles, polygons, etc)
- text rendering with custom font support (via stb_truetype)
- texture loading and rendering
- efficient batched rendering with dynamic buffers
- clip rect support

### zui (gui)
- immediate mode gui system built on zdraw
- widgets: checkbox, button, slider, keybind, combo box (more coming)
- window management with drag/resize support
- nested windows and group boxes
- styling system with color presets
- animations and hover effects

## example usage
```cpp
zdraw::initialize( device, context );
zui::initialize( hwnd );

zdraw::begin_frame( );
zui::begin( );

static float x = 100, y = 100, w = 400, h = 500;
if ( zui::begin_window( "window", x, y, w, h, true ) )
{
    static bool bool_value = false;
    zui::checkbox( "checkbox", bool_value );
    
    static int int_value  = 50;
    zui::slider_int( "int slider", int_value, 1, 100);
    
    if ( zui::button( "button", 200, 30 ) ) { }
    
    zui::end_window( );
}

zui::end( );
zdraw::end_frame( );
```

## dependencies
- directx11
- stb_truetype
- stb_image
##
