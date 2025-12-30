#ifndef ANIMATION_HPP
#define ANIMATION_HPP

#include "types.hpp"
#include <include/zdraw/zdraw.hpp>

#include <unordered_map>
#include <cmath>

namespace zui {

	class animation_manager
	{
	public:
		[[nodiscard]] float& get( widget_id id, float initial = 0.0f )
		{
			auto it = this->m_states.find( id );
			if ( it == this->m_states.end( ) )
			{
				it = this->m_states.emplace( id, initial ).first;
			}

			return it->second;
		}

		[[nodiscard]] float get_value( widget_id id, float initial = 0.0f ) const
		{
			auto it = this->m_states.find( id );
			return ( it != this->m_states.end( ) ) ? it->second : initial;
		}

		void set( widget_id id, float value )
		{
			this->m_states[ id ] = value;
		}

		void remove( widget_id id )
		{
			this->m_states.erase( id );
		}

		void clear( )
		{
			this->m_states.clear( );
		}

		float animate( widget_id id, float target, float speed, float initial = 0.0f )
		{
			auto& value = this->get( id, initial );
			const auto dt = zdraw::get_delta_time( );
			value = value + ( target - value ) * std::min( speed * dt, 1.0f );
			return value;
		}

		float animate_smooth( widget_id id, float target, float speed, float initial = 0.0f )
		{
			auto raw = this->animate( id, target, speed, initial );
			return raw * raw * ( 3.0f - 2.0f * raw );
		}

	private:
		std::unordered_map<widget_id, float> m_states{};
	};

	namespace ease {

		[[nodiscard]] inline float linear( float t ) noexcept
		{
			return t;
		}

		[[nodiscard]] inline float in_quad( float t ) noexcept
		{
			return t * t;
		}

		[[nodiscard]] inline float out_quad( float t ) noexcept
		{
			return t * ( 2.0f - t );
		}

		[[nodiscard]] inline float in_out_quad( float t ) noexcept
		{
			return t < 0.5f ? 2.0f * t * t : -1.0f + ( 4.0f - 2.0f * t ) * t;
		}

		[[nodiscard]] inline float in_cubic( float t ) noexcept
		{
			return t * t * t;
		}

		[[nodiscard]] inline float out_cubic( float t ) noexcept
		{
			const auto f = t - 1.0f;
			return f * f * f + 1.0f;
		}

		[[nodiscard]] inline float in_out_cubic( float t ) noexcept
		{
			return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow( -2.0f * t + 2.0f, 3.0f ) / 2.0f;
		}

		[[nodiscard]] inline float smoothstep( float t ) noexcept
		{
			return t * t * ( 3.0f - 2.0f * t );
		}

	} // namespace ease

} // namespace zui

#endif // !ANIMATION_HPP