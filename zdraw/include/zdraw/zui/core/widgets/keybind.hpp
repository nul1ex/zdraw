#ifndef KEYBIND_HPP
#define KEYBIND_HPP

#include "../context.hpp"
#include "../layout.hpp"
#include "../color.hpp"

#include <include/zdraw/zdraw.hpp>
#include <cstdio>
#include <cmath>

namespace zui {

	inline bool keybind( std::string_view label, int& key )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( label );
		const auto display_label = context::get_display_label( label );
		const auto& style = ctx( ).get_style( );
		auto& input = ctx( ).input( );
		auto& anims = ctx( ).anims( );

		const auto is_waiting = ( ctx( ).m_active_keybind_id == id );

		char button_text[ 64 ]{};
		if ( is_waiting )
		{
			std::snprintf( button_text, sizeof( button_text ), "..." );
		}
		else if ( key == 0 )
		{
			std::snprintf( button_text, sizeof( button_text ), "none" );
		}
		else
		{
			if ( key >= 0x41 && key <= 0x5A )
			{
				std::snprintf( button_text, sizeof( button_text ), "%c", key );
			}
			else if ( key >= 0x30 && key <= 0x39 )
			{
				std::snprintf( button_text, sizeof( button_text ), "%c", key );
			}
			else
			{
				auto key_name = "unknown";

				switch ( key )
				{
				case VK_LBUTTON: key_name = "lmb"; break;
				case VK_RBUTTON: key_name = "rmb"; break;
				case VK_MBUTTON: key_name = "mmb"; break;
				case VK_XBUTTON1: key_name = "mb4"; break;
				case VK_XBUTTON2: key_name = "mb5"; break;
				case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: key_name = "shift"; break;
				case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: key_name = "ctrl"; break;
				case VK_MENU: case VK_LMENU: case VK_RMENU: key_name = "alt"; break;
				case VK_SPACE: key_name = "space"; break;
				case VK_RETURN: key_name = "enter"; break;
				case VK_ESCAPE: key_name = "esc"; break;
				case VK_TAB: key_name = "tab"; break;
				case VK_CAPITAL: key_name = "caps"; break;
				case VK_INSERT: key_name = "insert"; break;
				case VK_DELETE: key_name = "delete"; break;
				case VK_HOME: key_name = "home"; break;
				case VK_END: key_name = "end"; break;
				case VK_PRIOR: key_name = "pgup"; break;
				case VK_NEXT: key_name = "pgdn"; break;
				case VK_LEFT: key_name = "left"; break;
				case VK_RIGHT: key_name = "right"; break;
				case VK_UP: key_name = "up"; break;
				case VK_DOWN: key_name = "down"; break;
				default: break;
				}

				std::snprintf( button_text, sizeof( button_text ), "%s", key_name );
			}
		}

		auto [label_w, label_h] = zdraw::measure_text( display_label );
		auto [button_text_w, button_text_h] = zdraw::measure_text( button_text );

		const auto button_width = style.keybind_width;
		const auto button_height = style.keybind_height;

		auto [avail_w, avail_h] = get_content_region_avail( );
		const auto total_h = std::max( button_height, label_h );

		auto local = item_add( avail_w, total_h );
		auto abs = to_absolute( local );

		const auto button_rect = rect{ abs.x + avail_w - button_width, abs.y, button_width, button_height };
		const auto can_interact = !ctx( ).overlay_blocking_input( );
		const auto hovered = can_interact && input.hovered( button_rect );
		const auto pressed = hovered && input.mouse_clicked( );

		if ( pressed )
		{
			ctx( ).m_active_keybind_id = id;
		}

		const auto dt = zdraw::get_delta_time( );
		const auto hover_anim = anims.animate( id, hovered ? 1.0f : 0.0f, 12.0f );
		const auto wait_anim = anims.animate( id + 1, is_waiting ? 1.0f : 0.0f, 12.0f );
		auto& pulse_anim = anims.get( id + 2 );

		if ( is_waiting )
		{
			pulse_anim += dt * 3.0f;
			if ( pulse_anim > 6.28318f )
			{
				pulse_anim -= 6.28318f;
			}
		}

		auto border_col = style.keybind_border;
		if ( hover_anim > 0.01f )
		{
			border_col = lighten( border_col, 1.0f + hover_anim * 0.3f );
		}

		zdraw::rect_filled( button_rect.x, button_rect.y, button_rect.w, button_rect.h, style.keybind_bg );

		if ( wait_anim > 0.01f )
		{
			constexpr auto pad = 2.0f;
			const auto fill_x = button_rect.x + pad;
			const auto fill_y = button_rect.y + pad;
			const auto fill_w = button_rect.w - pad * 2.0f;
			const auto fill_h = button_rect.h - pad * 2.0f;

			const auto pulse_intensity = ( std::sin( pulse_anim ) * 0.5f + 0.5f ) * 0.4f + 0.6f;
			const auto shift = std::sin( pulse_anim * 0.5f ) * 0.5f + 0.5f;

			auto col_left = lighten( style.keybind_waiting, 1.0f + pulse_intensity * 0.3f );
			auto col_right = darken( style.keybind_waiting, 0.7f + pulse_intensity * 0.2f );

			col_left.a = static_cast< std::uint8_t >( col_left.a * wait_anim * 0.4f );
			col_right.a = static_cast< std::uint8_t >( col_right.a * wait_anim * 0.4f );

			if ( shift > 0.5f )
			{
				zdraw::rect_filled_multi_color( fill_x, fill_y, fill_w, fill_h, col_right, col_left, col_left, col_right );
			}
			else
			{
				zdraw::rect_filled_multi_color( fill_x, fill_y, fill_w, fill_h, col_left, col_right, col_right, col_left );
			}

			const auto glow_alpha = static_cast< std::uint8_t >( 60 * pulse_intensity * wait_anim );
			const auto glow_col = zdraw::rgba{ style.keybind_waiting.r, style.keybind_waiting.g, style.keybind_waiting.b, glow_alpha };
			const auto glow_expand = 1.5f * pulse_intensity;

			zdraw::rect( button_rect.x - glow_expand, button_rect.y - glow_expand, button_rect.w + glow_expand * 2.0f, button_rect.h + glow_expand * 2.0f, glow_col );
		}

		zdraw::rect( button_rect.x, button_rect.y, button_rect.w, button_rect.h, border_col );

		const auto text_x = button_rect.x + ( button_rect.w - button_text_w ) * 0.5f;
		const auto text_y = button_rect.y + ( button_rect.h - button_text_h ) * 0.5f;
		zdraw::text( text_x, text_y, button_text, style.text );

		if ( !display_label.empty( ) )
		{
			const auto label_x = abs.x;
			const auto label_y = abs.y + ( button_height - label_h ) * 0.5f;

			zdraw::text( label_x, label_y, display_label, style.text );
		}

		if ( is_waiting )
		{
			if ( input.mouse_clicked( ) && !hovered )
			{
				key = VK_LBUTTON;
				ctx( ).m_active_keybind_id = invalid_id;
				return true;
			}

			static const int modifier_keys[ ] = { VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU };
			for ( const auto mod_key : modifier_keys )
			{
				if ( GetAsyncKeyState( mod_key ) & 0x8000 )
				{
					key = mod_key;
					ctx( ).m_active_keybind_id = invalid_id;
					return true;
				}
			}

			for ( int vk = 0x01; vk < 0xFF; ++vk )
			{
				if ( vk == VK_LBUTTON )
				{
					continue;
				}

				if ( vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_LMENU || vk == VK_RMENU )
				{
					continue;
				}

				if ( GetAsyncKeyState( vk ) & 0x8000 )
				{
					if ( vk == VK_ESCAPE )
					{
						key = 0;
					}
					else
					{
						key = vk;
					}

					ctx( ).m_active_keybind_id = invalid_id;
					return true;
				}
			}
		}

		return false;
	}

} // namespace zui

#endif // !KEYBIND_HPP