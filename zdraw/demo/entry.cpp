#include <global.hpp>

int main( )
{
	if ( !render::initialize( ) )
	{
		std::printf( "failed to initialize renderer\n" );
		return -1;
	}

	render::loop( );
	return 0;
}