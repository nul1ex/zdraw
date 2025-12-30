#ifndef COLOR_HPP
#define COLOR_HPP

#include <include/zdraw/zdraw.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace zui {

	struct hsv
	{
		float h{ 0.0f }; // 0-360
		float s{ 0.0f }; // 0-1
		float v{ 0.0f }; // 0-1
		float a{ 1.0f }; // 0-1
	};

	[[nodiscard]] inline zdraw::rgba lighten( const zdraw::rgba& color, float factor ) noexcept
	{
		return zdraw::rgba
		{
			static_cast< std::uint8_t >( std::min( color.r * factor, 255.0f ) ),
			static_cast< std::uint8_t >( std::min( color.g * factor, 255.0f ) ),
			static_cast< std::uint8_t >( std::min( color.b * factor, 255.0f ) ),
			color.a
		};
	}

	[[nodiscard]] inline zdraw::rgba darken( const zdraw::rgba& color, float factor ) noexcept
	{
		return zdraw::rgba
		{
			static_cast< std::uint8_t >( color.r * factor ),
			static_cast< std::uint8_t >( color.g * factor ),
			static_cast< std::uint8_t >( color.b * factor ),
			color.a
		};
	}

	[[nodiscard]] inline zdraw::rgba alpha( const zdraw::rgba& color, float a ) noexcept
	{
		return zdraw::rgba{ color.r, color.g, color.b, static_cast< std::uint8_t >( a * 255.0f ) };
	}

	[[nodiscard]] inline zdraw::rgba alpha( const zdraw::rgba& color, std::uint8_t a ) noexcept
	{
		return zdraw::rgba{ color.r, color.g, color.b, a };
	}

	[[nodiscard]] inline zdraw::rgba lerp( const zdraw::rgba& a, const zdraw::rgba& b, float t ) noexcept
	{
		return zdraw::rgba
		{
			static_cast< std::uint8_t >( a.r + ( b.r - a.r ) * t ),
			static_cast< std::uint8_t >( a.g + ( b.g - a.g ) * t ),
			static_cast< std::uint8_t >( a.b + ( b.b - a.b ) * t ),
			static_cast< std::uint8_t >( a.a + ( b.a - a.a ) * t )
		};
	}

	[[nodiscard]] inline hsv rgb_to_hsv( const zdraw::rgba& color ) noexcept
	{
		const auto r = color.r / 255.0f;
		const auto g = color.g / 255.0f;
		const auto b = color.b / 255.0f;

		const auto max_c = std::max( { r, g, b } );
		const auto min_c = std::min( { r, g, b } );
		const auto delta = max_c - min_c;

		hsv result{};
		result.v = max_c;
		result.s = ( max_c != 0.0f ) ? ( delta / max_c ) : 0.0f;
		result.a = color.a / 255.0f;

		if ( delta != 0.0f )
		{
			if ( max_c == r )
			{
				result.h = 60.0f * std::fmod( ( g - b ) / delta, 6.0f );
			}
			else if ( max_c == g )
			{
				result.h = 60.0f * ( 2.0f + ( b - r ) / delta );
			}
			else
			{
				result.h = 60.0f * ( 4.0f + ( r - g ) / delta );
			}
		}

		if ( result.h < 0.0f )
		{
			result.h += 360.0f;
		}

		return result;
	}

	[[nodiscard]] inline zdraw::rgba hsv_to_rgb( const hsv& color ) noexcept
	{
		const auto c = color.v * color.s;
		const auto h_prime = color.h / 60.0f;
		const auto x = c * ( 1.0f - std::abs( std::fmod( h_prime, 2.0f ) - 1.0f ) );
		const auto m = color.v - c;

		float r1, g1, b1;

		if ( h_prime < 1.0f ) { r1 = c; g1 = x; b1 = 0.0f; }
		else if ( h_prime < 2.0f ) { r1 = x; g1 = c; b1 = 0.0f; }
		else if ( h_prime < 3.0f ) { r1 = 0.0f; g1 = c; b1 = x; }
		else if ( h_prime < 4.0f ) { r1 = 0.0f; g1 = x; b1 = c; }
		else if ( h_prime < 5.0f ) { r1 = x; g1 = 0.0f; b1 = c; }
		else { r1 = c; g1 = 0.0f; b1 = x; }

		return zdraw::rgba
		{
			static_cast< std::uint8_t >( ( r1 + m ) * 255.0f ),
			static_cast< std::uint8_t >( ( g1 + m ) * 255.0f ),
			static_cast< std::uint8_t >( ( b1 + m ) * 255.0f ),
			static_cast< std::uint8_t >( color.a * 255.0f )
		};
	}

	[[nodiscard]] inline zdraw::rgba hsv_to_rgb( float h, float s, float v, float a = 1.0f ) noexcept
	{
		return hsv_to_rgb( hsv{ h, s, v, a } );
	}

} // namespace zui

#endif // !COLOR_HPP