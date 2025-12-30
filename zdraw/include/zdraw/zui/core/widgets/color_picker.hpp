#ifndef COLOR_PICKER_HPP
#define COLOR_PICKER_HPP

#include "../context.hpp"
#include "../layout.hpp"
#include "../color.hpp"
#include "../overlay.hpp"

#include <include/zdraw/zdraw.hpp>
#include <optional>

namespace zui {

	inline std::optional<zdraw::rgba>& get_color_clipboard( )
	{
		static std::optional<zdraw::rgba> clipboard{};
		return clipboard;
	}

	class color_picker_context_menu : public overlay
	{
	public:
		color_picker_context_menu( widget_id id, const rect& anchor, zdraw::rgba* color_ptr ) : overlay{ id, anchor }, m_color_ptr{ color_ptr } { }

		[[nodiscard]] bool hit_test( float x, float y ) const override
		{
			return this->get_popup_rect( ).contains( x, y );
		}

		bool process_input( const input_state& input ) override
		{
			if ( this->m_closing )
			{
				return false;
			}

			const auto popup = this->get_popup_rect( );

			if ( input.mouse_clicked && !popup.contains( input.mouse_x, input.mouse_y ) )
			{
				this->m_closing = true;
				return true;
			}

			if ( input.mouse_clicked && popup.contains( input.mouse_x, input.mouse_y ) )
			{
				const auto button_h = 22.0f;
				const auto padding = 4.0f;

				const auto copy_rect = rect{ popup.x + padding, popup.y + padding, popup.w - padding * 2.0f, button_h };
				const auto paste_rect = rect{ popup.x + padding, copy_rect.bottom( ) + 2.0f, popup.w - padding * 2.0f, button_h };

				if ( copy_rect.contains( input.mouse_x, input.mouse_y ) )
				{
					if ( this->m_color_ptr )
					{
						get_color_clipboard( ) = *this->m_color_ptr;
					}

					this->m_closing = true;
					return true;
				}

				if ( paste_rect.contains( input.mouse_x, input.mouse_y ) )
				{
					auto& clipboard = get_color_clipboard( );
					if ( clipboard.has_value( ) && this->m_color_ptr )
					{
						*this->m_color_ptr = clipboard.value( );
						this->m_changed = true;
					}

					this->m_closing = true;
					return true;
				}
			}

			return popup.contains( input.mouse_x, input.mouse_y );
		}

		void render( const style& style, const input_state& input ) override
		{
			const auto dt = zdraw::get_delta_time( );
			const auto popup = this->get_popup_rect( );

			const auto anim_speed = this->m_closing ? 18.0f : 16.0f;
			const auto target = this->m_closing ? 0.0f : 1.0f;
			this->m_open_anim = this->m_open_anim + ( target - this->m_open_anim ) * std::min( anim_speed * dt, 1.0f );

			if ( this->m_open_anim < 0.01f && this->m_closing )
			{
				this->m_fully_closed = true;
				return;
			}

			const auto ease_t = ease::out_cubic( this->m_open_anim );
			const auto alpha_mult = ease_t;
			const auto scale = 0.9f + ease_t * 0.1f;

			const auto scaled_w = popup.w * scale;
			const auto scaled_h = popup.h * scale;
			const auto scaled_x = popup.x + ( popup.w - scaled_w ) * 0.5f;
			const auto scaled_y = popup.y + ( popup.h - scaled_h ) * 0.5f;

			auto bg_col = style.combo_popup_bg;
			bg_col.a = static_cast< std::uint8_t >( bg_col.a * alpha_mult );

			auto border_col = lighten( style.combo_popup_border, 1.1f );
			border_col.a = static_cast< std::uint8_t >( border_col.a * alpha_mult );

			zdraw::rect_filled( scaled_x, scaled_y, scaled_w, scaled_h, bg_col );
			zdraw::rect( scaled_x, scaled_y, scaled_w, scaled_h, border_col );

			if ( ease_t < 0.3f )
			{
				return;
			}

			const auto content_alpha = std::clamp( ( ease_t - 0.3f ) / 0.7f, 0.0f, 1.0f );

			const auto button_h = 22.0f;
			const auto padding = 4.0f;

			const auto copy_rect = rect{ popup.x + padding, popup.y + padding, popup.w - padding * 2.0f, button_h };
			const auto paste_rect = rect{ popup.x + padding, copy_rect.bottom( ) + 2.0f, popup.w - padding * 2.0f, button_h };

			const auto copy_hovered = copy_rect.contains( input.mouse_x, input.mouse_y );
			const auto paste_hovered = paste_rect.contains( input.mouse_x, input.mouse_y );
			const auto has_clipboard = get_color_clipboard( ).has_value( );

			{
				auto btn_bg = copy_hovered ? style.combo_item_hovered : zdraw::rgba{ 0, 0, 0, 0 };
				btn_bg.a = static_cast< std::uint8_t >( std::min( btn_bg.a * 2.0f, 255.0f ) * content_alpha );

				if ( copy_hovered )
				{
					zdraw::rect_filled( copy_rect.x, copy_rect.y, copy_rect.w, copy_rect.h, btn_bg );
				}

				auto text_col = style.text;
				if ( copy_hovered )
				{
					text_col = lighten( text_col, 1.2f );
				}

				text_col.a = static_cast< std::uint8_t >( text_col.a * content_alpha );

				auto [text_w, text_h] = zdraw::measure_text( "copy" );
				const auto text_x = copy_rect.x + ( copy_rect.w - text_w ) * 0.5f;
				const auto text_y = copy_rect.y + ( copy_rect.h - text_h ) * 0.5f;
				zdraw::text( text_x, text_y, "copy", text_col );
			}

			{
				auto btn_bg = paste_hovered && has_clipboard ? style.combo_item_hovered : zdraw::rgba{ 0, 0, 0, 0 };
				btn_bg.a = static_cast< std::uint8_t >( std::min( btn_bg.a * 2.0f, 255.0f ) * content_alpha );

				if ( paste_hovered && has_clipboard )
				{
					zdraw::rect_filled( paste_rect.x, paste_rect.y, paste_rect.w, paste_rect.h, btn_bg );
				}

				auto text_col = style.text;
				if ( !has_clipboard )
				{
					text_col.a = static_cast< std::uint8_t >( text_col.a * 0.4f );
				}
				else if ( paste_hovered )
				{
					text_col = lighten( text_col, 1.2f );
				}

				text_col.a = static_cast< std::uint8_t >( text_col.a * content_alpha );

				auto [text_w, text_h] = zdraw::measure_text( "paste" );
				const auto text_x = paste_rect.x + ( paste_rect.w - text_w ) * 0.5f;
				const auto text_y = paste_rect.y + ( paste_rect.h - text_h ) * 0.5f;
				zdraw::text( text_x, text_y, "paste", text_col );
			}
		}

		[[nodiscard]] bool should_close( ) const override { return this->m_closing; }
		[[nodiscard]] bool is_closed( ) const override { return this->m_fully_closed; }
		[[nodiscard]] bool was_changed( ) const { return this->m_changed; }

	private:
		[[nodiscard]] rect get_popup_rect( ) const
		{
			constexpr auto padding = 4.0f;
			constexpr auto button_h = 22.0f;
			constexpr auto button_gap = 2.0f;

			constexpr auto popup_w = 70.0f;
			constexpr auto popup_h = padding + button_h + button_gap + button_h + padding;

			return rect
			{
				this->m_anchor.x,
				this->m_anchor.y,
				popup_w,
				popup_h
			};
		}

		zdraw::rgba* m_color_ptr{ nullptr };
		float m_open_anim{ 0.0f };
		bool m_closing{ false };
		bool m_fully_closed{ false };
		bool m_changed{ false };
	};

	class color_picker_overlay : public overlay
	{
	public:
		color_picker_overlay( widget_id id, const rect& anchor, zdraw::rgba* color_ptr ) : overlay{ id, anchor }, m_color_ptr{ color_ptr }
		{
			if ( color_ptr )
			{
				const auto hsv_color = rgb_to_hsv( *color_ptr );
				this->m_hue = hsv_color.h / 360.0f;
				this->m_saturation = hsv_color.s;
				this->m_value = hsv_color.v;
			}
		}

		[[nodiscard]] bool hit_test( float x, float y ) const override
		{
			return this->get_popup_rect( ).contains( x, y ) || this->m_anchor.contains( x, y );
		}

		bool process_input( const input_state& input ) override
		{
			if ( this->m_closing )
			{
				return false;
			}

			const auto popup = this->get_popup_rect( );

			if ( input.mouse_clicked && !popup.contains( input.mouse_x, input.mouse_y ) && !this->m_anchor.contains( input.mouse_x, input.mouse_y ) )
			{
				this->m_closing = true;
				return true;
			}

			const auto pad = 8.0f;
			const auto alpha_bar_w = 14.0f;
			const auto sv_size = popup.w - pad * 3.0f - alpha_bar_w;
			const auto hue_h = 14.0f;

			const auto sv_rect = rect{ popup.x + pad, popup.y + pad, sv_size, sv_size };
			const auto hue_rect = rect{ popup.x + pad, sv_rect.bottom( ) + pad, sv_size, hue_h };
			const auto alpha_rect = rect{ sv_rect.right( ) + pad, popup.y + pad, alpha_bar_w, sv_size + pad + hue_h };

			if ( input.mouse_clicked )
			{
				if ( sv_rect.contains( input.mouse_x, input.mouse_y ) )
				{
					this->m_active_component = 1;
				}
				else if ( hue_rect.contains( input.mouse_x, input.mouse_y ) )
				{
					this->m_active_component = 2;
				}
				else if ( alpha_rect.contains( input.mouse_x, input.mouse_y ) )
				{
					this->m_active_component = 3;
				}
			}

			if ( input.mouse_released )
			{
				this->m_active_component = 0;
			}

			if ( input.mouse_down && this->m_active_component != 0 )
			{
				if ( this->m_active_component == 1 )
				{
					this->m_saturation = std::clamp( ( input.mouse_x - sv_rect.x ) / sv_rect.w, 0.0f, 1.0f );
					this->m_value = 1.0f - std::clamp( ( input.mouse_y - sv_rect.y ) / sv_rect.h, 0.0f, 1.0f );
				}
				else if ( this->m_active_component == 2 )
				{
					this->m_hue = std::clamp( ( input.mouse_x - hue_rect.x ) / hue_rect.w, 0.0f, 1.0f );
				}
				else if ( this->m_active_component == 3 )
				{
					const auto alpha_norm = 1.0f - std::clamp( ( input.mouse_y - alpha_rect.y ) / alpha_rect.h, 0.0f, 1.0f );
					if ( this->m_color_ptr )
					{
						this->m_color_ptr->a = static_cast< std::uint8_t >( alpha_norm * 255.0f );
					}
				}

				if ( this->m_color_ptr && this->m_active_component != 3 )
				{
					const auto new_color = hsv_to_rgb( this->m_hue * 360.0f, this->m_saturation, this->m_value );
					this->m_color_ptr->r = new_color.r;
					this->m_color_ptr->g = new_color.g;
					this->m_color_ptr->b = new_color.b;
				}

				this->m_changed = true;
			}

			return popup.contains( input.mouse_x, input.mouse_y );
		}

		void render( const style& style, const input_state& input ) override
		{
			( void )input;
			const auto popup = this->get_popup_rect( );

			const auto bg_top = style.combo_popup_bg;
			const auto bg_bottom = darken( bg_top, 0.9f );

			zdraw::rect_filled_multi_color( popup.x, popup.y, popup.w, popup.h, bg_top, bg_top, bg_bottom, bg_bottom );
			zdraw::rect( popup.x, popup.y, popup.w, popup.h, lighten( style.combo_popup_border, 1.1f ) );

			zdraw::push_clip_rect( popup.x, popup.y, popup.right( ), popup.bottom( ) );

			const auto pad = 8.0f;
			const auto alpha_bar_w = 14.0f;
			const auto sv_size = popup.w - pad * 3.0f - alpha_bar_w;
			const auto hue_h = 14.0f;

			const auto sv_rect = rect{ popup.x + pad, popup.y + pad, sv_size, sv_size };
			const auto hue_rect = rect{ popup.x + pad, sv_rect.bottom( ) + pad, sv_size, hue_h };
			const auto alpha_rect = rect{ sv_rect.right( ) + pad, popup.y + pad, alpha_bar_w, sv_size + pad + hue_h };

			this->render_sv_square( sv_rect );
			this->render_hue_bar( hue_rect );
			this->render_alpha_bar( alpha_rect );
			this->render_cursors( sv_rect, hue_rect, alpha_rect );

			zdraw::pop_clip_rect( );
		}

		[[nodiscard]] bool should_close( ) const override { return this->m_closing; }
		[[nodiscard]] bool is_closed( ) const override { return this->m_closing; }
		[[nodiscard]] bool was_changed( ) const { return this->m_changed; }
		void clear_changed( ) { this->m_changed = false; }

	private:
		[[nodiscard]] rect get_popup_rect( ) const
		{
			return rect
			{
				this->m_anchor.x,
				this->m_anchor.bottom( ) + 2.0f,
				194.0f,
				194.0f
			};
		}

		void render_sv_square( const rect& r ) const
		{
			for ( int y = 0; y < static_cast< int >( r.h ); ++y )
			{
				for ( int x = 0; x < static_cast< int >( r.w ); ++x )
				{
					const auto s = static_cast< float >( x ) / r.w;
					const auto v = 1.0f - static_cast< float >( y ) / r.h;
					const auto color = hsv_to_rgb( this->m_hue * 360.0f, s, v );
					zdraw::rect_filled( r.x + x, r.y + y, 1.0f, 1.0f, color );
				}
			}
		}

		void render_hue_bar( const rect& r ) const
		{
			for ( int x = 0; x < static_cast< int >( r.w ); ++x )
			{
				const auto h = ( static_cast< float >( x ) / r.w ) * 360.0f;
				const auto color = hsv_to_rgb( h, 1.0f, 1.0f );
				zdraw::rect_filled( r.x + x, r.y, 1.0f, r.h, color );
			}
		}

		void render_alpha_bar( const rect& r ) const
		{
			for ( int x = 0; x < static_cast< int >( r.w ); x += 6 )
			{
				for ( int y = 0; y < static_cast< int >( r.h ); y += 6 )
				{
					const auto is_dark = ( ( x / 6 ) + ( y / 6 ) ) % 2 == 0;
					const auto checker_col = is_dark ? zdraw::rgba{ 180, 180, 180, 255 } : zdraw::rgba{ 220, 220, 220, 255 };
					zdraw::rect_filled( r.x + x, r.y + y, std::min( 6.0f, r.w - x ), std::min( 6.0f, r.h - y ), checker_col );
				}
			}

			if ( this->m_color_ptr )
			{
				for ( int y = 0; y < static_cast< int >( r.h ); ++y )
				{
					const auto a = static_cast< std::uint8_t >( ( 1.0f - static_cast< float >( y ) / r.h ) * 255.0f );
					const auto color = zdraw::rgba{ this->m_color_ptr->r, this->m_color_ptr->g, this->m_color_ptr->b, a };
					zdraw::rect_filled( r.x, r.y + y, r.w, 1.0f, color );
				}
			}
		}

		void render_cursors( const rect& sv, const rect& hue, const rect& alpha ) const
		{
			const auto sv_x = sv.x + this->m_saturation * sv.w;
			const auto sv_y = sv.y + ( 1.0f - this->m_value ) * sv.h;
			zdraw::rect( sv_x - 4.0f, sv_y - 4.0f, 8.0f, 8.0f, zdraw::rgba{ 255, 255, 255, 255 }, 2.0f );
			zdraw::rect( sv_x - 3.0f, sv_y - 3.0f, 6.0f, 6.0f, zdraw::rgba{ 0, 0, 0, 255 }, 3.0f );

			const auto hue_x = hue.x + this->m_hue * hue.w;
			zdraw::rect( hue_x - 2.0f, hue.y - 2.0f, 4.0f, hue.h + 4.0f, zdraw::rgba{ 255, 255, 255, 255 }, 2.0f );
			zdraw::rect( hue_x - 1.0f, hue.y - 1.0f, 2.0f, hue.h + 2.0f, zdraw::rgba{ 0, 0, 0, 255 }, 1.0f );

			if ( this->m_color_ptr )
			{
				const auto alpha_y = alpha.y + ( 1.0f - this->m_color_ptr->a / 255.0f ) * alpha.h;
				zdraw::rect( alpha.x - 2.0f, alpha_y - 2.0f, alpha.w + 4.0f, 4.0f, zdraw::rgba{ 255, 255, 255, 255 }, 2.0f );
				zdraw::rect( alpha.x - 1.0f, alpha_y - 1.0f, alpha.w + 2.0f, 2.0f, zdraw::rgba{ 0, 0, 0, 255 }, 1.0f );
			}
		}

		zdraw::rgba* m_color_ptr{ nullptr };
		float m_hue{ 0.0f };
		float m_saturation{ 0.0f };
		float m_value{ 1.0f };
		int m_active_component{ 0 }; // 0=none, 1=sv, 2=hue, 3=alpha
		bool m_closing{ false };
		bool m_changed{ false };
	};

	inline bool color_picker( std::string_view label, zdraw::rgba& color, float width = 0.0f )
	{
		auto* win = ctx( ).current_window( );
		if ( !win )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( label );
		const auto display_label = context::get_display_label( label );
		const auto context_menu_id = id + 1;
		const auto& style = ctx( ).get_style( );
		auto& input = ctx( ).input( );
		auto& overlays = ctx( ).overlays( );

		const auto is_open = overlays.is_open( id );
		const auto context_menu_open = overlays.is_open( context_menu_id );

		auto changed = false;

		if ( auto* popup = dynamic_cast< color_picker_overlay* >( overlays.find( id ) ) )
		{
			changed = popup->was_changed( );
			popup->clear_changed( );
		}

		if ( auto* ctx_menu = dynamic_cast< color_picker_context_menu* >( overlays.find( context_menu_id ) ) )
		{
			if ( ctx_menu->was_changed( ) )
			{
				changed = true;
			}
		}

		const auto swatch_size = style.color_picker_swatch_size;
		const auto has_label = !display_label.empty( );

		auto label_w = 0.0f;
		auto label_h = 0.0f;
		if ( has_label )
		{
			auto [w, h] = zdraw::measure_text( display_label );
			label_w = w;
			label_h = h;
		}

		const auto total_w = has_label ? ( swatch_size + style.item_spacing_x + label_w ) : swatch_size;
		const auto total_h = has_label ? std::max( swatch_size, label_h ) : swatch_size;

		auto local = item_add( total_w, total_h );
		auto abs = to_absolute( local );

		auto x_offset = 0.0f;
		if ( width > 0.0f )
		{
			x_offset = width - total_w;
		}

		const auto swatch_y = has_label ? ( abs.y + ( total_h - swatch_size ) * 0.5f ) : abs.y;
		const auto swatch_rect = rect{ abs.x + x_offset, swatch_y, swatch_size, swatch_size };
		const auto hovered = input.hovered( swatch_rect );

		if ( hovered && input.mouse_clicked( ) && !ctx( ).overlay_blocking_input( ) )
		{
			if ( is_open )
			{
				overlays.close( id );
			}
			else
			{
				overlays.add<color_picker_overlay>( id, swatch_rect, &color );
			}
		}

		if ( hovered && input.right_mouse_clicked( ) && !ctx( ).overlay_blocking_input( ) )
		{
			if ( !context_menu_open )
			{
				const auto menu_anchor = rect{ input.mouse_x( ), input.mouse_y( ), 0.0f, 0.0f };
				overlays.add<color_picker_context_menu>( context_menu_id, menu_anchor, &color );
			}
		}

		const auto border_col = is_open ? lighten( style.color_picker_border, 1.3f ) : ( hovered ? lighten( style.color_picker_border, 1.15f ) : style.color_picker_border );
		zdraw::rect_filled( swatch_rect.x, swatch_rect.y, swatch_rect.w, swatch_rect.h, style.color_picker_bg );

		constexpr auto pad = 2.0f;
		zdraw::rect_filled( swatch_rect.x + pad, swatch_rect.y + pad, swatch_rect.w - pad * 2.0f, swatch_rect.h - pad * 2.0f, color );

		zdraw::rect( swatch_rect.x, swatch_rect.y, swatch_rect.w, swatch_rect.h, border_col );

		if ( has_label )
		{
			const auto text_x = swatch_rect.right( ) + style.item_spacing_x;
			const auto text_y = abs.y + ( total_h - label_h ) * 0.5f;

			zdraw::text( text_x, text_y, display_label, style.text );
		}

		return changed;
	}

} // namespace zui

#endif // !COLOR_PICKER_HPP
