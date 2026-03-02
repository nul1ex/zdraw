#include <global.hpp>

namespace render {

	bool initialize( )
	{
		if ( !window::initialize( ) )
		{
			std::printf("failed to initialize window\n");
			return false;
		}

		if ( !directx::initialize( ) )
		{
			std::printf("failed to initialize directx\n");
			return false;
		}

		if ( !zdraw::initialize( directx::device, directx::device_context ) )
		{
			std::printf("failed to initialize zdraw\n");
			return false;
		}

		if ( !zui::initialize( window::hwnd ) )
		{
			std::printf("failed to initialize zui\n");
			return false;
		}

		menu::initialize( directx::device, directx::device_context );

		return true;
	}

	void loop( )
	{
		constexpr float clear_color[ 4 ]{ 0.0f, 0.0f, 0.0f, 0.0f };
		MSG msg{};

		while ( msg.message != WM_QUIT )
		{
			while ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
			{
				if ( msg.message == WM_QUIT )
				{
					break;
				}

				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}

			if ( GetAsyncKeyState( VK_END ) & 1 )
			{
				DestroyWindow( window::hwnd );
				break;
			}

			{
				menu::update( );
			}

			directx::device_context->OMSetRenderTargets( 1, &directx::render_target_view, nullptr );
			directx::device_context->ClearRenderTargetView( directx::render_target_view, clear_color );

			zdraw::begin_frame( );
			{
				menu::draw( );
			}
			zdraw::end_frame( );

			directx::swap_chain->Present( 1, 0 );
		}
	}

	bool window::initialize( )
	{
		const WNDCLASSEXW wc
		{
			.cbSize = sizeof( WNDCLASSEXW ),
			.style = CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc = procedure,
			.hInstance = GetModuleHandleW( nullptr ),
			.hCursor = LoadCursorW( nullptr, IDC_ARROW ),
			.lpszClassName = L"zdraw"
		};

		if ( !RegisterClassExW( &wc ) )
		{
			return false;
		}

		hwnd = CreateWindowExW( 0, L"zdraw", L"zdraw demo", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, wc.hInstance, nullptr );
		if ( !hwnd )
		{
			return false;
		}

		ShowWindow( hwnd, SW_SHOWDEFAULT );
		UpdateWindow( hwnd );

		return true;
	}

	long long __stdcall window::procedure( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
	{
		zui::process_wndproc_message( msg, wparam, lparam );

		// @note: could move these to process_wndproc_message

		if (msg == WM_SIZE && wparam != SIZE_MINIMIZED)
			directx::resize(LOWORD(lparam), HIWORD(lparam));

		if (msg == WM_DESTROY)
			PostQuitMessage(0);

		return DefWindowProcW( hwnd, msg, wparam, lparam );
	}

	void directx::resize(UINT width, UINT height)
	{
		if (!swap_chain || width == 0 || height == 0)
			return;

		device_context->OMSetRenderTargets(0, nullptr, nullptr);

		if (render_target_view) {
			render_target_view->Release();
			render_target_view = nullptr;
		}

		swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

		{
			ID3D11Texture2D* back_buffer{};
			swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
			device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
			back_buffer->Release();
		}

		D3D11_VIEWPORT viewport
		{
			.TopLeftX = 0.0f,
			.TopLeftY = 0.0f,
			.Width = static_cast<float>(width),
			.Height = static_cast<float>(height),
			.MinDepth = 0.0f,
			.MaxDepth = 1.0f
		};

		device_context->RSSetViewports(1, &viewport);
	}

	bool directx::initialize( )
	{
		DXGI_SWAP_CHAIN_DESC sc_desc
		{
			.BufferDesc = {.Format = DXGI_FORMAT_R8G8B8A8_UNORM },
			.SampleDesc = {.Count = 1 },
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = 2,
			.OutputWindow = window::hwnd,
			.Windowed = TRUE,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.Flags = 0 
		};

		constexpr D3D_FEATURE_LEVEL feature_levels[ ]{ D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

		D3D_FEATURE_LEVEL selected{};

		if ( FAILED( D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, feature_levels, _countof( feature_levels ), D3D11_SDK_VERSION, &sc_desc, &swap_chain, &device, &selected, &device_context ) ) )
		{
			return false;
		}

		ID3D11Texture2D* back_buffer{ nullptr };
		if ( FAILED( swap_chain->GetBuffer( 0, IID_PPV_ARGS( &back_buffer ) ) ) )
		{
			return false;
		}

		const auto result{ device->CreateRenderTargetView( back_buffer, nullptr, &render_target_view ) };
		back_buffer->Release( );

		if ( FAILED( result ) )
		{
			return false;
		}

		RECT rc{};
		GetClientRect(window::hwnd, &rc);

		D3D11_VIEWPORT viewport
		{
			.TopLeftX = 0.0f,
			.TopLeftY = 0.0f,
			.Width = static_cast<float>(rc.right - rc.left),
			.Height = static_cast<float>(rc.bottom - rc.top),
			.MinDepth = 0.0f,
			.MaxDepth = 1.0f
		};

		device_context->RSSetViewports( 1, &viewport );

		return true;
	}

} // namespace render