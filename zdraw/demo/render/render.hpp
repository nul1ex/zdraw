#pragma once

#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

namespace render {

	bool initialize( );
	void loop( );

	namespace window {

		bool initialize( );
		long long __stdcall procedure( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );

		inline HWND hwnd{ nullptr };

	} // namespace window

	namespace directx {
		void resize(UINT width, UINT height);
		bool initialize( );

		inline ID3D11Device* device{ nullptr };
		inline ID3D11DeviceContext* device_context{ nullptr };
		inline ID3D11RenderTargetView* render_target_view{ nullptr };
		inline IDXGISwapChain* swap_chain{ nullptr };

	} // namespace directx

} // namespace render