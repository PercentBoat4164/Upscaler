#pragma once
#ifdef ENABLE_DX12
#    include <d3d12.h>
#    include "GraphicsAPI.hpp"


struct IUnityGraphicsD3D12v7;

class DX12 final : public GraphicsAPI {
    static IUnityGraphicsD3D12v7* graphicsInterface;

    static ID3D12CommandAllocator* commandAllocator;

public:
    DX12()                       = delete;
    DX12(const DX12&)            = delete;
    DX12(DX12&&)                 = delete;
    DX12& operator=(const DX12&) = delete;
    DX12& operator=(DX12&&)      = delete;
    ~DX12()                      = delete;

    static bool                   registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces);
    static IUnityGraphicsD3D12v7* getGraphicsInterface();
    static bool                   unregisterUnityInterfaces();

    [[nodiscard]] static bool initializeOneTimeSubmits();
    [[nodiscard]] static ID3D12GraphicsCommandList* getOneTimeSubmitCommandList();
    [[nodiscard]] static bool executeOneTimeSubmitCommandList(ID3D12GraphicsCommandList* commandList);
    [[nodiscard]] static bool shutdownOneTimeSubmits();
};
#endif