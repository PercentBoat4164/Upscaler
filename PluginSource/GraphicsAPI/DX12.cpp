#include "DX12.hpp"

// Graphics API
#include <d3d12.h>
#include <d3d12compatibility.h>

// Unity
#include <IUnityGraphicsD3D12.h>

GraphicsAPI::Type DX12::getType() {
    return GraphicsAPI::DX12;
}

void DX12::prepareForOneTimeSubmits() {
    device = DX12Interface->GetDevice();
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_oneTimeSubmitCommandAllocator));
    // clang-format off
    device->CreateCommandList(
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      _oneTimeSubmitCommandAllocator,
      nullptr,
      IID_PPV_ARGS(&_oneTimeSubmitCommandList)
    );
    // clang-format on
}

ID3D12GraphicsCommandList *DX12::beginOneTimeSubmitRecording() {
    _oneTimeSubmitRecording = true;
    return _oneTimeSubmitCommandList;
}

void DX12::endOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;
    _oneTimeSubmitCommandList->Close();
    ID3D12Fence *_oneTimeSubmitFence{};
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_oneTimeSubmitFence));
    HANDLE event = CreateEventEx(nullptr, "", 0, EVENT_ALL_ACCESS);

    uint64_t value = DX12Interface->ExecuteCommandList(_oneTimeSubmitCommandList, 0, nullptr);
    DX12Interface->GetCommandQueue()->Signal(_oneTimeSubmitFence, value);
    _oneTimeSubmitFence->SetEventOnCompletion(value, event);
    WaitForSingleObject(event, INFINITE);

    CloseHandle(event);
    _oneTimeSubmitFence->Release();
    _oneTimeSubmitRecording = false;
}

void DX12::cancelOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;
    _oneTimeSubmitCommandList->Reset(_oneTimeSubmitCommandAllocator, nullptr);
    _oneTimeSubmitRecording = false;
}

void DX12::finishOneTimeSubmits() {
    cancelOneTimeSubmitRecording();
    _oneTimeSubmitCommandAllocator->Release();
    _oneTimeSubmitCommandAllocator->Release();
}

DX12 *DX12::get() {
    static DX12 *dx12{new DX12};
    return dx12;
}

bool DX12::useUnityInterfaces(IUnityInterfaces *t_unityInterfaces) {
    DX12Interface = t_unityInterfaces->Get<IUnityGraphicsD3D12v7>();
    return true;
}

IUnityGraphicsD3D12v7 *DX12::getUnityInterface() {
    return DX12Interface;
}
