#pragma once
#ifdef ENABLE_DX11
#    include "GraphicsAPI.hpp"

class ID3D11DeviceContext;

struct IUnityGraphicsD3D11;

class DX11 final : public GraphicsAPI {
    static ID3D11DeviceContext* oneTimeSubmitContext;
    static IUnityGraphicsD3D11* graphicsInterface;

public:
    DX11()                       = delete;
    DX11(const DX11&)            = delete;
    DX11(DX11&&)                 = delete;
    DX11& operator=(const DX11&) = delete;
    DX11& operator=(DX11&&)      = delete;
    ~DX11()                      = delete;

    static bool                 registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces);
    static IUnityGraphicsD3D11* getGraphicsInterface();
    static bool                 unregisterUnityInterfaces();

    static void                 createOneTimeSubmitContext();
    static ID3D11DeviceContext* getOneTimeSubmitContext();
    static void                 destroyOneTimeSubmitContext();
};
#endif