#include <include/zdraw/zdraw.hpp>
#include <include/zdraw/zui.hpp>

#include <chrono>
#include <vector>
#include <random>

using Microsoft::WRL::ComPtr;

namespace
{
	struct app_state
	{
		HWND m_hwnd{ nullptr };
		ComPtr<IDXGISwapChain> m_swap_chain{};
		ComPtr<ID3D11Device> m_device{};
		ComPtr<ID3D11DeviceContext> m_context{};
		ComPtr<ID3D11RenderTargetView> m_rtv{};

		UINT m_width{ 1600 };
		UINT m_height{ 1200 };
		bool m_vsync{ false };
	} g_app;

	static void set_viewport( UINT w, UINT h )
	{
		D3D11_VIEWPORT vp{};
		vp.TopLeftX = 0.0f;
		vp.TopLeftY = 0.0f;
		vp.Width = static_cast< float >( w );
		vp.Height = static_cast< float >( h );
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		g_app.m_context->RSSetViewports( 1, &vp );
	}

	static bool create_rtv( )
	{
		ComPtr<ID3D11Texture2D> back_buffer{};
		const auto hr{ g_app.m_swap_chain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast< void** >( back_buffer.GetAddressOf( ) ) ) };
		if ( FAILED( hr ) || !back_buffer )
		{
			return false;
		}

		g_app.m_rtv.Reset( );
		const auto hr2{ g_app.m_device->CreateRenderTargetView( back_buffer.Get( ), nullptr, &g_app.m_rtv ) };
		return SUCCEEDED( hr2 );
	}

	static bool resize_swap_chain( UINT w, UINT h )
	{
		if ( !g_app.m_swap_chain )
		{
			return false;
		}

		g_app.m_context->OMSetRenderTargets( 0, nullptr, nullptr );
		g_app.m_rtv.Reset( );

		const auto hr{ g_app.m_swap_chain->ResizeBuffers( 0, w, h, DXGI_FORMAT_UNKNOWN, 0 ) };
		if ( FAILED( hr ) )
		{
			return false;
		}

		if ( !create_rtv( ) )
		{
			return false;
		}

		set_viewport( w, h );
		g_app.m_width = w;
		g_app.m_height = h;
		return true;
	}

	static LRESULT CALLBACK wnd_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
	{
		switch ( msg )
		{
		case WM_SIZE:
		{
			if ( g_app.m_swap_chain )
			{
				const auto w{ static_cast< UINT >( LOWORD( lparam ) ) };
				const auto h{ static_cast< UINT >( HIWORD( lparam ) ) };
				if ( w > 0 && h > 0 )
				{
					resize_swap_chain( w, h );
				}
			}

			return 0;
		}
		case WM_DESTROY:
		{
			PostQuitMessage( 0 );
			return 0;
		}
		default:
			return DefWindowProcW( hwnd, msg, wparam, lparam );
		}
	}

	static HWND create_window( HINSTANCE hinst, UINT client_w, UINT client_h )
	{
		WNDCLASSEXW wc{};
		wc.cbSize = sizeof( wc );
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = wnd_proc;
		wc.hInstance = hinst;
		wc.hCursor = LoadCursorW( nullptr, IDC_ARROW );
		wc.hbrBackground = reinterpret_cast< HBRUSH >( COLOR_WINDOW + 1 );
		wc.lpszClassName = L"zdrawbench_class";

		if ( !RegisterClassExW( &wc ) )
		{
			return nullptr;
		}

		RECT rc{ 0, 0, static_cast< LONG >( client_w ), static_cast< LONG >( client_h ) };
		AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, 0 );

		const auto wnd_w{ rc.right - rc.left };
		const auto wnd_h{ rc.bottom - rc.top };

		return CreateWindowExW( 0, wc.lpszClassName, L"zdrawbench", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, wnd_w, wnd_h, nullptr, nullptr, hinst, nullptr );
	}

	static bool setup_d3d11( HWND hwnd )
	{
		DXGI_SWAP_CHAIN_DESC scd{};
		scd.BufferCount = 2;
		scd.BufferDesc.Width = 0;
		scd.BufferDesc.Height = 0;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferDesc.RefreshRate.Numerator = 0;
		scd.BufferDesc.RefreshRate.Denominator = 1;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.OutputWindow = hwnd;
		scd.SampleDesc.Count = 1;
		scd.SampleDesc.Quality = 0;
		scd.Windowed = TRUE;
		scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		scd.Flags = 0;

		const D3D_FEATURE_LEVEL fl_request[ ]{ D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
		D3D_FEATURE_LEVEL fl_chosen{};

		UINT dev_flags{ 0 };
		const auto hr{ D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, dev_flags,fl_request, static_cast< UINT >( std::size( fl_request ) ), D3D11_SDK_VERSION, &scd, &g_app.m_swap_chain, &g_app.m_device, &fl_chosen, &g_app.m_context ) };

		if ( FAILED( hr ) )
		{
			return false;
		}

		if ( !create_rtv( ) )
		{
			return false;
		}

		RECT cr{};
		GetClientRect( hwnd, &cr );
		const auto w{ static_cast< UINT >( cr.right - cr.left ) };
		const auto h{ static_cast< UINT >( cr.bottom - cr.top ) };
		set_viewport( w, h );

		g_app.m_width = w;
		g_app.m_height = h;

		ComPtr<IDXGIDevice1> dxgi_dev{};
		if ( SUCCEEDED( g_app.m_device.As( &dxgi_dev ) ) )
		{
			dxgi_dev->SetMaximumFrameLatency( 1 );
		}

		return true;
	}

} // anonymous namespace

namespace nmenu {

	static float g_main_x{ 100.0f };
	static float g_main_y{ 100.0f };
	static float g_main_w{ 550.0f };
	static float g_main_h{ 500.0f };

	static void draw( )
	{
		zui::begin( );

		static int tab{ 0 };

		if ( zui::begin_window( "main", g_main_x, g_main_y, g_main_w, g_main_h ) )
		{
			const auto [main_w, main_h] = zui::get_content_region_avail( );
			const auto header_width = main_w;
			const auto header_height = main_h / 8;

			if ( zui::begin_nested_window( "header", header_width, header_height ) )
			{
				const auto [header_w, header_h] = zui::get_content_region_avail( );
				const auto button_width = zui::calc_item_width( 4 );
				const auto button_height = header_h;

				if ( zui::button( "combat", button_width, button_height ) )
				{
					tab = 0;
				}

				zui::same_line( );

				if ( zui::button( "visual", button_width, button_height ) )
				{
					tab = 1;
				}

				zui::same_line( );

				if ( zui::button( "misc", button_width, button_height ) )
				{
					tab = 2;
				}

				zui::same_line( );

				if ( zui::button( "config", button_width, button_height ) )
				{
					tab = 3;
				}

				zui::end_nested_window( );
			}

			{
				const auto [content_w, content_h] = zui::get_content_region_avail( );
				const auto column_width = zui::calc_item_width( 2 );

				if ( zui::begin_nested_window( "left", column_width, content_h ) )
				{
					const auto [nested_w, nested_h] = zui::get_content_region_avail( );
					const auto group_height = ( nested_h - zui::get_style( ).item_spacing_y ) / 2.0f;

					if ( zui::begin_group_box( "group 1", nested_w, group_height ) )
					{
						static bool test1 = false;
						zui::checkbox( "option 1", test1 );

						static bool test2 = false;
						zui::checkbox( "option 2", test2 );

						static int value1 = 50;
						zui::slider_int( "int", value1, 0, 100 );

						static int value2 = 50;
						zui::slider_int( "int 2", value2, 0, 100 );

						zui::end_group_box( );
					}

					if ( zui::begin_group_box( "group 2", nested_w, group_height ) )
					{
						static bool test2 = false;
						zui::checkbox( "option 1 g2", test2 );

						static float value2 = 0.5f;
						zui::slider_float( "float", value2, 0.0f, 1.0f );

						static float value3 = 0.5f;
						zui::slider_float( "float 2", value3, 0.0f, 1.0f );

						zui::end_group_box( );
					}

					zui::end_nested_window( );
				}

				zui::same_line( );

				if ( zui::begin_nested_window( "right", column_width, content_h ) )
				{
					const auto [nested_w, nested_h] = zui::get_content_region_avail( );
					const auto group_height = ( nested_h - zui::get_style( ).item_spacing_y ) / 2.0f;

					if ( zui::begin_group_box( "group 3", nested_w, group_height ) )
					{
						static bool test3 = false;
						zui::checkbox( "option 1 g3", test3 );

						static int key = 0;
						zui::keybind( "keybind", key );

						static int current_theme = 4;
						constexpr const char* theme_names[ ] = { "dark blue", "light pink", "mint", "dark white", "lavender", "peach", "sky" };

						if ( zui::combo( "theme", current_theme, theme_names, 7 ) )
						{
							zui::apply_color_preset( static_cast< zui::color_preset >( current_theme ) );
						}

						zui::end_group_box( );
					}

					if ( zui::begin_group_box( "group 4", nested_w, group_height ) )
					{
						if ( zui::button( "button 1", nested_w - zui::get_style( ).window_padding_x * 2.0f, 24.0f ) )
						{
						}

						constexpr const char* items[ ] = { "test", "something", "something else" };
						static int current = 0;
						zui::combo( "mode", current, items, 3 );

						static int value1 = 50;
						zui::slider_int( "int g4", value1, 0, 100 );

						zui::end_group_box( );
					}

					zui::end_nested_window( );
				}
			}

			zui::end_window( );
		}

		zui::end( );
	}

} // namespace nmenu

int WINAPI wWinMain( HINSTANCE instance_handle, HINSTANCE, PWSTR, int )
{
	g_app.m_width = 1600;
	g_app.m_height = 1200;

	g_app.m_hwnd = create_window( instance_handle, g_app.m_width, g_app.m_height );
	if ( !g_app.m_hwnd )
	{
		return -1;
	}

	if ( !setup_d3d11( g_app.m_hwnd ) )
	{
		return -1;
	}

	if ( !zdraw::initialize( g_app.m_device.Get( ), g_app.m_context.Get( ) ) )
	{
		return -1;
	}

	zui::initialize( g_app.m_hwnd );

	MSG msg{};
	while ( msg.message != WM_QUIT )
	{
		while ( PeekMessageW( &msg, nullptr, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessageW( &msg );
		}

		if ( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 )
		{
			PostQuitMessage( 0 );
		}

		if ( GetAsyncKeyState( 'V' ) & 0x1 )
		{
			g_app.m_vsync = !g_app.m_vsync;
		}

		constexpr float clear_col[ 4 ]{ 0.10f, 0.10f, 0.10f, 1.0f };
		g_app.m_context->OMSetRenderTargets( 1, g_app.m_rtv.GetAddressOf( ), nullptr );
		g_app.m_context->ClearRenderTargetView( g_app.m_rtv.Get( ), clear_col );

		zdraw::begin_frame( );
		{
			nmenu::draw( );
		}
		zdraw::end_frame( );

		g_app.m_swap_chain->Present( g_app.m_vsync ? 1 : 0, 0 );
	}

	g_app.m_rtv.Reset( );
	g_app.m_context.Reset( );
	g_app.m_device.Reset( );
	g_app.m_swap_chain.Reset( );

	return 0;
}