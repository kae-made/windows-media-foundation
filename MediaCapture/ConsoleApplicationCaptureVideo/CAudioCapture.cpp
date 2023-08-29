#include "CAudioCapture.h"
#include <Mferror.h>

template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

HRESULT CopyAttribute(IMFAttributes* pSrc, IMFAttributes* pDest, const GUID& key);

//-------------------------------------------------------------------
//  CreateInstance
//
//  Static class method to create the CCapture object.
//-------------------------------------------------------------------

HRESULT CAudioCapture::CreateInstance(
    HWND     hwnd,       // Handle to the window to receive events
    CAudioCapture** ppCapture // Receives a pointer to the CAudioCapture object.
)
{
    if (ppCapture == NULL)
    {
        return E_POINTER;
    }

    CAudioCapture* pCapture = new (std::nothrow) CAudioCapture(hwnd);

    if (pCapture == NULL)
    {
        return E_OUTOFMEMORY;
    }

    // The CAudioCapture constructor sets the ref count to 1.
    *ppCapture = pCapture;

    return S_OK;
}


//-------------------------------------------------------------------
//  constructor
//-------------------------------------------------------------------

CAudioCapture::CAudioCapture(HWND hwnd) :
    m_pReader(NULL),
    m_pWriter(NULL),
    m_hwndEvent(hwnd),
    m_nRefCount(1),
    m_bFirstSample(FALSE),
    m_llBaseTime(0),
    m_pwszSymbolicLink(NULL)
{
    InitializeCriticalSection(&m_critsec);
}

//-------------------------------------------------------------------
//  destructor
//-------------------------------------------------------------------

CAudioCapture::~CAudioCapture()
{
    assert(m_pReader == NULL);
    assert(m_pWriter == NULL);
    DeleteCriticalSection(&m_critsec);
}



/////////////// IUnknown methods ///////////////

//-------------------------------------------------------------------
//  AddRef
//-------------------------------------------------------------------

ULONG CAudioCapture::AddRef()
{
    return InterlockedIncrement(&m_nRefCount);
}


//-------------------------------------------------------------------
//  Release
//-------------------------------------------------------------------

ULONG CAudioCapture::Release()
{
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    if (uCount == 0)
    {
        delete this;
    }
    return uCount;
}



//-------------------------------------------------------------------
//  QueryInterface
//-------------------------------------------------------------------

HRESULT CAudioCapture::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(CAudioCapture, IMFSourceReaderCallback),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}


/////////////// IMFSourceReaderCallback methods ///////////////

//-------------------------------------------------------------------
// OnReadSample
//
// Called when the IMFMediaSource::ReadSample method completes.
//-------------------------------------------------------------------

HRESULT CAudioCapture::OnReadSample(
    HRESULT hrStatus,
    DWORD /*dwStreamIndex*/,
    DWORD /*dwStreamFlags*/,
    LONGLONG llTimeStamp,
    IMFSample* pSample      // Can be NULL
)
{
    EnterCriticalSection(&m_critsec);

    if (!IsCapturing())
    {
        LeaveCriticalSection(&m_critsec);
        return S_OK;
    }

    HRESULT hr = S_OK;
    DWORD actualStreamIndex = 0;
    DWORD streamFlags = 0;

    if (FAILED(hrStatus))
    {
        hr = hrStatus;
        goto done;
    }

    if (pSample)
    {
        if (m_bFirstSample)
        {
            m_llBaseTime = llTimeStamp;
            m_bFirstSample = FALSE;
        }

        // rebase the time stamp
        llTimeStamp -= m_llBaseTime;

        hr = pSample->SetSampleTime(llTimeStamp);

        if (FAILED(hr)) { goto done; }

        hr = m_pWriter->WriteSample(0, pSample);

        if (FAILED(hr)) { goto done; }
    }


    // Read another sample.
    hr = m_pReader->ReadSample(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        0,
        NULL,   // actual
        NULL,   // flags
        NULL,   // timestamp
        NULL    // sample
    );

done:
    if (FAILED(hr))
    {
        NotifyError(hr);
    }

    LeaveCriticalSection(&m_critsec);
    return hr;
}


//-------------------------------------------------------------------
// OpenMediaSource
//
// Set up preview for a specified media source. 
//-------------------------------------------------------------------

HRESULT CAudioCapture::OpenMediaSource(IMFMediaSource* pSource)
{
    HRESULT hr = S_OK;

    IMFAttributes* pAttributes = NULL;

    hr = MFCreateAttributes(&pAttributes, 2);

    if (SUCCEEDED(hr))
    {
        hr = pAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);
    }

    if (SUCCEEDED(hr))
    {
        hr = MFCreateSourceReaderFromMediaSource(
            pSource,
            pAttributes,
            &m_pReader
        );
    }

    SafeRelease(&pAttributes);
    return hr;
}

//-------------------------------------------------------------------
// ConfigureMediaSource
//
// Configure media source.
//-------------------------------------------------------------------

HRESULT CAudioCapture::ConfigureMediaSource(IMFActivate* pDevice)
{
    HRESULT hr = S_OK;

    IMFMediaSource* pSource = NULL;
    DWORD actualStreamIndex = 0;
    DWORD streamFlags = 0;

    EnterCriticalSection(&m_critsec);

    // Create the media source for the device.
    hr = pDevice->ActivateObject(
        __uuidof(IMFMediaSource),
        (void**)&pSource
    );

    // Get the symbolic link. This is needed to handle device-
    // loss notifications. (See CheckDeviceLost.)

    if (SUCCEEDED(hr))
    {
        hr = pDevice->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_SYMBOLIC_LINK,
            &m_pwszSymbolicLink,
            NULL
        );
    }

    if (SUCCEEDED(hr))
    {
        hr = OpenMediaSource(pSource);
    }

    if (SUCCEEDED(hr)) {
        hr = ConfigureSourceReader(m_pReader);

        if (SUCCEEDED(hr))
        {
            hr = m_pReader->GetCurrentMediaType(
                (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                &m_pSourceMediaType
            );
        }
    }

    LeaveCriticalSection(&m_critsec);

    return hr;
}

//-------------------------------------------------------------------
// ConfigureMediaSink
//
// Configure media sink.
//-------------------------------------------------------------------

HRESULT CAudioCapture::ConfigureMediaSink(
    const WCHAR* pwszFileName
)
{
    HRESULT hr = S_OK;
    DWORD sink_stream = 0;

    EnterCriticalSection(&m_critsec);

    // Create the sink writer 
    if (SUCCEEDED(hr))
    {
        hr = MFCreateSinkWriterFromURL(
            pwszFileName,
            NULL,
            NULL,
            &m_pWriter
        );
    }
    if (SUCCEEDED(hr))
    {
        hr = ConfigureEncoder(m_pSourceMediaType, m_pWriter, &sink_stream);
    }



    if (SUCCEEDED(hr))
    {
        hr = m_pWriter->SetInputMediaType(sink_stream, m_pSourceMediaType, NULL);
    }

    LeaveCriticalSection(&m_critsec);
    
    return hr;
}

//-------------------------------------------------------------------
// Start
//
// Start capturing.
//-------------------------------------------------------------------

HRESULT CAudioCapture::Start()
{
    HRESULT hr = S_OK;

    EnterCriticalSection(&m_critsec);

    if (m_pWriter) {
        hr = m_pWriter->BeginWriting();
    }
    else {
        hr = S_FALSE;
    }


    if (SUCCEEDED(hr))
    {
        m_bFirstSample = TRUE;
        m_llBaseTime = 0;

        // Request the first audio frame.
        // https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-readsample#asynchronous-mode
        // variables should be NULL in the case of asynchronous
        hr = m_pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            NULL,
            NULL,
            NULL,
            NULL
        );
    }


    LeaveCriticalSection(&m_critsec);

    return hr;
}

//-------------------------------------------------------------------
// StopAndFlush
//
// Stop capturing and flush out contents to sink.
//-------------------------------------------------------------------
HRESULT CAudioCapture::StopAndFlush()
{
    HRESULT hr = S_OK;

    EnterCriticalSection(&m_critsec);

    if (m_pWriter)
    {
        hr = m_pWriter->Finalize();
    }

    SafeRelease(&m_pWriter);

    LeaveCriticalSection(&m_critsec);

    return hr;
}

//-------------------------------------------------------------------
// EndSession
//
// End capturing session.
//-------------------------------------------------------------------
HRESULT CAudioCapture::EndSession()
{
    HRESULT hr = S_OK;

    EnterCriticalSection(&m_critsec);

    SafeRelease(&m_pReader);
    SafeRelease(&m_pSourceMediaType);

    LeaveCriticalSection(&m_critsec);

    return hr;
}


//-------------------------------------------------------------------
// StartCapture
//
// Start capturing.
//-------------------------------------------------------------------

HRESULT CAudioCapture::StartCapture(
    IMFActivate* pActivate,
    const WCHAR* pwszFileName
)
{
    HRESULT hr = S_OK;

    IMFMediaSource* pSource = NULL;
    DWORD actualStreamIndex = 0;
    DWORD streamFlags = 0;

    EnterCriticalSection(&m_critsec);

    // Create the media source for the device.
    hr = pActivate->ActivateObject(
        __uuidof(IMFMediaSource),
        (void**)&pSource
    );

    // Get the symbolic link. This is needed to handle device-
    // loss notifications. (See CheckDeviceLost.)

    if (SUCCEEDED(hr))
    {
        hr = pActivate->GetAllocatedString(
             MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_SYMBOLIC_LINK,
            &m_pwszSymbolicLink,
            NULL
        );
    }

    if (SUCCEEDED(hr))
    {
        hr = OpenMediaSource(pSource);
    }

    // Create the sink writer 
    if (SUCCEEDED(hr))
    {
        hr = MFCreateSinkWriterFromURL(
            pwszFileName,
            NULL,
            NULL,
            &m_pWriter
        );
    }

    // Set up the encoding parameters.
    if (SUCCEEDED(hr))
    {
        hr = ConfigureCapture();
    }

    if (SUCCEEDED(hr)) {
        hr = m_pWriter->BeginWriting();
    }

    if (SUCCEEDED(hr))
    {
        m_bFirstSample = TRUE;
        m_llBaseTime = 0;

        // Request the first audio frame.
        // https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-readsample#asynchronous-mode
        // variables should be NULL in the case of asynchronous
        hr = m_pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            NULL,
            NULL,
            NULL,
            NULL
        );
    }

    SafeRelease(&pSource);
    LeaveCriticalSection(&m_critsec);
    return hr;
}


//-------------------------------------------------------------------
// EndCaptureSession
//
// Stop the capture session. 
//
// NOTE: This method resets the object's state to State_NotReady.
// To start another capture session, call SetCaptureFile.
//-------------------------------------------------------------------

HRESULT CAudioCapture::EndCaptureSession()
{
    EnterCriticalSection(&m_critsec);

    HRESULT hr = S_OK;

    if (m_pWriter)
    {
        hr = m_pWriter->Finalize();
    }

    SafeRelease(&m_pWriter);
    SafeRelease(&m_pReader);

    LeaveCriticalSection(&m_critsec);

    return hr;
}


BOOL CAudioCapture::IsCapturing()
{
    EnterCriticalSection(&m_critsec);

    BOOL bIsCapturing = (m_pWriter != NULL);

    LeaveCriticalSection(&m_critsec);

    return bIsCapturing;
}



//-------------------------------------------------------------------
//  CheckDeviceLost
//  Checks whether the video capture device was removed.
//
//  The application calls this method when is receives a 
//  WM_DEVICECHANGE message.
//-------------------------------------------------------------------

HRESULT CAudioCapture::CheckDeviceLost(DEV_BROADCAST_HDR* pHdr, BOOL* pbDeviceLost)
{
    if (pbDeviceLost == NULL)
    {
        return E_POINTER;
    }

    EnterCriticalSection(&m_critsec);

    DEV_BROADCAST_DEVICEINTERFACE* pDi = NULL;

    *pbDeviceLost = FALSE;

    if (!IsCapturing())
    {
        goto done;
    }
    if (pHdr == NULL)
    {
        goto done;
    }
    if (pHdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
    {
        goto done;
    }

    // Compare the device name with the symbolic link.

    pDi = (DEV_BROADCAST_DEVICEINTERFACE*)pHdr;

    if (m_pwszSymbolicLink)
    {
        if (_wcsicmp(m_pwszSymbolicLink, pDi->dbcc_name) == 0)
        {
            *pbDeviceLost = TRUE;
        }
    }

done:
    LeaveCriticalSection(&m_critsec);
    return S_OK;
}


/////////////// Private/protected class methods ///////////////



//-------------------------------------------------------------------
//  ConfigureSourceReader
//
//  Sets the media type on the source reader.
//-------------------------------------------------------------------

HRESULT CAudioCapture::ConfigureSourceReader(IMFSourceReader* pReader)
{
    // The list of acceptable types.
    GUID subtypes[] = {
        MFVideoFormat_NV12, MFVideoFormat_YUY2, MFVideoFormat_UYVY,
        MFVideoFormat_RGB32, MFVideoFormat_RGB24, MFVideoFormat_IYUV
    };

    HRESULT hr = S_OK;
    BOOL    bUseNativeType = FALSE;

    GUID subtype = { 0 };

    IMFMediaType* pType = NULL;
    IMFMediaType* pUncompressedAudioType = NULL;
    IMFMediaType* pPCMAudio = NULL;
    IMFMediaType** ppPCMAudio = &pPCMAudio;

    // Create a partial media type that specifies uncompressed PCM audio.

    hr = MFCreateMediaType(&pType);

    if (SUCCEEDED(hr))
    {
        hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    }

    if (SUCCEEDED(hr))
    {
        hr = pType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    }

    // Set this type on the source reader. The source reader will
    // load the necessary decoder.
    if (SUCCEEDED(hr))
    {
        hr = pReader->SetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            NULL,
            pType
        );
    }

    // Ensure the stream is selected.
    if (SUCCEEDED(hr))
    {
        hr = pReader->SetStreamSelection(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            TRUE
        );
    }

    // Return the PCM format to the caller.
    if (SUCCEEDED(hr))
    {
      //  *ppPCMAudio = pUncompressedAudioType;
      //  (*ppPCMAudio)->AddRef();
    }

    SafeRelease(&pUncompressedAudioType);



done:
    SafeRelease(&pType);
    return hr;
}


// https://learn.microsoft.com/ja-jp/windows/win32/medfound/uncompressed-audio-media-types
HRESULT CreatePCMAudioType(
    UINT32 sampleRate,        // Samples per second
    UINT32 bitsPerSample,     // Bits per sample
    UINT32 cChannels,         // Number of channels
    IMFMediaType** ppType     // Receives a pointer to the media type.
)
{
    HRESULT hr = S_OK;

    IMFMediaType* pType = NULL;

    // Calculate derived values.
    UINT32 blockAlign = cChannels * (bitsPerSample / 8);
    UINT32 bytesPerSecond = blockAlign * sampleRate;

    // Create the empty media type.
    hr = MFCreateMediaType(&pType);
    if (FAILED(hr))
    {
        goto done;
    }

    // Set attributes on the type.
    hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, cChannels);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bytesPerSecond);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    if (FAILED(hr))
    {
        goto done;
    }

    // Return the type to the caller.
    *ppType = pType;
    (*ppType)->AddRef();

done:
    SafeRelease(&pType);
    return hr;
}

HRESULT CAudioCapture::ConfigureEncoder(
    IMFMediaType* pType,
    IMFSinkWriter* pWriter,
    DWORD* pdwStreamIndex
)
{
    HRESULT hr = S_OK;

    GUID majortype = { 0 };
    GUID subtype = { 0 };
    UINT32 cChannels = 0;
    UINT32 samplesPerSec = 0;
    UINT32 bitsPerSample = 0;

    IMFMediaType* pTypeW;

    hr = pType->GetMajorType(&majortype);
    if (FAILED(hr))
    {
        return hr;
    }

    if (majortype != MFMediaType_Audio)
    {
        return MF_E_INVALIDMEDIATYPE;
    }

    // Get the audio subtype.
    hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (FAILED(hr))
    {
        return hr;
    }

    if (subtype == MFAudioFormat_PCM)
    {
        // This is already a PCM audio type. Return the same pointer.

       // pTypeW = pType;
       // pTypeW->AddRef();


   // }
   // else {
        // Get the sample rate and other information from the audio format.

        cChannels = MFGetAttributeUINT32(pType, MF_MT_AUDIO_NUM_CHANNELS, 0);
        samplesPerSec = MFGetAttributeUINT32(pType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
        bitsPerSample = MFGetAttributeUINT32(pType, MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

        // Note: Some encoded audio formats do not contain a value for bits/sample.
        // In that case, use a default value of 16. Most codecs will accept this value.

        if (cChannels == 0 || samplesPerSec == 0)
        {
            return MF_E_INVALIDTYPE;
        }

        // Create the corresponding PCM audio type.
        hr = CreatePCMAudioType(samplesPerSec, bitsPerSample, cChannels, &pTypeW);
        //}

        if (SUCCEEDED(hr))
        {
            hr = pWriter->AddStream(pTypeW, pdwStreamIndex);
        }
    }


    return hr;
}



//-------------------------------------------------------------------
// ConfigureCapture
//
// Configures the capture session.
//
//-------------------------------------------------------------------

HRESULT CAudioCapture::ConfigureCapture()
{
    HRESULT hr = S_OK;
    DWORD sink_stream = 0;

    IMFMediaType* pType = NULL;

    hr = ConfigureSourceReader(m_pReader);

    if (SUCCEEDED(hr))
    {
        hr = m_pReader->GetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            &pType
        );
    }

    if (SUCCEEDED(hr))
    {
        hr = ConfigureEncoder(pType, m_pWriter, &sink_stream);
    }



    if (SUCCEEDED(hr))
    {
        hr = m_pWriter->SetInputMediaType(sink_stream, pType, NULL);
    }

    SafeRelease(&pType);
    return hr;
}


