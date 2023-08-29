// ConsoleApplicationSoundPlaybackTest.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
// https://sumiretool.net/article/d/blog/Media+Foundation+%E3%82%92%E4%BD%BF%E3%81%86%EF%BC%88%E4%B8%8B%E6%BA%96%E5%82%99%E3%81%A8%E9%9F%B3%E5%A3%B0%E3%83%95%E3%82%A1%E3%82%A4%E3%83%AB%E3%81%AE%E5%86%8D%E7%94%9F%EF%BC%89

#include <iostream>
#include <mfapi.h>
#pragma comment(lib, "mfplat.lib")

#include <mfidl.h>
#include <mfreadwrite.h>

template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}


// https://learn.microsoft.com/ja-jp/troubleshoot/windows-server/performance/obtain-console-window-handle#sample-code
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

// from player.cpp in sample
HRESULT CreateMediaSinkActivate(
    IMFStreamDescriptor* pSourceSD,     // Pointer to the stream descriptor.
    HWND hVideoWindow,                  // Handle to the video clipping window.
    IMFActivate** ppActivate
)
{
    IMFMediaTypeHandler* pHandler = NULL;
    IMFActivate* pActivate = NULL;

    // Get the media type handler for the stream.
    HRESULT hr = pSourceSD->GetMediaTypeHandler(&pHandler);
    if (FAILED(hr))
    {
        goto done;
    }

    // Get the major media type.
    GUID guidMajorType;
    hr = pHandler->GetMajorType(&guidMajorType);
    if (FAILED(hr))
    {
        goto done;
    }

    // Create an IMFActivate object for the renderer, based on the media type.
    if (MFMediaType_Audio == guidMajorType)
    {
        // Create the audio renderer.
        hr = MFCreateAudioRendererActivate(&pActivate);
    }
    else if (MFMediaType_Video == guidMajorType)
    {
        // Create the video renderer.
        hr = MFCreateVideoRendererActivate(hVideoWindow, &pActivate);
    }
    else
    {
        // Unknown stream type. 
        hr = E_FAIL;
        // Optionally, you could deselect this stream instead of failing.
    }
    if (FAILED(hr))
    {
        goto done;
    }

    // Return IMFActivate pointer to caller.
    *ppActivate = pActivate;
    (*ppActivate)->AddRef();

done:
    SafeRelease(&pHandler);
    SafeRelease(&pActivate);
    return hr;
}

// Add a source node to a topology.
HRESULT AddSourceNode(
    IMFTopology* pTopology,           // Topology.
    IMFMediaSource* pSource,          // Media source.
    IMFPresentationDescriptor* pPD,   // Presentation descriptor.
    IMFStreamDescriptor* pSD,         // Stream descriptor.
    IMFTopologyNode** ppNode)         // Receives the node pointer.
{
    IMFTopologyNode* pNode = NULL;

    // Create the node.
    HRESULT hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pNode);
    if (FAILED(hr))
    {
        goto done;
    }

    // Set the attributes.
    hr = pNode->SetUnknown(MF_TOPONODE_SOURCE, pSource);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD);
    if (FAILED(hr))
    {
        goto done;
    }

    // Add the node to the topology.
    hr = pTopology->AddNode(pNode);
    if (FAILED(hr))
    {
        goto done;
    }

    // Return the pointer to the caller.
    *ppNode = pNode;
    (*ppNode)->AddRef();

done:
    SafeRelease(&pNode);
    return hr;
}

// Add an output node to a topology.
HRESULT AddOutputNode(
    IMFTopology* pTopology,     // Topology.
    IMFActivate* pActivate,     // Media sink activation object.
    DWORD dwId,                 // Identifier of the stream sink.
    IMFTopologyNode** ppNode)   // Receives the node pointer.
{
    IMFTopologyNode* pNode = NULL;

    // Create the node.
    HRESULT hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pNode);
    if (FAILED(hr))
    {
        goto done;
    }

    // Set the object pointer.
    hr = pNode->SetObject(pActivate);
    if (FAILED(hr))
    {
        goto done;
    }

    // Set the stream sink ID attribute.
    hr = pNode->SetUINT32(MF_TOPONODE_STREAMID, dwId);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE);
    if (FAILED(hr))
    {
        goto done;
    }

    // Add the node to the topology.
    hr = pTopology->AddNode(pNode);
    if (FAILED(hr))
    {
        goto done;
    }

    // Return the pointer to the caller.
    *ppNode = pNode;
    (*ppNode)->AddRef();

done:
    SafeRelease(&pNode);
    return hr;
}



// https://learn.microsoft.com/en-us/windows/win32/medfound/creating-playback-topologies#connecting-streams-to-media-sinks
//  Add a topology branch for one stream.
//
//  For each stream, this function does the following:
//
//    1. Creates a source node associated with the stream. 
//    2. Creates an output node for the renderer. 
//    3. Connects the two nodes.
//
//  The media session will add any decoders that are needed.
HRESULT AddBranchToPartialTopology(
    IMFTopology* pTopology,         // Topology.
    IMFMediaSource* pSource,        // Media source.
    IMFPresentationDescriptor* pPD, // Presentation descriptor.
    DWORD iStream,                  // Stream index.
    HWND hVideoWnd)                 // Window for video playback.
{
    IMFStreamDescriptor* pSD = NULL;
    IMFActivate* pSinkActivate = NULL;
    IMFTopologyNode* pSourceNode = NULL;
    IMFTopologyNode* pOutputNode = NULL;

    BOOL fSelected = FALSE;

    HRESULT hr = pPD->GetStreamDescriptorByIndex(iStream, &fSelected, &pSD);
    if (FAILED(hr))
    {
        goto done;
    }

    if (fSelected)
    {
        // Create the media sink activation object.
        hr = CreateMediaSinkActivate(pSD, hVideoWnd, &pSinkActivate);
        if (FAILED(hr))
        {
            goto done;
        }

        // Add a source node for this stream.
        hr = AddSourceNode(pTopology, pSource, pPD, pSD, &pSourceNode);
        if (FAILED(hr))
        {
            goto done;
        }

        // Create the output node for the renderer.
        hr = AddOutputNode(pTopology, pSinkActivate, 0, &pOutputNode);
        if (FAILED(hr))
        {
            goto done;
        }

        // Connect the source node to the output node.
        hr = pSourceNode->ConnectOutput(0, pOutputNode, 0);
    }
    // else: If not selected, don't add the branch. 

done:
    SafeRelease(&pSD);
    SafeRelease(&pSinkActivate);
    SafeRelease(&pSourceNode);
    SafeRelease(&pOutputNode);
    return hr;
}

int main()
{
    std::cout << "Hello World!\n";

    LPCWSTR pFilePath = L"C:\\Users\\kae-m\\OneDrive - Knowledge & Experience\\ドキュメント\\private\\山形東東京同窓会\\曲\\1_蛍_count.mp3";

    IMFMediaSession* pSession = nullptr;

    IMFMediaSource* pMediaSource = nullptr; // <- mfidl.h
    IMFPresentationDescriptor* pSourcePD = nullptr;
    MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;
    IMFSourceResolver* pSourceResolver = nullptr;
    IUnknown* pSource = nullptr;

    IMFTopology* pTopology = nullptr;
    DWORD cSourceStreams = 0;

    HWND hVideoWnd = GetWindowHandleOfConsoleApp();

    PROPVARIANT varStart;

    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (SUCCEEDED(hr)) {
        std::cout << "Media Foundation initialized" << std::endl;
    }
    else {
        std::cout << "Media Foundation initialization failed - " << hr << std::endl;
        goto done;
    }

    hr = MFCreateMediaSession(NULL, &pSession);
    if (SUCCEEDED(hr)) {
        std::cout << "Media Session created" << std::endl;
    }
    else {
        std::cout << "Media Session creation failed - " << hr << std::endl;
        goto done;
    }

    hr = MFCreateSourceResolver(&pSourceResolver);
    if (FAILED(hr)) {
        std::cout << "Source Reslover creation failed - " << hr << std::endl;
        goto done;
    }

    hr = pSourceResolver->CreateObjectFromURL(pFilePath, MF_RESOLUTION_MEDIASOURCE, NULL, &objectType, &pSource);
    if (FAILED(hr)) {
        std::cout << "Source creation failed - " << hr << std::endl;
        goto done;
    }

    hr = pSource->QueryInterface(IID_PPV_ARGS(&pMediaSource));
    if (FAILED(hr)) {
        std::cout << "Media Source interface query failed - " << hr << std::endl;
        goto done;
    }

    SafeRelease(&pSourceResolver);
    SafeRelease(&pSource);

    hr = pMediaSource->CreatePresentationDescriptor(&pSourcePD);
    if (FAILED(hr)) {
        std::cout << "Presentation Descriptor creation failed - " << hr << std::endl;
        goto done;
    }

    hr = MFCreateTopology(&pTopology);
    if (FAILED(hr)) {
        std::cout << "Topology creation failed - " << hr << std::endl;
        goto done;
    }

    hr = pSourcePD->GetStreamDescriptorCount(&cSourceStreams);
    if (FAILED(hr)) {
        std::cout << "Stream Descriptor Count get failed - " << hr << std::endl;
        goto done;
    }

    for (DWORD i = 0; i < cSourceStreams; i++) {
        hr = AddBranchToPartialTopology(pTopology, pMediaSource, pSourcePD, i, hVideoWnd);
        if (FAILED(hr)) {
            std::cout << "Add Branch To Partial Topology failed - " << hr << std::endl;
            goto done;
        }
    }
    
    hr = pSession->SetTopology(0, pTopology);
    if (FAILED(hr)) {
        std::cout << "Set Topology failed - " << hr << std::endl;
        goto done;
    }

    PropVariantInit(&varStart);

    hr = pSession->Start(&GUID_NULL, &varStart);
    if (FAILED(hr)) {
        std::cout << "Start Session failed - " << hr << std::endl;
        goto done;
    }
    else {
        PropVariantClear(&varStart);
    }

    do {
        std::cout << "Press a key 'q' to quit...";
    } while (std::cin.get() != 'q');

    hr = pSession->Stop();
    if (FAILED(hr)) {
        std::cout << "Stop Session failed - " << hr << std::endl;
        goto done;
    }

    hr = pMediaSource->Shutdown();
    if (FAILED(hr)) {
        std::cout << "Shutdown Media Source failed - " << hr << std::endl;
        goto done;
    }

    hr = pSession->Shutdown();
    if (FAILED(hr)) {
        std::cout << "Shutdown Session failed - " << hr << std::endl;
        goto done;
    }

done:
    std::cout << "Finished";
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
