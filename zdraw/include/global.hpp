#ifndef GLOBAL_HPP
#define GLOBAL_HPP

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// linking
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// standard
#include <windows.h>
#include <dwmapi.h>
#include <wrl/client.h>

#include <vector>
#include <array>
#include <algorithm>
#include <fstream>
#include <numbers>

// directx
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <directxmath.h>

// core
#include <include/zdraw/zdraw.hpp>
#include <include/zdraw/zui/zui.hpp>
#include <include/zdraw/zscene/zscene.hpp>

#include <include/menu/menu.hpp>
#include <include/render/render.hpp>

#endif // !GLOBAL_HPP
