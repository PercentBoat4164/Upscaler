#ifdef ENABLE_DX12
#    include "DX12.hpp"

// These headers are required even though CLion reports them as unused.
#    include <d3d12.h>
#    include <d3d12compatibility.h>

#    include <IUnityGraphicsD3D12.h>

IUnityGraphicsD3D12v7* DX12::graphicsInterface{nullptr};

ID3D12CommandAllocator* DX12::commandAllocator{nullptr};

bool DX12::registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces) {
    graphicsInterface = t_unityInterfaces->Get<IUnityGraphicsD3D12v7>();
    return true;
}

IUnityGraphicsD3D12v7* DX12::getGraphicsInterface() {
    return graphicsInterface;
}

bool DX12::unregisterUnityInterfaces() {
    graphicsInterface = nullptr;
    return true;
}

bool DX12::initializeOneTimeSubmits() {
    return SUCCEEDED(graphicsInterface->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
}

ID3D12GraphicsCommandList* DX12::getOneTimeSubmitCommandList() {
    ID3D12GraphicsCommandList* list{nullptr};
    (void) graphicsInterface->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&list));
    return list;
}

bool DX12::executeOneTimeSubmitCommandList(ID3D12GraphicsCommandList* commandList) {
    if (FAILED(commandList->Close())) return false;
    ID3D12Fence* fence{nullptr};
    if (FAILED(graphicsInterface->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return false;
    graphicsInterface->GetCommandQueue()->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList**>(&commandList));
    if (FAILED(graphicsInterface->GetCommandQueue()->Signal(fence, 1))) return false;
    while (fence->GetCompletedValue() != 1) {}
    return SUCCEEDED(commandList->Release());
}

bool DX12::shutdownOneTimeSubmits() {
    return SUCCEEDED(commandAllocator->Release());
}
#endif