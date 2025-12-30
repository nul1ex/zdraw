#ifndef RENDER_HPP
#define RENDER_HPP

namespace render {

	bool initialize( );
	void loop( );

	namespace window {

		bool initialize( );
		long long __stdcall procedure( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );

		inline HWND hwnd{ nullptr };

	} // namespace window

	namespace directx {

		bool initialize( );

		inline ID3D11Device* device{ nullptr };
		inline ID3D11DeviceContext* device_context{ nullptr };
		inline ID3D11RenderTargetView* render_target_view{ nullptr };
		inline IDXGISwapChain* swap_chain{ nullptr };

	} // namespace directx

} // namespace render

#endif // !RENDER_HPP