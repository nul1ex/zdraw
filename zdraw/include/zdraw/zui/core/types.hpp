#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace zui {

	struct rect
	{
		float x{ 0.0f };
		float y{ 0.0f };
		float w{ 0.0f };
		float h{ 0.0f };

		[[nodiscard]] constexpr bool contains( float px, float py ) const noexcept
		{
			return px >= this->x && px <= this->x + this->w && py >= this->y && py <= this->y + this->h;
		}

		[[nodiscard]] constexpr rect offset( float dx, float dy ) const noexcept
		{
			return { this->x + dx, this->y + dy, this->w, this->h };
		}

		[[nodiscard]] constexpr rect expand( float amount ) const noexcept
		{
			return { this->x - amount, this->y - amount, this->w + amount * 2.0f, this->h + amount * 2.0f };
		}

		[[nodiscard]] constexpr float right( ) const noexcept { return this->x + this->w; }
		[[nodiscard]] constexpr float bottom( ) const noexcept { return this->y + this->h; }
		[[nodiscard]] constexpr float center_x( ) const noexcept { return this->x + this->w * 0.5f; }
		[[nodiscard]] constexpr float center_y( ) const noexcept { return this->y + this->h * 0.5f; }
	};

	using widget_id = std::uintptr_t;
	constexpr widget_id invalid_id = 0;

} // namespace zui

#endif // !TYPES_HPP