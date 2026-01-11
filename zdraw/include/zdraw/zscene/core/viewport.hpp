#pragma once

namespace zscene {

	using Microsoft::WRL::ComPtr;

	struct viewport
	{
		ComPtr<ID3D11Texture2D> m_render_target{};
		ComPtr<ID3D11RenderTargetView> m_rtv{};
		ComPtr<ID3D11ShaderResourceView> m_srv{};
		ComPtr<ID3D11Texture2D> m_depth_texture{};
		ComPtr<ID3D11DepthStencilView> m_dsv{};

		ComPtr<ID3D11RenderTargetView> m_old_rtv{};
		ComPtr<ID3D11DepthStencilView> m_old_dsv{};
		D3D11_VIEWPORT m_old_viewport{};

		int m_width{ 0 };
		int m_height{ 0 };

		bool create( ID3D11Device* device, int width, int height );
		void destroy( );
		bool resize( ID3D11Device* device, int width, int height );

		void begin( ID3D11DeviceContext* ctx, float clear_r = 0.1f, float clear_g = 0.1f, float clear_b = 0.1f, float clear_a = 1.0f );
		void end( ID3D11DeviceContext* ctx );

		[[nodiscard]] ID3D11ShaderResourceView* get_srv( ) const noexcept { return this->m_srv.Get( ); }
		[[nodiscard]] float aspect_ratio( ) const noexcept { return this->m_height > 0 ? static_cast< float >( this->m_width ) / static_cast< float >( this->m_height ) : 1.0f; }
	};

} // namespace zscene
