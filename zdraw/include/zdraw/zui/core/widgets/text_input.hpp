#ifndef TEXT_INPUT_HPP
#define TEXT_INPUT_HPP

#include "../context.hpp"
#include "../layout.hpp"
#include "../color.hpp"

#include <include/zdraw/zdraw.hpp>
#include <string>
#include <algorithm>
#include <cmath>

namespace zui {

	namespace detail {

		struct text_input_state
		{
			std::size_t cursor_pos{ 0 };
			std::size_t selection_start{ 0 };
			std::size_t selection_end{ 0 };
			float cursor_blink_timer{ 0.0f };
			float scroll_offset{ 0.0f };
			float cursor_anim_x{ 0.0f };
			bool cursor_anim_initialized{ false };
			std::unordered_map<int, float> key_repeat_timers{};
			std::unordered_map<int, bool> key_was_down{};
		};

		inline std::unordered_map<widget_id, text_input_state>& get_text_input_states( )
		{
			static std::unordered_map<widget_id, text_input_state> states{};
			return states;
		}

		inline bool is_printable_char( int vk )
		{
			return ( vk >= 0x20 && vk <= 0x7E );
		}

		inline char vk_to_char( int vk, bool shift_held )
		{
			if ( vk >= 'A' && vk <= 'Z' )
			{
				const auto caps_on = ( GetKeyState( VK_CAPITAL ) & 0x0001 ) != 0;
				const auto upper = shift_held != caps_on;
				return upper ? static_cast< char >( vk ) : static_cast< char >( vk + 32 );
			}

			if ( vk >= '0' && vk <= '9' )
			{
				if ( shift_held )
				{
					constexpr const char* shift_nums = ")!@#$%^&*(";
					return shift_nums[ vk - '0' ];
				}
				return static_cast< char >( vk );
			}

			if ( !shift_held )
			{
				switch ( vk )
				{
				case VK_SPACE: return ' ';
				case VK_OEM_1: return ';';
				case VK_OEM_PLUS: return '=';
				case VK_OEM_COMMA: return ',';
				case VK_OEM_MINUS: return '-';
				case VK_OEM_PERIOD: return '.';
				case VK_OEM_2: return '/';
				case VK_OEM_3: return '`';
				case VK_OEM_4: return '[';
				case VK_OEM_5: return '\\';
				case VK_OEM_6: return ']';
				case VK_OEM_7: return '\'';
				default: break;
				}
			}
			else
			{
				switch ( vk )
				{
				case VK_SPACE: return ' ';
				case VK_OEM_1: return ':';
				case VK_OEM_PLUS: return '+';
				case VK_OEM_COMMA: return '<';
				case VK_OEM_MINUS: return '_';
				case VK_OEM_PERIOD: return '>';
				case VK_OEM_2: return '?';
				case VK_OEM_3: return '~';
				case VK_OEM_4: return '{';
				case VK_OEM_5: return '|';
				case VK_OEM_6: return '}';
				case VK_OEM_7: return '"';
				default: break;
				}
			}

			return '\0';
		}

	} // namespace detail

	inline bool text_input( std::string_view label, std::string& text, std::size_t max_length = 256, std::string_view hint = "" )
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

		auto& states = detail::get_text_input_states( );
		auto& state = states[ id ];

		const auto is_active = ( ctx( ).m_active_text_input_id == id );

		auto [avail_w, avail_h] = get_content_region_avail( );

		const auto input_height = style.text_input_height;

		auto local = item_add( avail_w, input_height );
		auto abs = to_absolute( local );

		const auto frame_rect = rect{ abs.x, abs.y, avail_w, input_height };

		const auto can_interact = !ctx( ).overlay_blocking_input( );
		const auto hovered = can_interact && input.hovered( frame_rect );
		const auto clicked = hovered && input.mouse_clicked( );

		if ( clicked )
		{
			ctx( ).m_active_text_input_id = id;

			state.key_was_down.clear( );
			state.key_repeat_timers.clear( );

			const auto text_start_x = frame_rect.x + style.frame_padding_x - state.scroll_offset;
			const auto click_x = input.mouse_x( );

			std::size_t best_pos = 0;
			float best_dist = std::abs( click_x - text_start_x );

			for ( std::size_t i = 1; i <= text.size( ); ++i )
			{
				auto [tw, th] = zdraw::measure_text( std::string_view{ text.data( ), i } );
				const auto char_x = text_start_x + tw;
				const auto dist = std::abs( click_x - char_x );
				if ( dist < best_dist )
				{
					best_dist = dist;
					best_pos = i;
				}
			}

			state.cursor_pos = best_pos;
			state.selection_start = best_pos;
			state.selection_end = best_pos;
			state.cursor_blink_timer = 0.0f;
		}

		if ( is_active && input.mouse_clicked( ) && !hovered )
		{
			ctx( ).m_active_text_input_id = invalid_id;
		}

		auto changed = false;

		if ( is_active )
		{
			const auto shift_held = ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0;
			const auto ctrl_held = ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0;

			const auto dt = zdraw::get_delta_time( );

			auto process_key = [ &state, dt ]( int vk ) -> bool
				{
					const auto is_down = ( GetAsyncKeyState( vk ) & 0x8000 ) != 0;
					const auto was_down = state.key_was_down[ vk ];
					state.key_was_down[ vk ] = is_down;

					if ( !is_down )
					{
						state.key_repeat_timers[ vk ] = 0.0f;
						return false;
					}

					if ( !was_down )
					{
						state.key_repeat_timers[ vk ] = 0.0f;
						return true;
					}

					state.key_repeat_timers[ vk ] += dt;
					constexpr auto initial_delay = 0.4f;
					constexpr auto repeat_rate = 0.03f;

					if ( state.key_repeat_timers[ vk ] >= initial_delay )
					{
						const auto excess = state.key_repeat_timers[ vk ] - initial_delay;
						const auto repeats = static_cast< int >( excess / repeat_rate );
						const auto prev_repeats = static_cast< int >( ( excess - dt ) / repeat_rate );
						if ( repeats > prev_repeats )
						{
							return true;
						}
					}

					return false;
				};

			const auto has_selection = state.selection_start != state.selection_end;
			const auto sel_min = std::min( state.selection_start, state.selection_end );
			const auto sel_max = std::max( state.selection_start, state.selection_end );

			if ( process_key( VK_LEFT ) )
			{
				state.cursor_blink_timer = 0.0f;
				if ( ctrl_held )
				{
					while ( state.cursor_pos > 0 && text[ state.cursor_pos - 1 ] == ' ' )
					{
						state.cursor_pos--;
					}
					while ( state.cursor_pos > 0 && text[ state.cursor_pos - 1 ] != ' ' )
					{
						state.cursor_pos--;
					}
				}
				else if ( has_selection && !shift_held )
				{
					state.cursor_pos = sel_min;
				}
				else if ( state.cursor_pos > 0 )
				{
					state.cursor_pos--;
				}

				if ( shift_held )
				{
					state.selection_end = state.cursor_pos;
				}
				else
				{
					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
				}
			}

			if ( process_key( VK_RIGHT ) )
			{
				state.cursor_blink_timer = 0.0f;
				if ( ctrl_held )
				{
					while ( state.cursor_pos < text.size( ) && text[ state.cursor_pos ] != ' ' )
					{
						state.cursor_pos++;
					}
					while ( state.cursor_pos < text.size( ) && text[ state.cursor_pos ] == ' ' )
					{
						state.cursor_pos++;
					}
				}
				else if ( has_selection && !shift_held )
				{
					state.cursor_pos = sel_max;
				}
				else if ( state.cursor_pos < text.size( ) )
				{
					state.cursor_pos++;
				}

				if ( shift_held )
				{
					state.selection_end = state.cursor_pos;
				}
				else
				{
					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
				}
			}

			if ( process_key( VK_HOME ) )
			{
				state.cursor_blink_timer = 0.0f;
				state.cursor_pos = 0;
				if ( shift_held )
				{
					state.selection_end = state.cursor_pos;
				}
				else
				{
					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
				}
			}

			if ( process_key( VK_END ) )
			{
				state.cursor_blink_timer = 0.0f;
				state.cursor_pos = text.size( );
				if ( shift_held )
				{
					state.selection_end = state.cursor_pos;
				}
				else
				{
					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
				}
			}

			if ( ctrl_held && process_key( 'A' ) )
			{
				state.selection_start = 0;
				state.selection_end = text.size( );
				state.cursor_pos = text.size( );
			}

			if ( process_key( VK_BACK ) )
			{
				state.cursor_blink_timer = 0.0f;
				if ( has_selection )
				{
					text.erase( sel_min, sel_max - sel_min );
					state.cursor_pos = sel_min;
					state.selection_start = sel_min;
					state.selection_end = sel_min;
					changed = true;
				}
				else if ( state.cursor_pos > 0 )
				{
					if ( ctrl_held )
					{
						auto erase_start = state.cursor_pos;
						while ( erase_start > 0 && text[ erase_start - 1 ] == ' ' )
						{
							erase_start--;
						}
						while ( erase_start > 0 && text[ erase_start - 1 ] != ' ' )
						{
							erase_start--;
						}
						text.erase( erase_start, state.cursor_pos - erase_start );
						state.cursor_pos = erase_start;
					}
					else
					{
						text.erase( state.cursor_pos - 1, 1 );
						state.cursor_pos--;
					}
					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
					changed = true;
				}
			}

			if ( process_key( VK_DELETE ) )
			{
				state.cursor_blink_timer = 0.0f;
				if ( has_selection )
				{
					text.erase( sel_min, sel_max - sel_min );
					state.cursor_pos = sel_min;
					state.selection_start = sel_min;
					state.selection_end = sel_min;
					changed = true;
				}
				else if ( state.cursor_pos < text.size( ) )
				{
					if ( ctrl_held )
					{
						auto erase_end = state.cursor_pos;
						while ( erase_end < text.size( ) && text[ erase_end ] != ' ' )
						{
							erase_end++;
						}
						while ( erase_end < text.size( ) && text[ erase_end ] == ' ' )
						{
							erase_end++;
						}
						text.erase( state.cursor_pos, erase_end - state.cursor_pos );
					}
					else
					{
						text.erase( state.cursor_pos, 1 );
					}
					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
					changed = true;
				}
			}

			if ( process_key( VK_ESCAPE ) || process_key( VK_RETURN ) )
			{
				ctx( ).m_active_text_input_id = invalid_id;
			}

			for ( int vk = 0x20; vk <= 0xDF; ++vk )
			{
				if ( vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU )
				{
					continue;
				}

				if ( vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN || vk == VK_HOME || vk == VK_END || vk == VK_DELETE || vk == VK_BACK || vk == VK_ESCAPE || vk == VK_RETURN )
				{
					continue;
				}

				if ( process_key( vk ) )
				{
					const auto ch = detail::vk_to_char( vk, shift_held );
					if ( ch != '\0' && text.size( ) < max_length )
					{
						state.cursor_blink_timer = 0.0f;

						if ( has_selection )
						{
							text.erase( sel_min, sel_max - sel_min );
							state.cursor_pos = sel_min;
						}

						text.insert( state.cursor_pos, 1, ch );
						state.cursor_pos++;
						state.selection_start = state.cursor_pos;
						state.selection_end = state.cursor_pos;
						changed = true;
					}
				}
			}

			state.cursor_pos = std::min( state.cursor_pos, text.size( ) );
			state.selection_start = std::min( state.selection_start, text.size( ) );
			state.selection_end = std::min( state.selection_end, text.size( ) );
		}

		const auto dt = zdraw::get_delta_time( );
		const auto hover_anim = anims.animate( id, hovered ? 1.0f : 0.0f, 12.0f );
		const auto active_anim = anims.animate( id + 1, is_active ? 1.0f : 0.0f, 12.0f );

		if ( is_active )
		{
			state.cursor_blink_timer += dt;
			if ( state.cursor_blink_timer > 1.0f )
			{
				state.cursor_blink_timer -= 1.0f;
			}
		}

		auto border_col = style.text_input_border;
		if ( is_active )
		{
			border_col = style.accent;
		}
		else if ( hover_anim > 0.01f )
		{
			border_col = lighten( border_col, 1.0f + hover_anim * 0.3f );
		}

		zdraw::rect_filled( frame_rect.x, frame_rect.y, frame_rect.w, frame_rect.h, style.text_input_bg );
		zdraw::rect( frame_rect.x, frame_rect.y, frame_rect.w, frame_rect.h, border_col );

		const auto text_padding_x = style.frame_padding_x;
		const auto text_area_w = frame_rect.w - text_padding_x * 2.0f;

		auto [text_w, text_h] = text.empty( ) ? zdraw::measure_text( "A" ) : zdraw::measure_text( text );
		if ( text.empty( ) ) text_w = 0.0f;

		if ( is_active )
		{
			auto [cursor_offset_w, cursor_offset_h] = state.cursor_pos == 0 ? std::pair{ 0.0f, 0.0f } : zdraw::measure_text( std::string_view{ text.data( ), state.cursor_pos } );

			const auto cursor_x_in_text = cursor_offset_w;
			const auto visible_start = state.scroll_offset;
			const auto visible_end = state.scroll_offset + text_area_w;

			if ( cursor_x_in_text < visible_start )
			{
				state.scroll_offset = cursor_x_in_text;
			}
			else if ( cursor_x_in_text > visible_end - 2.0f )
			{
				state.scroll_offset = cursor_x_in_text - text_area_w + 2.0f;
			}

			state.scroll_offset = std::max( 0.0f, state.scroll_offset );
		}

		zdraw::push_clip_rect( frame_rect.x + text_padding_x, frame_rect.y, frame_rect.x + text_padding_x + text_area_w, frame_rect.y + frame_rect.h );

		const auto text_x = frame_rect.x + text_padding_x - state.scroll_offset;
		const auto text_y = frame_rect.y + ( frame_rect.h - text_h ) * 0.5f;

		if ( is_active && state.selection_start != state.selection_end )
		{
			const auto sel_min_local = std::min( state.selection_start, state.selection_end );
			const auto sel_max_local = std::max( state.selection_start, state.selection_end );

			auto [sel_start_w, sel_start_h] = sel_min_local == 0 ? std::pair{ 0.0f, 0.0f } : zdraw::measure_text( std::string_view{ text.data( ), sel_min_local } );
			auto [sel_end_w, sel_end_h] = zdraw::measure_text( std::string_view{ text.data( ), sel_max_local } );

			const auto sel_x = text_x + sel_start_w;
			const auto sel_w = sel_end_w - sel_start_w;

			auto sel_col = style.accent;
			sel_col.a = 100;
			zdraw::rect_filled( sel_x, text_y - 1.0f, sel_w, text_h + 2.0f, sel_col );
		}

		if ( text.empty( ) && !is_active && !hint.empty( ) )
		{
			auto hint_col = style.text;
			hint_col.a = 100;
			zdraw::text( frame_rect.x + text_padding_x, text_y, hint, hint_col );
		}
		else if ( !text.empty( ) )
		{
			zdraw::text( text_x, text_y, text, style.text );
		}

		if ( is_active )
		{
			const auto cursor_visible = std::fmod( state.cursor_blink_timer, 1.0f ) < 0.5f;
			auto [cursor_offset_w, cursor_offset_h] = state.cursor_pos == 0 ? std::pair{ 0.0f, 0.0f } : zdraw::measure_text( std::string_view{ text.data( ), state.cursor_pos } );

			const auto target_cursor_x = text_x + cursor_offset_w;

			if ( !state.cursor_anim_initialized )
			{
				state.cursor_anim_x = target_cursor_x;
				state.cursor_anim_initialized = true;
			}
			else
			{
				state.cursor_anim_x += ( target_cursor_x - state.cursor_anim_x ) * std::min( 20.0f * dt, 1.0f );
			}

			const auto cursor_y = text_y;
			const auto cursor_h = text_h;

			const auto blink_alpha = ( std::sin( state.cursor_blink_timer * 6.28318f ) * 0.5f + 0.5f );
			auto cursor_col = style.text;
			cursor_col.a = static_cast< std::uint8_t >( 255.0f * ( 0.4f + 0.6f * blink_alpha ) );
			zdraw::rect_filled( state.cursor_anim_x, cursor_y, 1.0f, cursor_h, cursor_col );
		}
		else
		{
			state.cursor_anim_initialized = false;
		}

		zdraw::pop_clip_rect( );

		return changed;
	}

} // namespace zui

#endif // !TEXT_INPUT_HPP
