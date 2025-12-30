#ifndef MULTICOMBO_HPP
#define MULTICOMBO_HPP

#include "../context.hpp"
#include "../layout.hpp"
#include "../color.hpp"
#include "../overlay.hpp"

#include <include/zdraw/zdraw.hpp>
#include <string>
#include <vector>

namespace zui {

	class multicombo_overlay : public overlay
	{
	public:
		multicombo_overlay( widget_id id, const rect& anchor, float width, const char* const* items, int items_count, bool* selected_items, std::function<void( )> on_change = nullptr ) : overlay{ id, anchor }, m_width{ width }, m_items_count{ items_count }, m_selected_items{ selected_items }, m_on_change{ std::move( on_change ) }
		{
			this->m_items.reserve( items_count );
			for ( int i = 0; i < items_count; ++i )
			{
				this->m_items.emplace_back( items[ i ] );
			}

			this->m_item_anims.resize( items_count, 0.0f );
			this->m_check_anims.resize( items_count, 0.0f );

			if ( selected_items )
			{
				for ( int i = 0; i < items_count; ++i )
				{
					this->m_check_anims[ i ] = selected_items[ i ] ? 1.0f : 0.0f;
				}
			}
		}

		[[nodiscard]] bool hit_test( float x, float y ) const override
		{
			return this->get_dropdown_rect( ).contains( x, y ) || this->m_anchor.contains( x, y );
		}

		bool process_input( const input_state& input ) override
		{
			if ( this->m_closing )
			{
				return false;
			}

			const auto dropdown = this->get_dropdown_rect( );

			if ( input.mouse_clicked && !dropdown.contains( input.mouse_x, input.mouse_y ) && !this->m_anchor.contains( input.mouse_x, input.mouse_y ) )
			{
				this->m_closing = true;
				return true;
			}

			if ( input.mouse_clicked && dropdown.contains( input.mouse_x, input.mouse_y ) )
			{
				const auto padding_top = 6.0f;
				const auto item_height = 20.0f;

				for ( int i = 0; i < this->m_items_count; ++i )
				{
					const auto item_y = dropdown.y + padding_top + i * item_height;
					const auto item_rect = rect{ dropdown.x + 6.0f, item_y, dropdown.w - 12.0f, item_height };

					if ( item_rect.contains( input.mouse_x, input.mouse_y ) )
					{
						if ( this->m_selected_items )
						{
							this->m_selected_items[ i ] = !this->m_selected_items[ i ];

							if ( this->m_on_change )
							{
								this->m_on_change( );
							}

							this->m_changed = true;
						}
						return true;
					}
				}
			}

			return dropdown.contains( input.mouse_x, input.mouse_y );
		}

		void render( const style& style, const input_state& input ) override
		{
			const auto dt = zdraw::get_delta_time( );
			const auto dropdown = this->get_dropdown_rect( );

			const auto anim_speed = this->m_closing ? 16.0f : 14.0f;
			const auto target = this->m_closing ? 0.0f : 1.0f;
			this->m_open_anim = this->m_open_anim + ( target - this->m_open_anim ) * std::min( anim_speed * dt, 1.0f );

			if ( this->m_open_anim < 0.01f && this->m_closing )
			{
				this->m_fully_closed = true;
				return;
			}

			const auto ease_t = ease::out_cubic( this->m_open_anim );
			const auto animated_h = dropdown.h * ease_t;
			const auto alpha_mult = ease_t;

			auto bg_col = style.combo_popup_bg;
			bg_col.a = static_cast< std::uint8_t >( bg_col.a * alpha_mult );

			auto border_col = lighten( style.combo_popup_border, 1.1f );
			border_col.a = static_cast< std::uint8_t >( border_col.a * alpha_mult );

			zdraw::rect_filled( dropdown.x, dropdown.y, dropdown.w, animated_h, bg_col );
			zdraw::rect( dropdown.x, dropdown.y, dropdown.w, animated_h, border_col );

			zdraw::push_clip_rect( dropdown.x - 2.0f, dropdown.y, dropdown.x + dropdown.w + 2.0f, dropdown.y + animated_h );

			const auto padding_top = 6.0f;
			const auto item_height = style.combo_item_height;
			const auto item_padding = 6.0f;

			for ( int i = 0; i < this->m_items_count; ++i )
			{
				const auto item_y = dropdown.y + padding_top + i * item_height;
				const auto item_rect = rect{ dropdown.x + item_padding, item_y, dropdown.w - item_padding * 2.0f, item_height };

				const auto item_delay = i * 0.08f;
				const auto item_progress = std::clamp( this->m_open_anim - item_delay, 0.0f, 1.0f ) / ( 1.0f - std::min( item_delay, 0.5f ) );

				auto& item_anim = this->m_item_anims[ i ];
				item_anim = std::min( item_anim + 18.0f * dt, item_progress );

				const auto item_ease = ease::out_cubic( item_anim );
				const auto item_alpha = item_ease * alpha_mult;
				const auto slide_offset = ( 1.0f - item_ease ) * 8.0f;

				const auto is_selected = this->m_selected_items && this->m_selected_items[ i ];
				const auto is_hovered = item_rect.contains( input.mouse_x, input.mouse_y );

				auto& check_anim = this->m_check_anims[ i ];
				const auto check_target = is_selected ? 1.0f : 0.0f;
				check_anim += ( check_target - check_anim ) * std::min( 12.0f * dt, 1.0f );
				const auto check_ease = ease::smoothstep( check_anim );

				if ( is_hovered && !this->m_closing )
				{
					auto hover_col = style.combo_item_hovered;
					hover_col.a = static_cast< std::uint8_t >( std::min( hover_col.a * 2.5f, 255.0f ) * item_alpha );
					zdraw::rect_filled( item_rect.x, item_rect.y + slide_offset, item_rect.w, item_rect.h, hover_col );
				}

				constexpr auto check_size = 12.0f;
				const auto check_x = item_rect.x + 4.0f;
				const auto check_y = item_rect.y + ( item_height - check_size ) * 0.5f + slide_offset;

				auto check_bg = style.checkbox_bg;
				check_bg.a = static_cast< std::uint8_t >( check_bg.a * item_alpha );
				zdraw::rect_filled( check_x, check_y, check_size, check_size, check_bg );

				auto check_border = style.checkbox_border;
				if ( check_ease > 0.01f )
				{
					check_border = lerp( check_border, style.checkbox_check, check_ease * 0.5f );
				}

				check_border.a = static_cast< std::uint8_t >( check_border.a * item_alpha );
				zdraw::rect( check_x, check_y, check_size, check_size, check_border );

				if ( check_ease > 0.01f )
				{
					constexpr auto pad = 2.0f;
					const auto inner_size = check_size - pad * 2.0f;
					const auto scale = 0.6f + check_ease * 0.4f;
					const auto scaled_size = inner_size * scale;
					const auto fill_x = check_x + pad + ( inner_size - scaled_size ) * 0.5f;
					const auto fill_y = check_y + pad + ( inner_size - scaled_size ) * 0.5f;

					auto fill_col = style.checkbox_check;
					fill_col.a = static_cast< std::uint8_t >( fill_col.a * check_ease * item_alpha );
					zdraw::rect_filled( fill_x, fill_y, scaled_size, scaled_size, fill_col );
				}

				auto [text_w, text_h] = zdraw::measure_text( this->m_items[ i ] );
				const auto text_x = item_rect.x + check_size + 10.0f;
				const auto text_y = item_rect.y + ( item_height - text_h ) * 0.5f + slide_offset;

				auto text_col = style.text;
				if ( check_ease > 0.5f )
				{
					text_col = lerp( text_col, style.checkbox_check, ( check_ease - 0.5f ) * 0.6f );
				}
				if ( is_hovered && !this->m_closing )
				{
					text_col = lighten( text_col, 1.2f );
				}

				text_col.a = static_cast< std::uint8_t >( text_col.a * item_alpha );

				zdraw::text( text_x, text_y, this->m_items[ i ].c_str( ), text_col );
			}

			zdraw::pop_clip_rect( );
		}

		[[nodiscard]] bool should_close( ) const override { return this->m_closing; }
		[[nodiscard]] bool is_closed( ) const override { return this->m_fully_closed; }
		[[nodiscard]] bool was_changed( ) const { return this->m_changed; }

	private:
		[[nodiscard]] rect get_dropdown_rect( ) const
		{
			const auto padding_top = 6.0f;
			const auto padding_bottom = 6.0f;
			const auto item_height = 20.0f;
			const auto dropdown_h = static_cast< float >( this->m_items_count ) * item_height + padding_top + padding_bottom;

			return rect
			{
				this->m_anchor.x,
				this->m_anchor.bottom( ) + 4.0f,
				this->m_width,
				dropdown_h
			};
		}

		float m_width{ 0.0f };
		std::vector<std::string> m_items{};
		int m_items_count{ 0 };
		bool* m_selected_items{ nullptr };
		std::function<void( )> m_on_change{};

		float m_open_anim{ 0.0f };
		std::vector<float> m_item_anims{};
		std::vector<float> m_check_anims{};
		bool m_closing{ false };
		bool m_fully_closed{ false };
		bool m_changed{ false };
	};

	struct multicombo_scroll_state
	{
		float scroll_offset{ 0.0f };
		float hover_time{ 0.0f };
		bool was_hovered{ false };
	};

	inline std::unordered_map<widget_id, multicombo_scroll_state>& get_multicombo_states( )
	{
		static std::unordered_map<widget_id, multicombo_scroll_state> states{};
		return states;
	}

	inline bool multicombo( std::string_view label, bool* selected_items, const char* const items[ ], int items_count, float width = 0.0f )
	{
		auto* win = ctx( ).current_window( );
		if ( !win || items_count == 0 || !selected_items )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( label );
		const auto display_label = context::get_display_label( label );
		const auto& style = ctx( ).get_style( );
		auto& input = ctx( ).input( );
		auto& anims = ctx( ).anims( );
		auto& overlays = ctx( ).overlays( );

		const auto is_open = overlays.is_open( id );

		auto changed = false;
		if ( auto* popup = dynamic_cast< multicombo_overlay* >( overlays.find( id ) ) )
		{
			changed = popup->was_changed( );
		}

		auto [label_w, label_h] = zdraw::measure_text( display_label );

		if ( width <= 0.0f )
		{
			width = calc_item_width( 0 );
		}

		const auto combo_height = style.combo_height;
		const auto spacing = style.item_spacing_y * 0.25f;
		const auto total_height = label_h + spacing + combo_height;

		auto local = item_add( width, total_height );
		auto abs = to_absolute( local );

		const auto win_clip_top = win->bounds.y + win->visible_start_y;
		const auto win_clip_bottom = win->bounds.bottom( );
		const auto win_clip_left = win->bounds.x;
		const auto win_clip_right = win->bounds.right( );

		if ( !display_label.empty( ) )
		{
			zdraw::text( abs.x, abs.y, display_label, style.text );
		}

		const auto button_y = abs.y + label_h + spacing;
		const auto button_rect = rect{ abs.x, button_y, width, combo_height };
		const auto hovered = input.hovered( button_rect );

		if ( hovered && input.mouse_clicked( ) && !ctx( ).overlay_blocking_input( ) )
		{
			if ( is_open )
			{
				overlays.close( id );
			}
			else
			{
				overlays.add<multicombo_overlay>( id, button_rect, width, items, items_count, selected_items );
			}
		}

		const auto hover_anim = anims.animate( id, ( hovered || is_open ) ? 1.0f : 0.0f, 15.0f );

		const auto bg_col = lerp( style.combo_bg, style.combo_hovered, hover_anim );
		const auto border_col = is_open ? lighten( style.combo_border, 1.3f ) : ( hovered ? lighten( style.combo_border, 1.15f ) : style.combo_border );

		zdraw::rect_filled( button_rect.x, button_rect.y, button_rect.w, button_rect.h, bg_col );
		zdraw::rect( button_rect.x, button_rect.y, button_rect.w, button_rect.h, border_col );

		std::string display_text{};
		for ( int i = 0; i < items_count; ++i )
		{
			if ( selected_items[ i ] )
			{
				if ( !display_text.empty( ) )
				{
					display_text += ", ";
				}
				display_text += items[ i ];
			}
		}

		if ( display_text.empty( ) )
		{
			display_text = "none";
		}

		const auto arrow_size = 6.0f;
		const auto arrow_padding = style.frame_padding_x + 4.0f + arrow_size + 8.0f;
		const auto max_text_width = width - style.frame_padding_x - arrow_padding;

		auto [full_text_w, text_h] = zdraw::measure_text( display_text );
		const auto text_x = button_rect.x + style.frame_padding_x;
		const auto text_y = button_rect.y + ( combo_height - text_h ) * 0.5f;

		auto& scroll_states = get_multicombo_states( );
		auto& scroll_state = scroll_states[ id ];

		const auto dt = zdraw::get_delta_time( );
		const auto needs_scroll = full_text_w > max_text_width;

		if ( hovered && needs_scroll && !is_open )
		{
			if ( !scroll_state.was_hovered )
			{
				scroll_state.was_hovered = true;
				scroll_state.hover_time = 0.0f;
				scroll_state.scroll_offset = 0.0f;
			}

			scroll_state.hover_time += dt;

			constexpr auto scroll_delay = 0.5f;
			constexpr auto scroll_speed = 40.0f;
			constexpr auto pause_at_end = 1.0f;

			if ( scroll_state.hover_time > scroll_delay )
			{
				const auto scroll_time = scroll_state.hover_time - scroll_delay;
				const auto gap = 30.0f;
				const auto total_scroll_width = full_text_w + gap;
				const auto scroll_duration = total_scroll_width / scroll_speed;
				const auto cycle_duration = scroll_duration + pause_at_end;

				const auto cycle_time = std::fmod( scroll_time, cycle_duration );

				if ( cycle_time < scroll_duration )
				{
					scroll_state.scroll_offset = cycle_time * scroll_speed;
				}
				else
				{
					scroll_state.scroll_offset = 0.0f;
				}
			}
		}
		else
		{
			scroll_state.was_hovered = false;
			scroll_state.hover_time = 0.0f;
			scroll_state.scroll_offset = std::max( 0.0f, scroll_state.scroll_offset - dt * 100.0f );
		}

		const auto text_clip_right = button_rect.x + width - arrow_padding + 4.0f;

		const auto clip_left = std::max( text_x, win_clip_left );
		const auto clip_top = std::max( button_rect.y, win_clip_top );
		const auto clip_right = std::min( text_clip_right, win_clip_right );
		const auto clip_bottom = std::min( button_rect.bottom( ), win_clip_bottom );

		zdraw::push_clip_rect( clip_left, clip_top, clip_right, clip_bottom );

		if ( needs_scroll && scroll_state.scroll_offset > 0.01f )
		{
			const auto gap = 30.0f;
			const auto total_width = full_text_w + gap;

			const auto offset1 = -scroll_state.scroll_offset;
			zdraw::text( text_x + offset1, text_y, display_text, style.text );

			const auto offset2 = offset1 + total_width;
			if ( offset2 < max_text_width )
			{
				zdraw::text( text_x + offset2, text_y, display_text, style.text );
			}
		}
		else if ( needs_scroll )
		{
			auto [ellipsis_w, ellipsis_h] = zdraw::measure_text( "..." );

			std::string truncated{};
			float truncated_w = 0.0f;
			const auto available_for_text = max_text_width - ellipsis_w;

			for ( char c : display_text )
			{
				std::string test = truncated + c;
				auto [test_w, test_h] = zdraw::measure_text( test );
				if ( test_w > available_for_text )
				{
					break;
				}
				truncated = test;
				truncated_w = test_w;
			}

			truncated += "...";
			zdraw::text( text_x, text_y, truncated, style.text );
		}
		else
		{
			zdraw::text( text_x, text_y, display_text, style.text );
		}

		zdraw::pop_clip_rect( );

		const auto arrow_x = button_rect.right( ) - arrow_size - style.frame_padding_x - 4.0f;
		const auto arrow_y = button_rect.y + ( combo_height - arrow_size * 0.5f ) * 0.5f;

		auto arrow_col = lerp( style.combo_arrow, lighten( style.combo_arrow, 1.3f ), hover_anim );

		if ( is_open )
		{
			zdraw::line( arrow_x, arrow_y + 3.0f, arrow_x + arrow_size * 0.5f, arrow_y, arrow_col, 1.5f );
			zdraw::line( arrow_x + arrow_size * 0.5f, arrow_y, arrow_x + arrow_size, arrow_y + 3.0f, arrow_col, 1.5f );
		}
		else
		{
			zdraw::line( arrow_x, arrow_y, arrow_x + arrow_size * 0.5f, arrow_y + 3.0f, arrow_col, 1.5f );
			zdraw::line( arrow_x + arrow_size * 0.5f, arrow_y + 3.0f, arrow_x + arrow_size, arrow_y, arrow_col, 1.5f );
		}

		return changed;
	}

} // namespace zui

#endif // !MULTICOMBO_HPP
