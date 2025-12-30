#ifndef COMBO_HPP
#define COMBO_HPP

#include "../context.hpp"
#include "../layout.hpp"
#include "../color.hpp"
#include "../overlay.hpp"

#include <include/zdraw/zdraw.hpp>
#include <vector>

namespace zui {

	class combo_overlay : public overlay
	{
	public:
		combo_overlay( widget_id id, const rect& anchor, float width, const std::vector<std::string>& items, int* current_item, std::function<void( )> on_change = nullptr ) : overlay{ id, anchor }, m_width{ width }, m_items{ items }, m_current_item{ current_item }, m_on_change{ std::move( on_change ) }
		{
			this->m_item_anims.resize( items.size( ), 0.0f );
			this->m_hover_anims.resize( items.size( ), 0.0f );
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

				for ( int i = 0; i < static_cast< int >( this->m_items.size( ) ); ++i )
				{
					const auto item_y = dropdown.y + padding_top + i * item_height;
					const auto item_rect = rect{ dropdown.x + 6.0f, item_y, dropdown.w - 12.0f, item_height };

					if ( item_rect.contains( input.mouse_x, input.mouse_y ) )
					{
						if ( this->m_current_item )
						{
							*this->m_current_item = i;
						}

						if ( this->m_on_change )
						{
							this->m_on_change( );
						}

						this->m_changed = true;
						this->m_closing = true;
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

			for ( int i = 0; i < static_cast< int >( this->m_items.size( ) ); ++i )
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

				const auto is_selected = this->m_current_item && ( i == *this->m_current_item );
				const auto is_hovered = item_rect.contains( input.mouse_x, input.mouse_y );

				auto& hover_anim = this->m_hover_anims[ i ];
				const auto hover_target = ( is_hovered && !this->m_closing ) ? 1.0f : 0.0f;
				hover_anim += ( hover_target - hover_anim ) * std::min( 15.0f * dt, 1.0f );

				if ( is_selected )
				{
					auto selected_col = style.combo_item_selected;
					selected_col.a = static_cast< std::uint8_t >( std::min( selected_col.a * 2.0f, 255.0f ) * item_alpha );
					zdraw::rect_filled( item_rect.x, item_rect.y + slide_offset, item_rect.w, item_rect.h, selected_col );

					auto accent_col = style.accent;
					accent_col.a = static_cast< std::uint8_t >( 255 * item_alpha );
					zdraw::rect_filled( item_rect.x + 1.0f, item_rect.y + 3.0f + slide_offset, 3.0f, item_rect.h - 6.0f, accent_col );
				}
				else if ( hover_anim > 0.01f )
				{
					auto hover_col = style.combo_item_hovered;
					hover_col.a = static_cast< std::uint8_t >( std::min( hover_col.a * 2.5f, 255.0f ) * item_alpha * hover_anim );
					zdraw::rect_filled( item_rect.x, item_rect.y + slide_offset, item_rect.w, item_rect.h, hover_col );
				}

				auto [text_w, text_h] = zdraw::measure_text( this->m_items[ i ] );
				const auto text_x = item_rect.x + ( is_selected ? 12.0f : 10.0f );
				const auto text_y = item_rect.y + ( item_height - text_h ) * 0.5f + slide_offset;

				auto text_col = style.text;
				if ( is_selected )
				{
					text_col = style.accent;
				}
				else if ( hover_anim > 0.01f )
				{
					text_col = lerp( text_col, lighten( text_col, 1.3f ), hover_anim );
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
			const auto dropdown_h = static_cast< float >( this->m_items.size( ) ) * item_height + padding_top + padding_bottom;

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
		int* m_current_item{ nullptr };
		std::function<void( )> m_on_change{};

		float m_open_anim{ 0.0f };
		std::vector<float> m_item_anims{};
		std::vector<float> m_hover_anims{};
		bool m_closing{ false };
		bool m_fully_closed{ false };
		bool m_changed{ false };
	};

	inline bool combo( std::string_view label, int& current_item, const char* const items[ ], int items_count, float width = 0.0f )
	{
		auto* win = ctx( ).current_window( );
		if ( !win || items_count == 0 )
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
		if ( auto* popup = dynamic_cast< combo_overlay* >( overlays.find( id ) ) )
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
				std::vector<std::string> item_strings{};
				item_strings.reserve( items_count );
				for ( int i = 0; i < items_count; ++i )
				{
					item_strings.emplace_back( items[ i ] );
				}

				overlays.add<combo_overlay>( id, button_rect, width, item_strings, &current_item );
			}
		}

		const auto hover_anim = anims.animate( id, ( hovered || is_open ) ? 1.0f : 0.0f, 15.0f );

		const auto bg_col = lerp( style.combo_bg, style.combo_hovered, hover_anim );
		const auto border_col = is_open ? lighten( style.combo_border, 1.3f ) : ( hovered ? lighten( style.combo_border, 1.15f ) : style.combo_border );

		zdraw::rect_filled( button_rect.x, button_rect.y, button_rect.w, button_rect.h, bg_col );
		zdraw::rect( button_rect.x, button_rect.y, button_rect.w, button_rect.h, border_col );

		const auto current_text = ( current_item >= 0 && current_item < items_count ) ? items[ current_item ] : "";
		auto [text_w, text_h] = zdraw::measure_text( current_text );
		const auto text_x = button_rect.x + style.frame_padding_x;
		const auto text_y = button_rect.y + ( combo_height - text_h ) * 0.5f;
		zdraw::text( text_x, text_y, current_text, style.text );

		const auto arrow_size = 6.0f;
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

#endif // !COMBO_HPP