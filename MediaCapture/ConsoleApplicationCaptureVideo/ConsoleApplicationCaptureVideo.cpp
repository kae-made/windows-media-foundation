// ConsoleApplicationCaptureVideo.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <iostream>
#include "CVideoCapture.h"
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
int main()
{
    std::cout << "Hello World!\n";

    HWND hwnd = GetWindowHandleOfConsoleApp();
    DEV_BROADCAST_DEVICEINTERFACE di = { 0 };
    HDEVNOTIFY  g_hdevnotify = NULL;

    UINT32 devCount = 0;
    IMFActivate** ppDevices;
    IMFActivate* pDevice = NULL;

    HRESULT hr = S_OK;
    IMFAttributes* pAttributes = NULL;

    WCHAR* pDevName = NULL;

    CVideoCapture* pCapture;
    

    IMFMediaSource* pSource = NULL;
    WCHAR* pwszSymbolicLink = NULL;

    LPCWSTR outputFileName = L"output.mp4";
    EncodingParameters params;

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


    hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) {
        goto done;
    }
    hr = pAttributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        goto done;
    }
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &devCount);
    if (FAILED(hr) || devCount == 0) {
        goto done;
    }
    SafeRelease(&pAttributes);

    pDevice = ppDevices[0];
    pDevice->AddRef();
    
    hr = pDevice->GetAllocatedString(
        MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
        &pDevName,
        NULL);
    if (FAILED(hr)) {
        goto done;
    }
    std::cout << "Device Name - '" << pDevName << "'" << std::endl;
    CoTaskMemFree(pDevName);

    hr = pDevice->GetAllocatedString(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
        &pwszSymbolicLink, NULL);
    if (FAILED(hr)) {
        goto done;
    }

    hr = CVideoCapture::CreateInstance(hwnd, &pCapture);
    if (FAILED(hr)) {
        goto done;
    }

    params.subtype = MFVideoFormat_H264;
    params.bitrate = TARGET_BIT_RATE;

    hr = pCapture->StartCapture(pDevice, outputFileName, params);
    if (FAILED(hr)) {
        goto done;
    }

    Sleep(30 * 1000);

    hr = pCapture->EndCaptureSession();

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
