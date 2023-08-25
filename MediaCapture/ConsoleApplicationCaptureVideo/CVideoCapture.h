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


const UINT WM_APP_PREVIEW_ERROR = WM_APP + 1;
const UINT32 TARGET_BIT_RATE = 240 * 1000;

struct EncodingParameters
{
    GUID    subtype;
    UINT32  bitrate;
};


class CVideoCapture :
    public IMFSourceReaderCallback
{
public:
    static HRESULT CreateInstance(
        HWND     hwnd,
        CVideoCapture** ppPlayer
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

    HRESULT     StartCapture(IMFActivate* pActivate, const WCHAR* pwszFileName, const EncodingParameters& param);
    HRESULT     EndCaptureSession();
    BOOL        IsCapturing();
    HRESULT     CheckDeviceLost(DEV_BROADCAST_HDR* pHdr, BOOL* pbDeviceLost);

protected:

    enum State
    {
        State_NotReady = 0,
        State_Ready,
        State_Capturing,
    };

    // Constructor is private. Use static CreateInstance method to instantiate.
    CVideoCapture(HWND hwnd);

    // Destructor is private. Caller should call Release.
    virtual ~CVideoCapture();

    void    NotifyError(HRESULT hr) { PostMessage(m_hwndEvent, WM_APP_PREVIEW_ERROR, (WPARAM)hr, 0L); }

    HRESULT OpenMediaSource(IMFMediaSource* pSource);
    HRESULT ConfigureCapture(const EncodingParameters& param);
    HRESULT EndCaptureInternal();

    long                    m_nRefCount;        // Reference count.
    CRITICAL_SECTION        m_critsec;

    HWND                    m_hwndEvent;        // Application window to receive events. 

    IMFSourceReader* m_pReader;
    IMFSinkWriter* m_pWriter;

    BOOL                    m_bFirstSample;
    LONGLONG                m_llBaseTime;

    WCHAR* m_pwszSymbolicLink;
};

