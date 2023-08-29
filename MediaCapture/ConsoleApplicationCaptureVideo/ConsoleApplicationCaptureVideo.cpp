// ConsoleApplicationCaptureVideo.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <iostream>

//#define VIDEO_CAPTURE TRUE

#if defined( VIDEO_CAPTURE)
#include "CVideoCapture.h"
#else
#include "CAudioCapture.h"
#endif
#include <ks.h>
#include <ksmedia.h>

template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

#define MY_CONSOLE_WINDOW_TITLE_SIZE 1024
HWND GetWindowHandleOfConsoleApp()
{
    HWND hwndFound;
    wchar_t pszNewWindowTitle[MY_CONSOLE_WINDOW_TITLE_SIZE];
    wchar_t pszOldWindowTitle[MY_CONSOLE_WINDOW_TITLE_SIZE];

    GetConsoleTitle(pszOldWindowTitle, MY_CONSOLE_WINDOW_TITLE_SIZE);
    wsprintf(pszNewWindowTitle, L"%d/%d", GetTickCount(), GetCurrentProcessId());
    SetConsoleTitle(pszNewWindowTitle);
    Sleep(40);
    hwndFound = FindWindow(NULL, pszNewWindowTitle);

    return hwndFound;
}

HRESULT GetDevSource(const GUID& devSourceType, IMFActivate** ppDevice, DWORD index)
{
    UINT32 devCount = 0;
    IMFActivate** ppDevices;

    HRESULT hr = S_OK;
    IMFAttributes* pAttributes = NULL;

    WCHAR* pDevName = NULL;

    *ppDevice = NULL;

    hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) {
        goto done;
    }
    hr = pAttributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        devSourceType);
    if (FAILED(hr)) {
        goto done;
    }
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &devCount);
    if (FAILED(hr) || devCount == 0) {
        goto done;
    }
    SafeRelease(&pAttributes);

    for (UINT32 i = 0; i < devCount; i++) {
        IMFActivate* device = ppDevices[i];
        hr = device->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
            &pDevName,
            NULL);
        if (FAILED(hr)) {
            goto done;
        }
        std::wcout << "Device Name - '" << pDevName << "'" << std::endl;
        CoTaskMemFree(pDevName);
        if (i == index) {
            break;
        }
    }


    *ppDevice = ppDevices[index];
    (*ppDevice)->AddRef();

done:
    return hr;

}

int main()
{
    std::cout << "Hello World!\n";

    HWND hwnd = GetWindowHandleOfConsoleApp();
    DEV_BROADCAST_DEVICEINTERFACE di = { 0 };
    HDEVNOTIFY  g_hdevnotify = NULL;
    UINT32 roundMax = 2;

#if defined(VIDEO_CAPTURE)
    EncodingParameters params;
#endif

    UINT32 devCount = 0;
    IMFActivate* pDevice = NULL;

    HRESULT hr = S_OK;
    IMFAttributes* pAttributes = NULL;

    WCHAR* pDevName = NULL;

#if VIDEO_CAPTURE
    CVideoCapture* pCapture;
#else
    CAudioCapture* pCapture;
#endif

    WCHAR* pwszSymbolicLink = NULL;

#if VIDEO_CAPTURE
    LPCWSTR outputFileName = L"output.mp4";
#else
    LPCWSTR outputFileNames[4] = { L"output0.wav",L"output1.wav" ,L"output2.wav" ,L"output3.wav" };
//    std::wcout << "file name" << outputFileNames[0] << std::endl;
#endif

    // Initialize the COM library
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    // Initialize Media Foundation
    if (SUCCEEDED(hr))
    {
        hr = MFStartup(MF_VERSION);
    }
    else {
        goto done;
    }

    di.dbcc_size = sizeof(di);
    di.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    di.dbcc_classguid = KSCATEGORY_CAPTURE;
    g_hdevnotify = RegisterDeviceNotification(
        hwnd,
        &di,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );

    if (g_hdevnotify == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

#if VIDEO_CAPTURE
    hr = GetDevSource(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID, &pDevice, 0);
#else
    hr = GetDevSource(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID, &pDevice, 1);
#endif

    if (FAILED(hr)) {
        goto done;
    }


    hr = pDevice->GetAllocatedString(
#if VIDEO_CAPTURE
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
#else
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_SYMBOLIC_LINK,
#endif
        &pwszSymbolicLink, NULL);
    if (FAILED(hr)) {
        goto done;
    }

#if VIDEO_CAPTURE
    hr = CVideoCapture::CreateInstance(hwnd, &pCapture);
#else
    hr = CAudioCapture::CreateInstance(hwnd, &pCapture);
#endif
    if (FAILED(hr)) {
        goto done;
    }

#if VIDEO_CAPTURE
     params.subtype = MFVideoFormat_H264;
    params.bitrate = TARGET_BIT_RATE;
    hr = pCapture->StartCapture(pDevice, outputFileName, params);
    hr = pCapture->StartCapture(pDevice, outputFileName);
#else
    hr = pCapture->ConfigureMediaSource(pDevice);
    if (FAILED(hr)) {
        goto done;
    }

    for (UINT32 round = 0; round < roundMax; round++) {
        hr = pCapture->ConfigureMediaSink(outputFileNames[round]);
        if (FAILED(hr)) {
            goto done;
        }
        hr = pCapture->Start();
#endif
        if (FAILED(hr)) {
            goto done;
        }

        std::cout << "Start recording..." << std::endl;
        Sleep(10 * 1000);

#if defined(VIDEO_CAPTURE)
        hr = pCapture->EndCaptureSession();
#else
        std::cout << "Stop recording." << std::endl;
        hr = pCapture->StopAndFlush();
        if (FAILED(hr)) {
            goto done;
        }
        std::wcout << "Flushed to " << outputFileNames[round] << std::endl;
    }
    hr = pCapture->EndSession();
#endif
    std::cout << "End recoding." << std::endl;

    SafeRelease(&pCapture);


done:
    hr = MFShutdown();

    return 0;
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します
