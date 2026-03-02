#include <global.hpp>

// @todo: add debug prints for window mode
#if WINDOW_MODE
int __stdcall wWinMain( HINSTANCE instance_handle, HINSTANCE, PWSTR, int )
{
	if ( !render::initialize( ) )
	{
		return -1;
	}

	render::loop( );
	return 0;
}
#else
int main( )
{
	if (!render::initialize( ))
	{
		std::printf("failed to initialize renderer\n");
		return -1;
	}

	render::loop( );
	return 0;
}

#endif