#pragma once

#include "resources/background.hpp"

namespace menu {

	inline zscene::scene demo_scene{};

	void initialize( ID3D11Device* device, ID3D11DeviceContext* context );
	void update( );
	void draw( );

} // namespace menu
