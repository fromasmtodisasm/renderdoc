#include "d3d9_swapchain.h"

#include "serialise/serialiser.h"

WrappedD3DSwapChain9::WrappedD3DSwapChain9(IDirect3DSwapChain9 *swapChain,
                                           WrappedD3DDevice9 *device, UINT index, bool implicit)
    : m_RefCounter(swapChain, false),
      m_SoftRefCounter(NULL, false),
	m_Implicit(implicit),
      m_SwapChain(swapChain),
      m_Device(device),
      m_Index(index)
{
  m_SwapChain->QueryInterface(__uuidof(IDirect3DSwapChain9Ex), (void **)&m_SwapChainEx);
  m_Device->SoftRef();
  m_Device->InternalRef();    // swapchains are never set as resources
  if (m_Implicit)
  {
	  m_RefCounter.Release();
	  RDCASSERT(m_RefCounter.GetRefCount() == 0);
  }
}

HRESULT __stdcall WrappedD3DSwapChain9::QueryInterface(REFIID riid, void **ppvObj)
{
  if(riid == __uuidof(IDirect3DSwapChain9))
  {
    HRESULT hr = m_SwapChain->QueryInterface(riid, ppvObj);

    if(SUCCEEDED(hr))
    {
      AddRef();
      m_SwapChain->Release();
      *ppvObj = this;
      return S_OK;
    }
    else
    {
      *ppvObj = NULL;
      return hr;
    }
  }
  else if(riid == __uuidof(IDirect3DSwapChain9Ex))
  {
    HRESULT hr = m_SwapChain->QueryInterface(riid, ppvObj);

    if(SUCCEEDED(hr))
    {
      AddRef();
      m_SwapChain->Release();
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

  return m_SwapChain->QueryInterface(riid, ppvObj);
}

ULONG __stdcall WrappedD3DSwapChain9::AddRef()
{
  m_Device->SoftRef();
  return m_RefCounter.AddRef();
}

ULONG __stdcall WrappedD3DSwapChain9::Release()
{
  m_Device->SoftRelease();
  ULONG ret = m_RefCounter.Release();
  if(ret == 0 && !m_Implicit)
  {
    m_Device->RemoveSwapchain(m_Index);
    m_Device->SoftRelease();

    m_SwapChainEx->Release();

    UINT refCount = m_SwapChain->Release();
    RDCASSERT(refCount == 0);
    delete this;
  }
  return ret;
}

HRESULT __stdcall WrappedD3DSwapChain9::Present(CONST RECT *pSourceRect, CONST RECT *pDestRect,
                                                HWND hDestWindowOverride,
                                                CONST RGNDATA *pDirtyRegion, DWORD dwFlags)
{
  m_Device->RenderOverlay(hDestWindowOverride);
  return m_SwapChain->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetFrontBufferData(IDirect3DSurface9 *pDestSurface)
{
  return m_SwapChain->GetFrontBufferData(pDestSurface);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type,
                                                      IDirect3DSurface9 **ppBackBuffer)
{
  return m_SwapChain->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetRasterStatus(D3DRASTER_STATUS *pRasterStatus)
{
  return m_SwapChain->GetRasterStatus(pRasterStatus);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetDisplayMode(D3DDISPLAYMODE *pMode)
{
  return m_SwapChain->GetDisplayMode(pMode);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetDevice(IDirect3DDevice9 **ppDevice)
{
  IDirect3DDevice9 *device;
  HRESULT res = m_SwapChain->GetDevice(&device);
  if(res == S_OK)
  {
    // transfer the refcount to the wrapper
    m_Device->AddRef();
    device->Release();
    *ppDevice = m_Device;
  }
  return res;
}

HRESULT __stdcall WrappedD3DSwapChain9::GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters)
{
  return m_SwapChain->GetPresentParameters(pPresentationParameters);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetLastPresentCount(UINT *pLastPresentCount)
{
  RDCASSERT(m_SwapChainEx != NULL);
  return m_SwapChainEx->GetLastPresentCount(pLastPresentCount);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetPresentStats(D3DPRESENTSTATS *pPresentationStatistics)
{
  RDCASSERT(m_SwapChainEx != NULL);
  return m_SwapChainEx->GetPresentStats(pPresentationStatistics);
}

HRESULT __stdcall WrappedD3DSwapChain9::GetDisplayModeEx(D3DDISPLAYMODEEX *pMode,
                                                         D3DDISPLAYROTATION *pRotation)
{
  RDCASSERT(m_SwapChainEx != NULL);
  return m_SwapChainEx->GetDisplayModeEx(pMode, pRotation);
}
