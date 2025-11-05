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

namespace nbench
{
	struct params
	{
		int m_rect_count{ 4000 };
		int m_circle_count{ 2000 };
		int m_line_count{ 3000 };
		int m_text_count{ 2000 };

		int m_textured_draws{ 6000 };
		int m_texture_count{ 16 };
		int m_texture_size{ 4 };

		bool m_enable_texture_swaps{ true };
		bool m_enable_geometry{ true };
		bool m_enable_text{ true };
		bool m_enable_anim{ true };

		unsigned m_rng_seed{ 1337u };
	};

	namespace detail
	{
		using Microsoft::WRL::ComPtr;

		struct state
		{
			std::vector<ComPtr<ID3D11ShaderResourceView>> m_textures{};
			params m_cfg{};
			std::minstd_rand m_rng{ 1337u };

			double m_time{ 0.0 };
			std::chrono::high_resolution_clock::time_point m_last_time{ std::chrono::high_resolution_clock::now( ) };

			float m_fps_timer{ 0.0f };
			int m_frame_count{ 0 };
			float m_current_fps{ 60.0f };

			bool m_inited{ false };
		};

		static state g_state{};

		static zdraw::rgba rand_color_u8( std::minstd_rand& rng, std::uint8_t a = 255u )
		{
			auto r{ static_cast< std::uint8_t >( ( rng( ) % 206 ) + 50 ) };
			auto g{ static_cast< std::uint8_t >( ( rng( ) % 206 ) + 50 ) };
			auto b{ static_cast< std::uint8_t >( ( rng( ) % 206 ) + 50 ) };
			return zdraw::rgba( r, g, b, a );
		}

		static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> make_solid_texture( int size, zdraw::rgba color )
		{
			const auto side{ std::max( 1, size ) };
			std::vector<std::byte> pixels( static_cast< std::size_t >( side * side * 4 ) );

			for ( int i{ 0 }; i < side * side; ++i )
			{
				pixels[ static_cast< std::size_t >( i * 4 + 0 ) ] = static_cast< std::byte >( color.r );
				pixels[ static_cast< std::size_t >( i * 4 + 1 ) ] = static_cast< std::byte >( color.g );
				pixels[ static_cast< std::size_t >( i * 4 + 2 ) ] = static_cast< std::byte >( color.b );
				pixels[ static_cast< std::size_t >( i * 4 + 3 ) ] = static_cast< std::byte >( color.a );
			}

			auto srv{ zdraw::load_texture_from_memory( std::span<const std::byte>( pixels.data( ), pixels.size( ) ) ) };
			return srv;
		}

		static void ensure_textures( )
		{
			auto& s{ g_state };

			if ( static_cast< int >( s.m_textures.size( ) ) == s.m_cfg.m_texture_count )
			{
				return;
			}

			s.m_textures.clear( );
			s.m_textures.reserve( static_cast< std::size_t >( s.m_cfg.m_texture_count ) );

			for ( int i{ 0 }; i < s.m_cfg.m_texture_count; ++i )
			{
				const auto col{ rand_color_u8( s.m_rng, 255u ) };
				auto tex{ make_solid_texture( s.m_cfg.m_texture_size, col ) };
				if ( tex )
				{
					s.m_textures.push_back( tex );
				}
			}

			if ( s.m_textures.size( ) < 2 )
			{
				const auto colA{ zdraw::rgba( 255, 255, 255, 255 ) };
				const auto colB{ zdraw::rgba( 200, 200, 200, 255 ) };
				auto a{ make_solid_texture( s.m_cfg.m_texture_size, colA ) };
				auto b{ make_solid_texture( s.m_cfg.m_texture_size, colB ) };
				if ( a ) { s.m_textures.push_back( a ); }
				if ( b ) { s.m_textures.push_back( b ); }
			}
		}

		static void update_timers( )
		{
			auto& s{ g_state };

			const auto now{ std::chrono::high_resolution_clock::now( ) };
			const auto dt{ std::chrono::duration<float>( now - s.m_last_time ).count( ) };
			s.m_last_time = now;

			s.m_time += static_cast< double >( dt );

			s.m_fps_timer += dt;
			s.m_frame_count += 1;

			if ( s.m_fps_timer >= 1.0f )
			{
				s.m_current_fps = static_cast< float >( s.m_frame_count ) / s.m_fps_timer;
				s.m_fps_timer = 0.0f;
				s.m_frame_count = 0;
			}
		}

		static void background( float w, float h )
		{
			const auto sky_top{ zdraw::rgba( 25, 25, 60, 255 ) };
			const auto horizon{ zdraw::rgba( 90, 55, 130, 255 ) };
			zdraw::rect_filled_multi_color( 0.0f, 0.0f, w, h, sky_top, sky_top, horizon, horizon );
		}

		static void stress_geometry( float w, float h )
		{
			auto& s{ g_state };
			if ( !s.m_cfg.m_enable_geometry )
			{
				return;
			}

			for ( int i{ 0 }; i < s.m_cfg.m_rect_count; ++i )
			{
				const auto t{ static_cast< float >( s.m_time ) };
				const auto fx{ static_cast< float >( i ) * 0.00073f };
				const auto fy{ static_cast< float >( i ) * 0.00039f };
				const auto cx{ std::fmod( ( std::sin( t * 0.9f + fx * 19.0f ) * 0.5f + 0.5f ) * ( w - 30.0f ), w - 30.0f ) };
				const auto cy{ std::fmod( ( std::cos( t * 1.1f + fy * 23.0f ) * 0.5f + 0.5f ) * ( h - 30.0f ), h - 30.0f ) };
				const auto rw{ 10.0f + std::fmod( i * 0.13f, 20.0f ) };
				const auto rh{ 10.0f + std::fmod( i * 0.17f, 20.0f ) };
				const auto col{ zdraw::rgba( static_cast< std::uint8_t >( 80 + ( i * 13 ) % 175 ),static_cast< std::uint8_t >( 80 + ( i * 29 ) % 175 ),static_cast< std::uint8_t >( 80 + ( i * 47 ) % 175 ),200 ) };

				zdraw::rect_filled( cx, cy, rw, rh, col );
			}

			for ( int i{ 0 }; i < s.m_cfg.m_line_count; ++i )
			{
				const auto t{ static_cast< float >( s.m_time ) };
				const auto a{ i * 0.6180339887f };
				const auto r{ 20.0f + std::fmod( i * 0.7f, ( std::min( w, h ) * 0.5f ) ) };
				const auto cx{ w * 0.5f };
				const auto cy{ h * 0.5f };

				const auto x0{ cx + std::cos( a + t * 0.7f ) * r };
				const auto y0{ cy + std::sin( a + t * 0.7f ) * r * 0.7f };
				const auto x1{ cx + std::cos( a + t * 1.1f + 0.5f ) * ( r + 10.0f ) };
				const auto y1{ cy + std::sin( a + t * 1.1f + 0.5f ) * ( r + 10.0f ) * 0.7f };

				const auto col{ zdraw::rgba( 220, 220, 255, 90 ) };
				const auto th{ 1.0f + static_cast< float >( ( i % 3 ) ) * 0.3f };

				zdraw::line( x0, y0, x1, y1, col, th );
			}

			for ( int i{ 0 }; i < s.m_cfg.m_circle_count; ++i )
			{
				const auto t{ static_cast< float >( s.m_time ) };
				const auto a{ ( i * 13 ) * 0.0219f + t * 0.35f };
				const auto r{ 10.0f + ( i % 64 ) * 2.1f };
				const auto cx{ w * 0.5f + std::cos( a ) * ( w * 0.33f ) };
				const auto cy{ h * 0.5f + std::sin( a ) * ( h * 0.22f ) };
				const auto rad{ 2.0f + static_cast< float >( ( i % 7 ) ) };
				const auto col{ zdraw::rgba( static_cast< std::uint8_t >( 120 + ( i * 5 ) % 120 ),static_cast< std::uint8_t >( 120 + ( i * 7 ) % 120 ),static_cast< std::uint8_t >( 120 + ( i * 9 ) % 120 ),140 ) };

				zdraw::circle_filled( cx, cy, rad, col, 16 );
			}
		}

		static void stress_draw_calls( float w, float h )
		{
			auto& s{ g_state };
			if ( !s.m_cfg.m_enable_texture_swaps )
			{
				return;
			}

			ensure_textures( );

			const auto count{ std::max( 0, s.m_cfg.m_textured_draws ) };
			const auto tex_n{ static_cast< int >( s.m_textures.size( ) ) };
			if ( tex_n < 2 )
			{
				return;
			}

			const auto t{ static_cast< float >( s.m_time ) };
			const auto cx{ w * 0.5f };
			const auto cy{ h * 0.5f };

			for ( int i{ 0 }; i < count; ++i )
			{
				const auto idx{ ( i & 1 ) ? 1 : 0 };
				auto* tex{ s.m_textures[ static_cast< std::size_t >( idx ) ].Get( ) };

				const auto ang{ ( i * 0.0174533f ) + t * 0.9f };
				const auto rad{ 40.0f + static_cast< float >( i % 400 ) * 0.85f };
				const auto x{ cx + std::cos( ang ) * rad };
				const auto y{ cy + std::sin( ang * 0.77f ) * rad * 0.6f };
				const auto sz{ 6.0f + static_cast< float >( ( i % 5 ) ) * 0.5f };

				zdraw::rect_textured( x, y, sz, sz, tex, 0.0f, 0.0f, 1.0f, 1.0f );
			}
		}

		static void stress_text( float w, float h )
		{
			auto& s{ g_state };
			if ( !s.m_cfg.m_enable_text )
			{
				return;
			}

			const char* lines[ ]{
				"zdrawbench",
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
				"abcdefghijklmnopqrstuvwxyz",
				"0123456789 ~!@#$%^&*()_+[]{}",
			};

			const auto n_lines{ static_cast< int >( sizeof( lines ) / sizeof( lines[ 0 ] ) ) };
			const auto t{ static_cast< float >( s.m_time ) };

			for ( int i{ 0 }; i < s.m_cfg.m_text_count; ++i )
			{
				const auto ln{ i % n_lines };
				const auto x{ 10.0f + std::fmod( i * 13.0f + t * 60.0f, std::max( 10.0f, w - 220.0f ) ) };
				const auto y{ 20.0f + std::fmod( i * 7.0f + t * 24.0f, std::max( 10.0f, h - 40.0f ) ) };

				const auto r{ static_cast< std::uint8_t >( 100 + ( i * 17 ) % 155 ) };
				const auto g{ static_cast< std::uint8_t >( 140 + ( i * 11 ) % 115 ) };
				const auto b{ static_cast< std::uint8_t >( 160 + ( i * 5 ) % 95 ) };
				const auto a{ static_cast< std::uint8_t >( 230 ) };

				zdraw::text( x, y, lines[ ln ], zdraw::rgba( r, g, b, a ) );
			}
		}

		static void draw_fps_overlay( float fps )
		{
			char fps_text[ 64 ];
			std::snprintf( fps_text, sizeof( fps_text ), "fps: %.1f", fps );

			zdraw::rect_filled( 15.0f, 15.0f, 110.0f, 26.0f, zdraw::rgba( 0, 0, 0, 140 ) );
			zdraw::text( 20.0f, 20.0f, fps_text, zdraw::rgba( 100, 255, 180, 255 ) );
		}

	} // namespace detail

	static void init( const params& p )
	{
		auto& s{ detail::g_state };
		s.m_cfg = p;
		s.m_rng.seed( p.m_rng_seed );
		detail::ensure_textures( );
		s.m_inited = true;
	}

	static void frame( float w, float h )
	{
		auto& s{ detail::g_state };
		if ( !s.m_inited )
		{
			init( params{ } );
		}

		detail::update_timers( );

		detail::background( w, h );
		detail::stress_geometry( w, h );
		detail::stress_draw_calls( w, h );
		detail::stress_text( w, h );
		detail::draw_fps_overlay( s.m_current_fps );
	}

} // namespace nbench

namespace nmenu {

	static float g_main_x{ 100.0f };
	static float g_main_y{ 100.0f };

	static void draw( )
	{
		zui::begin( );

		static int tab{ 0 };

		if ( zui::begin_window( "main", g_main_x, g_main_y, 550.0f, 500.0f ) )
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

						static int value1 = 50;
						zui::slider_int( "value", value1, 0, 100 );

						zui::end_group_box( );
					}

					if ( zui::begin_group_box( "group 2", nested_w, group_height ) )
					{
						static bool test2 = false;
						zui::checkbox( "option 2", test2 );

						static float value2 = 0.5f;
						zui::slider_float( "float", value2, 0.0f, 1.0f );

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
						zui::checkbox( "option 3", test3 );

						static int key = 0;
						zui::keybind( "keybind", key );

						zui::end_group_box( );
					}

					if ( zui::begin_group_box( "group 4", nested_w, group_height ) )
					{
						if ( zui::button( "button 1", nested_w - zui::get_style( ).window_padding_x * 2.0f, 24.0f ) )
						{
						}

						constexpr const char* items[ ] = { "femboys", "oiled femboys", "something else" };
						static int current = 0;
						zui::combo( "mode", current, items, 3 );

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

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE, PWSTR, int )
{
	g_app.m_width = 1600;
	g_app.m_height = 1200;

	g_app.m_hwnd = create_window( hInstance, g_app.m_width, g_app.m_height );
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

	nbench::params p{};
	p.m_rect_count = 0;
	p.m_circle_count = 0;
	p.m_line_count = 1000;
	p.m_text_count = 1000;
	p.m_textured_draws = 0;
	p.m_texture_count = 0;
	p.m_enable_texture_swaps = false;
	p.m_enable_geometry = true;
	p.m_enable_text = true;

	nbench::init( p );

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
			//nbench::frame( static_cast< float >( g_app.m_width ), static_cast< float >( g_app.m_height ) );
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