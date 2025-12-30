#ifndef INPUT_HPP
#define INPUT_HPP

#include "types.hpp"

namespace zui {

	struct input_state
	{
		float mouse_x{ 0.0f };
		float mouse_y{ 0.0f };
		bool mouse_down{ false };
		bool mouse_clicked{ false };
		bool mouse_released{ false };

		bool right_mouse_down{ false };
		bool right_mouse_clicked{ false };
		bool right_mouse_released{ false };

		float scroll_delta{ 0.0f };

		[[nodiscard]] bool in_rect( const rect& r ) const noexcept
		{
			return r.contains( this->mouse_x, this->mouse_y );
		}
	};

	class input_manager
	{
	public:
		void set_hwnd( HWND hwnd ) noexcept
		{
			this->m_hwnd = hwnd;
		}

		void update( ) noexcept
		{
			this->m_prev = this->m_current;

			POINT pt{};
			GetCursorPos( &pt );

			if ( this->m_hwnd )
			{
				ScreenToClient( this->m_hwnd, &pt );
			}

			const auto down = ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) != 0;
			const auto right_down = ( GetAsyncKeyState( VK_RBUTTON ) & 0x8000 ) != 0;

			this->m_current.mouse_x = static_cast< float >( pt.x );
			this->m_current.mouse_y = static_cast< float >( pt.y );
			this->m_current.mouse_clicked = down && !this->m_prev.mouse_down;
			this->m_current.mouse_released = !down && this->m_prev.mouse_down;
			this->m_current.mouse_down = down;

			this->m_current.right_mouse_clicked = right_down && !this->m_prev.right_mouse_down;
			this->m_current.right_mouse_released = !right_down && this->m_prev.right_mouse_down;
			this->m_current.right_mouse_down = right_down;

			this->m_current.scroll_delta = this->m_pending_scroll_delta;
			this->m_pending_scroll_delta = 0.0f;
		}

		void add_scroll_delta( float delta ) noexcept
		{
			this->m_pending_scroll_delta += delta;
		}

		[[nodiscard]] const input_state& current( ) const noexcept { return this->m_current; }
		[[nodiscard]] const input_state& previous( ) const noexcept { return this->m_prev; }

		[[nodiscard]] float mouse_x( ) const noexcept { return this->m_current.mouse_x; }
		[[nodiscard]] float mouse_y( ) const noexcept { return this->m_current.mouse_y; }
		[[nodiscard]] bool mouse_down( ) const noexcept { return this->m_current.mouse_down; }
		[[nodiscard]] bool mouse_clicked( ) const noexcept { return this->m_current.mouse_clicked; }
		[[nodiscard]] bool mouse_released( ) const noexcept { return this->m_current.mouse_released; }

		[[nodiscard]] float mouse_delta_x( ) const noexcept { return this->m_current.mouse_x - this->m_prev.mouse_x; }
		[[nodiscard]] float mouse_delta_y( ) const noexcept { return this->m_current.mouse_y - this->m_prev.mouse_y; }

		[[nodiscard]] bool right_mouse_down( ) const noexcept { return this->m_current.right_mouse_down; }
		[[nodiscard]] bool right_mouse_clicked( ) const noexcept { return this->m_current.right_mouse_clicked; }
		[[nodiscard]] bool right_mouse_released( ) const noexcept { return this->m_current.right_mouse_released; }

		[[nodiscard]] float scroll_delta( ) const noexcept { return this->m_current.scroll_delta; }

		[[nodiscard]] bool hovered( const rect& r ) const noexcept
		{
			return this->m_current.in_rect( r );
		}

	private:
		HWND m_hwnd{ nullptr };
		input_state m_current{};
		input_state m_prev{};
		float m_pending_scroll_delta{ 0.0f };
	};

} // namespace zui

#endif // !INPUT_HPP