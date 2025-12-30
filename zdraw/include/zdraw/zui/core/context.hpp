#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "types.hpp"
#include "style.hpp"
#include "input.hpp"
#include "animation.hpp"
#include "overlay.hpp"

#include <vector>
#include <string>
#include <unordered_map>

namespace zui {

	struct window_state
	{
		std::string title{};
		rect bounds{};
		float cursor_x{ 0.0f };
		float cursor_y{ 0.0f };
		float line_height{ 0.0f };
		rect last_item{};
		bool is_child{ false };

		float scroll_y{ 0.0f };
		float content_height{ 0.0f };
		float visible_start_y{ 0.0f };
		widget_id scroll_id{ invalid_id };
		bool scrollbar_visible{ false };
		bool scrollbar_disabled{ false };
	};

	struct style_var_backup
	{
		style_var var{};
		float prev{};
	};

	struct style_color_backup
	{
		style_color idx{};
		zdraw::rgba prev{};
	};

	class context
	{
	public:
		bool initialize( HWND hwnd )
		{
			this->m_input.set_hwnd( hwnd );
			return hwnd != nullptr;
		}

		void begin_frame( )
		{
			this->m_input.update( );
			this->m_windows.clear( );
			this->m_id_stack.clear( );
		}

		void end_frame( )
		{
			if ( this->m_overlays.has_active_overlay( ) )
			{
				this->m_overlays.process_input( this->m_input.current( ) );
			}

			this->m_overlays.render( this->m_style, this->m_input.current( ) );
			this->m_overlays.cleanup( );

			if ( this->m_input.mouse_released( ) )
			{
				this->m_active_window_id = invalid_id;
				this->m_active_resize_id = invalid_id;
				this->m_active_slider_id = invalid_id;
				this->m_active_scrollbar_id = invalid_id;
			}
		}

		[[nodiscard]] input_manager& input( ) noexcept { return this->m_input; }
		[[nodiscard]] const input_manager& input( ) const noexcept { return this->m_input; }

		[[nodiscard]] style& get_style( ) noexcept { return this->m_style; }
		[[nodiscard]] const style& get_style( ) const noexcept { return this->m_style; }

		[[nodiscard]] animation_manager& anims( ) noexcept { return this->m_anims; }

		[[nodiscard]] overlay_manager& overlays( ) noexcept { return this->m_overlays; }
		[[nodiscard]] const overlay_manager& overlays( ) const noexcept { return this->m_overlays; }

		[[nodiscard]] bool overlay_blocking_input( ) const { return this->m_overlays.has_active_overlay( ); }

		void push_window( window_state&& state ) { this->m_windows.push_back( std::move( state ) ); }
		void pop_window( ) { if ( !this->m_windows.empty( ) ) { this->m_windows.pop_back( ); } }

		[[nodiscard]] window_state* current_window( )
		{
			return this->m_windows.empty( ) ? nullptr : &this->m_windows.back( );
		}

		[[nodiscard]] const window_state* current_window( ) const
		{
			return this->m_windows.empty( ) ? nullptr : &this->m_windows.back( );
		}

		void push_id( widget_id id )
		{
			this->m_id_stack.push_back( id );
		}

		void pop_id( )
		{
			if ( !this->m_id_stack.empty( ) )
			{
				this->m_id_stack.pop_back( );
			}
		}

		[[nodiscard]] static std::string_view get_display_label( std::string_view label )
		{
			const auto pos = label.find( "##" );
			if ( pos != std::string_view::npos )
			{
				return label.substr( 0, pos );
			}

			return label;
		}

		[[nodiscard]] widget_id generate_id( std::string_view label )
		{
			auto h = fnv1a64( label.data( ), label.size( ) );

			for ( const auto& parent_id : this->m_id_stack )
			{
				h ^= parent_id;
				h *= 1099511628211ull;
			}

			return h;
		}

		widget_id m_active_window_id{ invalid_id };
		widget_id m_active_resize_id{ invalid_id };
		widget_id m_active_slider_id{ invalid_id };
		widget_id m_active_keybind_id{ invalid_id };
		widget_id m_active_scrollbar_id{ invalid_id };
		widget_id m_active_text_input_id{ invalid_id };

		std::vector<style_var_backup> m_style_var_stack{};
		std::vector<style_color_backup> m_style_color_stack{};
		std::unordered_map<widget_id, float> m_scroll_states{};
		std::unordered_map<widget_id, bool> m_scrollbar_visible{};
		std::unordered_map<widget_id, float> m_group_box_heights{};

	private:
		static std::uintptr_t fnv1a64( const void* p, std::size_t n )
		{
			const auto* b = static_cast< const unsigned char* >( p );
			auto h = 14695981039346656037ull;

			while ( n-- )
			{
				h ^= *b++;
				h *= 1099511628211ull;
			}

			return h;
		}

		input_manager m_input{};
		style m_style{};
		animation_manager m_anims{};
		overlay_manager m_overlays{};

		std::vector<window_state> m_windows{};
		std::vector<widget_id> m_id_stack{};
	};

	inline context& ctx( )
	{
		static context instance{};
		return instance;
	}

} // namespace zui

#endif // !CONTEXT_HPP