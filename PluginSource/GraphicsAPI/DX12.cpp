#include "DX12.hpp"

// Upscaler
#include <Upscaler/Upscaler.hpp>

// Graphics API
#include <d3d12.h>
#include <d3d12compatibility.h>

// Unity
#include <IUnityGraphicsD3D12.h>

DX12 *DX12::get() {
    static DX12 *dx12{new DX12};
    return dx12;
}

GraphicsAPI::Type DX12::getType() {
    return GraphicsAPI::DX12;
}

void DX12::prepareForOneTimeSubmits() {
    device = DX12Interface->GetDevice();
    if (FAILED(device->CreateCommandAllocator(
          D3D12_COMMAND_LIST_TYPE_DIRECT,
          IID_PPV_ARGS(&_oneTimeSubmitCommandAllocator)
        )))
        return (void) Upscaler::get()->setStatus(
          Upscaler::SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
          "Failed to create the DX12 Command Allocator."
        );
    if (FAILED(device->CreateCommandList(
          0,
          D3D12_COMMAND_LIST_TYPE_DIRECT,
          _oneTimeSubmitCommandAllocator,
          nullptr,
          IID_PPV_ARGS(&_oneTimeSubmitCommandList)
        )))
        return (void) Upscaler::get()->setStatus(
          Upscaler::SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
          "Failed to create the DX12 Command List."
        );
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_oneTimeSubmitFence))))
        return (void) Upscaler::get()->setStatus(
          Upscaler::SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
          "Failed to create the DX12 Fence."
        );
    _oneTimeSubmitEvent = CreateEventEx(nullptr, "", 0, EVENT_ALL_ACCESS);
}

ID3D12GraphicsCommandList *DX12::beginOneTimeSubmitRecording() {
    _oneTimeSubmitRecording = true;
    return _oneTimeSubmitCommandList;
}

void DX12::endOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;

    if (FAILED(_oneTimeSubmitCommandList->Close()))
        return (void) Upscaler::get()->setStatus(
          Upscaler::SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
          "Failed to close the DX12 Command List."
        );

    const uint64_t value = DX12Interface->ExecuteCommandList(_oneTimeSubmitCommandList, 0, nullptr);

    if (FAILED(DX12Interface->GetCommandQueue()->Signal(_oneTimeSubmitFence, value)))
        return (void) Upscaler::get()->setStatus(
          Upscaler::SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
          "Failed to signal the DX12 Fence."
        );
    if (FAILED(_oneTimeSubmitFence->SetEventOnCompletion(value, _oneTimeSubmitEvent)))
        return (void) Upscaler::get()->setStatus(
          Upscaler::SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
          "Failed to set event for the DX12 Fence."
        );
    WaitForSingleObject(_oneTimeSubmitEvent, INFINITE);
    if (FAILED(_oneTimeSubmitFence->Signal(0)))
        return (void) Upscaler::get()->setStatus(
          Upscaler::SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
          "Failed to signal the DX12 Fence."
        );

    _oneTimeSubmitRecording = false;
}

void DX12::cancelOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;
    if (FAILED(_oneTimeSubmitCommandList->Reset(_oneTimeSubmitCommandAllocator, nullptr)))
        return (void) Upscaler::get()->setStatus(
          Upscaler::SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
          "Failed to reset the DX12 Command List."
        );
    _oneTimeSubmitRecording = false;
}

void DX12::finishOneTimeSubmits() {
    cancelOneTimeSubmitRecording();
    CloseHandle(_oneTimeSubmitEvent);
    if (FAILED(_oneTimeSubmitFence->Release()))
        return (void) Upscaler::get()->setStatus(
          Upscaler::SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
          "Failed to release the DX12 Fence."
        );
    if (FAILED(_oneTimeSubmitCommandList->Release()))
        return (void) Upscaler::get()->setStatus(
          Upscaler::SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
          "Failed to release the DX12 Command List."
        );
    if (FAILED(_oneTimeSubmitCommandAllocator->Release()))
        return (void) Upscaler::get()->setStatus(
          Upscaler::SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
          "Failed to release the DX12 Command Allocator."
        );
}

bool DX12::useUnityInterfaces(IUnityInterfaces *t_unityInterfaces) {
    DX12Interface = t_unityInterfaces->Get<IUnityGraphicsD3D12v7>();
    return true;
}

IUnityGraphicsD3D12v7 *DX12::getUnityInterface() const {
    return DX12Interface;
}
