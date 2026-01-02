#include <include/global.hpp>

namespace menu {

	void initialize( ID3D11Device* device, ID3D11DeviceContext* context )
	{
		demo_scene.initialize( device, context, 400, 300 );
		demo_scene.load_model( "demo.glb", true, true );
		demo_scene.set_orientation( zscene::orientation::none );

		//demo_scene.enable_auto_rotate( true );
		auto world = demo_scene.get_world_transform( );
		world = world * DirectX::XMMatrixTranslation( 0.0f, -1.0f, 0.0f );
		demo_scene.set_world_transform( world );

		demo_scene.set_clear_color( 0.0f, 0.0f, 0.0f, 0.0f );
		demo_scene.play( );
	}

	void update( )
	{
		demo_scene.update( zdraw::get_delta_time( ) );
		demo_scene.render( );
	}

	void draw( )
	{
		zui::begin( );

		{
			static auto background = zdraw::load_texture_from_memory( { std::span( reinterpret_cast< const std::byte* >( resources::background ), sizeof( resources::background ) ) } );
			if ( background )
			{
				zdraw::rect_textured( 0, 0, 1280, 720, background.Get( ) );
			}
		}

		{
			static auto win_x = 50.0f, win_y = 50.0f;
			static auto win_w = 500.0f, win_h = 500.0f;

			if ( zui::begin_window( "model", win_x, win_y, win_w, win_h, true, 300.0f, 300.0f, true ) )
			{
				auto& style = zui::get_style( );
				auto [avail_w, avail_h] = zui::get_content_region_avail( );

				constexpr auto button_h{ 25.0f };
				auto viewport_h = avail_h - button_h - style.item_spacing_y;

				if ( zui::begin_nested_window( "##viewport", avail_w, viewport_h ) )
				{
					auto* win = zui::ctx( ).current_window( );
					auto& bounds = win->bounds;

					zdraw::rect_textured( bounds.x, bounds.y, bounds.w, bounds.h, demo_scene.get_texture( ) );
					demo_scene.resize_viewport( static_cast< int >( bounds.w ), static_cast< int >( bounds.h ) );

					zui::end_nested_window( );
				}

				auto button_w = zui::calc_item_width( 2 );

				if ( zui::button( "play", button_w, button_h ) )
				{
					demo_scene.play( );
				}

				zui::same_line( );

				if ( zui::button( "pause", button_w, button_h ) )
				{
					demo_scene.pause( );
				}

				zui::end_window( );
			}
		}

		{
			static auto win2_x = 570.0f, win2_y = 50.0f;
			static auto win2_w = 420.0f, win2_h = 538.0f;

			if ( zui::begin_window( "widgets", win2_x, win2_y, win2_w, win2_h, true, 350.0f, 538.0f, true ) )
			{
				auto [avail_w, avail_h] = zui::get_content_region_avail( );

				if ( zui::begin_group_box( "buttons", avail_w ) )
				{
					if ( zui::button( "full width button", zui::calc_item_width( 0 ), 28.0f ) ) { }

					auto btn_w = zui::calc_item_width( 3 );
					if ( zui::button( "btn 1", btn_w, 26.0f ) ) { }

					zui::same_line( );

					if ( zui::button( "btn 2", btn_w, 26.0f ) ) { }

					zui::same_line( );

					if ( zui::button( "btn 3", btn_w, 26.0f ) ) { }

					zui::end_group_box( );
				}

				if ( zui::begin_group_box( "checkboxes & sliders", avail_w ) )
				{
					static bool check1{ true };
					static bool check2{ false };
					static float slider1{ 0.5f };
					static int slider2{ 50 };

					zui::checkbox( "enable something", check1 );
					zui::checkbox( "enable another thing", check2 );

					zui::separator( );

					zui::slider_float( "float slider", slider1, 0.0f, 1.0f );
					zui::slider_int( "int slider", slider2, 0, 100 );

					zui::end_group_box( );
				}

				if ( zui::begin_group_box( "combos & colors", avail_w ) )
				{
					static auto combo_idx{ 0 };
					static const char* combo_items[ ]{ "option 1", "option 2", "option 3", "option 4" };
					static bool multi_items[ 4 ]{ true, false, true, false };
					static auto color{ zdraw::rgba( 100, 150, 255, 255 ) };

					zui::combo( "single select", combo_idx, combo_items, 4 );
					zui::multicombo( "multi select", multi_items, combo_items, 4 );

					zui::separator( );

					zui::color_picker( "a color", color );

					zui::end_group_box( );
				}

				if ( zui::begin_group_box( "input", avail_w ) )
				{
					static auto text{ std::string( "type something..." ) };
					static auto hotkey{ VK_RBUTTON };

					zui::text_input( "##textbox", text, 64 );
					zui::keybind( "hotkey", hotkey );

					zui::end_group_box( );
				}

				zui::end_window( );
			}
		}

		zui::end( );
	}

} // namespace menu