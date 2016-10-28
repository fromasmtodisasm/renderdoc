#include "d3d9_swapchain.h"

#include "serialise/serialiser.h"

WrappedD3DSwapChain9::WrappedD3DSwapChain9(IDirect3DSwapChain9* swapChain, WrappedD3DDevice9* device)
	: m_SwapChain(swapChain)
	, m_Device(device)
{
	m_SwapChain->QueryInterface(__uuidof(IDirect3DSwapChain9Ex), (void **)&m_SwapChainEx);
}

HRESULT __stdcall WrappedD3DSwapChain9::QueryInterface(REFIID riid, void** ppvObj)
{
  if (riid == __uuidof(IDirect3DSwapChain9))
  {
	  HRESULT hr = m_Device->QueryInterface(riid, ppvObj);

	  if (SUCCEEDED(hr))
	  {
		  *ppvObj = this;
		  return S_OK;
	  }
	  else
	  {
		  *ppvObj = NULL;
		  return hr;
	  }
  }
  else if (riid == __uuidof(IDirect3DSwapChain9Ex))
  {
	  HRESULT hr = m_Device->QueryInterface(riid, ppvObj);

	  if (SUCCEEDED(hr))
	  {
		  *ppvObj = this;
		  return S_OK;
	  }
	  else
	  {
		  *ppvObj = NULL;
		  return hr;
	  }
  }
  else
  {
	  string guid = ToStr::Get(riid);
	  RDCWARN("Querying IDirect3DSwapChain for interface: %s", guid.c_str());
  }

  return m_Device->QueryInterface(riid, ppvObj);
}

ULONG __stdcall WrappedD3DSwapChain9::AddRef()
{
	return m_SwapChain->AddRef() - 1;
}

ULONG __stdcall WrappedD3DSwapChain9::Release()
{
	ULONG ref = m_SwapChain->Release();
	if (ref == 1)
	{
		//noone else is holding this object anymore
		//we can release our own reference
		m_SwapChain->Release();
		delete this;
	}
	return ref - 1;
}

HRESULT __stdcall WrappedD3DSwapChain9::Present(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags)
{
	//m_SwapChain->AddRef();
	//ULONG refcountBefore = m_SwapChain->Release();

	m_Device->RenderOverlay(hDestWindowOverride);

	//m_SwapChain->AddRef();
	//ULONG refcountAfter = m_SwapChain->Release();

	//refcountBefore;
	//refcountAfter;

	return m_SwapChain->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetFrontBufferData(IDirect3DSurface9* pDestSurface)
{
	return m_SwapChain->GetFrontBufferData(pDestSurface);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer)
{
	return m_SwapChain->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetRasterStatus(D3DRASTER_STATUS* pRasterStatus)
{
	return m_SwapChain->GetRasterStatus(pRasterStatus);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetDisplayMode(D3DDISPLAYMODE* pMode)
{
	return m_SwapChain->GetDisplayMode(pMode);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetDevice(IDirect3DDevice9** ppDevice)
{
	IDirect3DDevice9* device;
	HRESULT res = m_SwapChain->GetDevice(&device);
	if (res == S_OK)
	{
		*ppDevice = m_Device;
	}
	return res;
}

HRESULT __stdcall WrappedD3DSwapChain9::GetPresentParameters(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	return m_SwapChain->GetPresentParameters(pPresentationParameters);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetLastPresentCount(UINT* pLastPresentCount)
{
	RDCASSERT(m_SwapChainEx != NULL);
	return m_SwapChainEx->GetLastPresentCount(pLastPresentCount);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetPresentStats(D3DPRESENTSTATS* pPresentationStatistics)
{
	RDCASSERT(m_SwapChainEx != NULL);
	return m_SwapChainEx->GetPresentStats(pPresentationStatistics);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetDisplayModeEx(D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation)
{
	RDCASSERT(m_SwapChainEx != NULL);
	return m_SwapChainEx->GetDisplayModeEx(pMode, pRotation);
}
