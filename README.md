 # zdraw

  directx11 rendering library for windows with 2d graphics, immediate mode gui, and 3d model visualization.

  ![demo](https://i.imgur.com/Ws6CDgs.png)

  ## components

  ### zdraw (2d rendering)
  - 2d primitives (lines, rectangles, circles, polygons, arcs, triangles)
  - text rendering with custom font support (via freetype)
  - texture loading and rendering
  - efficient batched rendering with dynamic buffers
  - clip rect support
  - multi-color gradients and styled text

  ### zui (immediate mode gui)
  - widgets: button, checkbox, slider, keybind, combo box, multi-select combo, color picker, text input
  - window management with drag/resize/autosize support
  - nested windows and group boxes
  - styling system with color presets
  - animations and hover effects

  ### zscene (3d scene viewer)
  - gltf/glb model loading
  - skeletal animation playback with controls
  - perspective camera with auto-positioning
  - auto-fit and auto-orient models
  - auto-rotate turntable mode
  - render to texture for embedding in gui
  - skeleton bone screen projection (needs improvement)

  ## dependencies
  - directx11
  - freetype
  - stb (image loading)
  - cgltf (model loading)
