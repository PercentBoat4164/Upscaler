#ifdef ENABLE_DX11
#    include "DX11.hpp"

#    include <d3d11.h>

#    include <IUnityGraphicsD3D11.h>

ID3D11DeviceContext* DX11::_oneTimeSubmitContext{nullptr};
IUnityGraphicsD3D11* DX11::graphicsInterface{nullptr};

void DX11::createOneTimeSubmitContext() {
    graphicsInterface->GetDevice()->GetImmediateContext(&_oneTimeSubmitContext);
}

ID3D11DeviceContext* DX11::getOneTimeSubmitContext() {
    return _oneTimeSubmitContext;
}

void DX11::destroyOneTimeSubmitContext() {
    _oneTimeSubmitContext->Release();
}

bool DX11::registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces) {
    graphicsInterface = t_unityInterfaces->Get<IUnityGraphicsD3D11>();
    return true;
}

IUnityGraphicsD3D11* DX11::getGraphicsInterface() {
    return graphicsInterface;
}

bool DX11::unregisterUnityInterfaces() {
    graphicsInterface = nullptr;
    return true;
}
#endif