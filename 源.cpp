







//hook com组件
#define CINTERFACE//定义了这个以后引用com接口就得用C语言的方式了
#include <windows.h>
#include <stdio.h>
#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <Audiopolicy.h>
#include <Devicetopology.h>
#include <Endpointvolume.h>

#include "include/detours.h"


#pragma comment(lib,"detours.lib")

#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->lpVtbl->Release(); (punk) = NULL; }


//靠，还得自己制作UUID
struct DECLSPEC_UUID("A95664D2-9614-4F35-A746-DE8DB63617E6") DECLSPEC_NOVTABLE LX_IMMDeviceEnumerator;
struct DECLSPEC_UUID("1CB9AD4C-DBFA-4c32-B178-C2F568A703B2") DECLSPEC_NOVTABLE LX_IAudioClient;
struct DECLSPEC_UUID("F294ACFC-3146-4483-A7BF-ADDCA7C260E2") DECLSPEC_NOVTABLE LX_IAudioRenderClient;



const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(LX_IMMDeviceEnumerator);// 
const IID IID_IAudioClient = __uuidof(LX_IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(LX_IAudioRenderClient);



//目标：hook pRenderClient->ReleaseBuffer
HRESULT(STDMETHODCALLTYPE* True_ReleaseBuffer)(
    IAudioRenderClient* This,
    /* [annotation][in] */
    _In_  UINT32 NumFramesWritten,
    /* [annotation][in] */
    _In_  DWORD dwFlags) = NULL;

HRESULT(STDMETHODCALLTYPE MyReleaseBuffer)(
    IAudioRenderClient* This,
    /* [annotation][in] */
    _In_  UINT32 NumFramesWritten,
    /* [annotation][in] */
    _In_  DWORD dwFlags)
{
    printf("MyReleaseBuffer");
    return (True_ReleaseBuffer(This, NumFramesWritten, dwFlags));
}


BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (dwReason == DLL_PROCESS_ATTACH) 
    {
        DetourRestoreAfterWith();

        HRESULT hr;
#define REFTIMES_PER_SEC  10000000
        REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
        REFERENCE_TIME hnsActualDuration;
        IMMDeviceEnumerator* pEnumerator = NULL;
        IMMDevice* pDevice = NULL;
        IAudioClient* pAudioClient = NULL;
        IAudioRenderClient* pRenderClient = NULL;
        WAVEFORMATEX* pwfx = NULL;
        UINT32 bufferFrameCount;


        CoInitialize(0);

        hr = CoCreateInstance(
            CLSID_MMDeviceEnumerator, NULL,
            CLSCTX_ALL, IID_IMMDeviceEnumerator,
            (void**)&pEnumerator);
        
        hr = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator,eRender, eConsole, &pDevice);
        hr = pDevice->lpVtbl->Activate(pDevice,IID_IAudioClient, CLSCTX_ALL,NULL, (void**)&pAudioClient);
        hr = pAudioClient->lpVtbl->GetMixFormat(pAudioClient ,&pwfx);
        hr = pAudioClient->lpVtbl->Initialize(pAudioClient,AUDCLNT_SHAREMODE_SHARED,0,
            hnsRequestedDuration,
            0,
            pwfx,
            NULL);
        // Get the actual size of the allocated buffer.
        hr = pAudioClient->lpVtbl->GetBufferSize(pAudioClient ,&bufferFrameCount);
        hr = pAudioClient->lpVtbl->GetService(pAudioClient,IID_IAudioRenderClient,(void**)&pRenderClient);
        //至此拿到pRenderClient接口了，这里需要去hook该接口中ReleaseBuffer函数

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        True_ReleaseBuffer = pRenderClient->lpVtbl->ReleaseBuffer;
        DetourAttach(&(PVOID&)True_ReleaseBuffer, MyReleaseBuffer);
        DetourTransactionCommit();



        CoUninitialize();



    }
    else if (dwReason == DLL_PROCESS_DETACH) {

    }
    return TRUE;
}
//
///////////////////////////////////////////////////////////////// End of File.
