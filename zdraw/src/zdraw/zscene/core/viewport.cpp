#include <include/global.hpp>

namespace zscene {

	bool viewport::create( ID3D11Device* device, int width, int height )
	{
		if ( !device || width <= 0 || height <= 0 )
		{
			return false;
		}

		this->m_width = width;
		this->m_height = height;

		D3D11_TEXTURE2D_DESC tex_desc{};
		tex_desc.Width = static_cast< UINT >( width );
		tex_desc.Height = static_cast< UINT >( height );
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.SampleDesc.Quality = 0;
		tex_desc.Usage = D3D11_USAGE_DEFAULT;
		tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		auto hr = device->CreateTexture2D( &tex_desc, nullptr, &this->m_render_target );
		if ( FAILED( hr ) )
		{
			return false;
		}

		D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{};
		rtv_desc.Format = tex_desc.Format;
		rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

		hr = device->CreateRenderTargetView( this->m_render_target.Get( ), &rtv_desc, &this->m_rtv );
		if ( FAILED( hr ) )
		{
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		srv_desc.Format = tex_desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;

		hr = device->CreateShaderResourceView( this->m_render_target.Get( ), &srv_desc, &this->m_srv );
		if ( FAILED( hr ) )
		{
			return false;
		}

		D3D11_TEXTURE2D_DESC depth_desc{};
		depth_desc.Width = static_cast< UINT >( width );
		depth_desc.Height = static_cast< UINT >( height );
		depth_desc.MipLevels = 1;
		depth_desc.ArraySize = 1;
		depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depth_desc.SampleDesc.Count = 1;
		depth_desc.SampleDesc.Quality = 0;
		depth_desc.Usage = D3D11_USAGE_DEFAULT;
		depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		hr = device->CreateTexture2D( &depth_desc, nullptr, &this->m_depth_texture );
		if ( FAILED( hr ) )
		{
			return false;
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
		dsv_desc.Format = depth_desc.Format;
		dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

		hr = device->CreateDepthStencilView( this->m_depth_texture.Get( ), &dsv_desc, &this->m_dsv );
		if ( FAILED( hr ) )
		{
			return false;
		}

		return true;
	}

	void viewport::destroy( )
	{
		this->m_render_target.Reset( );
		this->m_rtv.Reset( );
		this->m_srv.Reset( );
		this->m_depth_texture.Reset( );
		this->m_dsv.Reset( );
		this->m_width = 0;
		this->m_height = 0;
	}

	bool viewport::resize( ID3D11Device* device, int width, int height )
	{
		if ( width == this->m_width && height == this->m_height )
		{
			return true;
		}

		this->destroy( );
		return this->create( device, width, height );
	}

	void viewport::begin( ID3D11DeviceContext* ctx, float clear_r, float clear_g, float clear_b, float clear_a )
	{
		if ( !ctx )
		{
			return;
		}

		UINT num_viewports{ 1 };
		ctx->RSGetViewports( &num_viewports, &this->m_old_viewport );
		ctx->OMGetRenderTargets( 1, &this->m_old_rtv, &this->m_old_dsv );
		ctx->OMSetRenderTargets( 1, this->m_rtv.GetAddressOf( ), this->m_dsv.Get( ) );

		const float clear_color[ 4 ] = { clear_r, clear_g, clear_b, clear_a };
		ctx->ClearRenderTargetView( this->m_rtv.Get( ), clear_color );
		ctx->ClearDepthStencilView( this->m_dsv.Get( ), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );

		D3D11_VIEWPORT vp{};
		vp.Width = static_cast< float >( this->m_width );
		vp.Height = static_cast< float >( this->m_height );
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0.0f;
		vp.TopLeftY = 0.0f;
		ctx->RSSetViewports( 1, &vp );
	}

	void viewport::end( ID3D11DeviceContext* ctx )
	{
		if ( !ctx )
		{
			return;
		}

		ctx->OMSetRenderTargets( 1, this->m_old_rtv.GetAddressOf( ), this->m_old_dsv.Get( ) );
		ctx->RSSetViewports( 1, &this->m_old_viewport );

		this->m_old_rtv.Reset( );
		this->m_old_dsv.Reset( );
	}

} // namespace zscene