#pragma once

namespace zscene {

	[[nodiscard]] bool load_gltf( const std::string& filepath, model& out_model );
	[[nodiscard]] bool load_gltf_from_memory( const void* data, std::size_t size, model& out_model, const std::string& base_path = "" );

} // namespace zscene