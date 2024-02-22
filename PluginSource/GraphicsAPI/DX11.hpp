#pragma once
#ifdef ENABLE_DX11
#    include "GraphicsAPI.hpp"

class ID3D11DeviceContext;

struct IUnityGraphicsD3D11;

class DX11 final : public GraphicsAPI {
    static ID3D11DeviceContext* _oneTimeSubmitContext;
    static IUnityGraphicsD3D11* graphicsInterface;

public:
    DX11() = delete;
    ~DX11() = delete;

    static bool                 registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces);
    static IUnityGraphicsD3D11* getGraphicsInterface();
    static bool                 unregisterUnityInterfaces();

    static void                 createOneTimeSubmitContext();
    static ID3D11DeviceContext* getOneTimeSubmitContext();
    static void                 destroyOneTimeSubmitContext();
};
#endif