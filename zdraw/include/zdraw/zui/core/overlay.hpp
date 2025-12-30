#ifndef OVERLAY_HPP
#define OVERLAY_HPP

#include "types.hpp"
#include "input.hpp"
#include "style.hpp"
#include "color.hpp"
#include "animation.hpp"

#include <include/zdraw/zdraw.hpp>
#include <vector>
#include <memory>
#include <functional>

namespace zui {

	class overlay_manager;

	class overlay
	{
	public:
		virtual ~overlay( ) = default;

		[[nodiscard]] widget_id id( ) const noexcept { return this->m_id; }
		[[nodiscard]] virtual bool hit_test( float x, float y ) const = 0;

		virtual bool process_input( const input_state& input ) = 0;
		virtual void render( const style& style, const input_state& input ) = 0;

		[[nodiscard]] virtual bool should_close( ) const = 0;
		[[nodiscard]] virtual bool is_closed( ) const = 0;
		[[nodiscard]] const rect& anchor_rect( ) const noexcept { return this->m_anchor; }

	protected:
		explicit overlay( widget_id id, const rect& anchor ) : m_id{ id }, m_anchor{ anchor } { }

		widget_id m_id{ invalid_id };
		rect m_anchor{};
	};

	class overlay_manager
	{
	public:
		template<typename T, typename... arg>
		T* add( arg&&... args )
		{
			auto ptr = std::make_unique<T>( std::forward<arg>( args )... );
			auto* raw = ptr.get( );
			this->m_overlays.push_back( std::move( ptr ) );
			return raw;
		}

		void close( widget_id id )
		{
			for ( auto& overlay : this->m_overlays )
			{
				if ( overlay && overlay->id( ) == id )
				{
					this->m_closing_ids.push_back( id );
					break;
				}
			}
		}

		[[nodiscard]] bool is_open( widget_id id ) const
		{
			for ( const auto& overlay : this->m_overlays )
			{
				if ( overlay && overlay->id( ) == id && !overlay->should_close( ) )
				{
					return true;
				}
			}

			return false;
		}

		[[nodiscard]] overlay* find( widget_id id )
		{
			for ( auto& overlay : this->m_overlays )
			{
				if ( overlay && overlay->id( ) == id )
				{
					return overlay.get( );
				}
			}

			return nullptr;
		}

		[[nodiscard]] bool wants_input( float x, float y ) const
		{
			for ( const auto& overlay : this->m_overlays )
			{
				if ( overlay && overlay->hit_test( x, y ) )
				{
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] bool has_active_overlay( ) const
		{
			return !this->m_overlays.empty( );
		}

		bool process_input( const input_state& input )
		{
			for ( auto it = this->m_overlays.rbegin( ); it != this->m_overlays.rend( ); ++it )
			{
				if ( *it && ( *it )->process_input( input ) )
				{
					return true;
				}
			}

			return false;
		}

		void render( const style& style, const input_state& input )
		{
			for ( auto& overlay : this->m_overlays )
			{
				if ( overlay )
				{
					overlay->render( style, input );
				}
			}
		}

		void cleanup( )
		{
			this->m_overlays.erase( std::remove_if( this->m_overlays.begin( ), this->m_overlays.end( ), [ ]( const std::unique_ptr<overlay>& o ) { return !o || o->is_closed( ); } ), this->m_overlays.end( ) );
			this->m_closing_ids.clear( );
		}

		void clear( )
		{
			this->m_overlays.clear( );
			this->m_closing_ids.clear( );
		}

	private:
		std::vector<std::unique_ptr<overlay>> m_overlays{};
		std::vector<widget_id> m_closing_ids{};
	};

} // namespace zui

#endif // !OVERLAY_HPP