#include <global.hpp>

int __stdcall wWinMain( HINSTANCE instance_handle, HINSTANCE, PWSTR, int )
{
	if ( !render::initialize( ) )
	{
		return -1;
	}

	render::loop( );
	return 0;
}