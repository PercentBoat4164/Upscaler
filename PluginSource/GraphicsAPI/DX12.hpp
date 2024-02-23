#pragma once
#ifdef ENABLE_DX12
#    include "GraphicsAPI.hpp"

struct IUnityGraphicsD3D12v7;

class DX12 final : public GraphicsAPI {
    static IUnityGraphicsD3D12v7* graphicsInterface;

public:
    DX12()  = delete;
    ~DX12() = delete;

    static bool                   registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces);
    static IUnityGraphicsD3D12v7* getGraphicsInterface();
    static bool                   unregisterUnityInterfaces();
};
#endif