#pragma once

#include <new>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Wmcodecdsp.h>
#include <assert.h>
#include <Dbt.h>
#include <shlwapi.h>

const UINT WM_APP_AUDIO_PREVIEW_ERROR = WM_APP + 1;

class CAudioCapture :
    public IMFSourceReaderCallback
{
public:
    static HRESULT CreateInstance(
        HWND     hwnd,
        CAudioCapture** ppPlayer
    );

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IMFSourceReaderCallback methods
    STDMETHODIMP OnReadSample(
        HRESULT hrStatus,
        DWORD dwStreamIndex,
        DWORD dwStreamFlags,
        LONGLONG llTimestamp,
        IMFSample* pSample
    );

    STDMETHODIMP OnEvent(DWORD, IMFMediaEvent*)
    {
        return S_OK;
    }

    STDMETHODIMP OnFlush(DWORD)
    {
        return S_OK;
    }

    HRESULT     StartCapture(IMFActivate* pActivate, const WCHAR* pwszFileName);
    HRESULT     EndCaptureSession();
    BOOL        IsCapturing();
    HRESULT     CheckDeviceLost(DEV_BROADCAST_HDR* pHdr, BOOL* pbDeviceLost);
    HRESULT     ConfigureSession(IMFActivate* pActivate, const WCHAR* pwszFileName);

protected:

    enum State
    {
        State_NotReady = 0,
        State_Ready,
        State_Capturing,
    };

    // Constructor is private. Use static CreateInstance method to instantiate.
    CAudioCapture(HWND hwnd);

    // Destructor is private. Caller should call Release.
    virtual ~CAudioCapture();

    void    NotifyError(HRESULT hr) { PostMessage(m_hwndEvent, WM_APP_AUDIO_PREVIEW_ERROR, (WPARAM)hr, 0L); }

    HRESULT OpenMediaSource(IMFMediaSource* pSource);
    HRESULT ConfigureCapture();

    long                    m_nRefCount;        // Reference count.
    CRITICAL_SECTION        m_critsec;

    HWND                    m_hwndEvent;        // Application window to receive events. 

    IMFSourceReader* m_pReader;
    IMFSinkWriter* m_pWriter;

    BOOL                    m_bFirstSample;
    LONGLONG                m_llBaseTime;

    WCHAR* m_pwszSymbolicLink;
};

