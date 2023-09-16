#pragma once

#ifdef ENABLE_DX11

#    include "GraphicsAPI.hpp"

// Graphics API
#    include <d3d11.h>

// Unity
#    include "IUnityGraphicsD3D11.h"

class DX11 : public GraphicsAPI {
private:
    ID3D11DeviceContext *_oneTimeSubmitContext{nullptr};
    bool                 _oneTimeSubmitRecording{false};

    DX11() = default;

    IUnityGraphicsD3D11 *DX11Interface;
    ID3D11Device        *device{};

public:
    DX11(const DX11 &)            = delete;
    DX11(DX11 &&)                 = default;
    DX11 &operator=(const DX11 &) = delete;
    DX11 &operator=(DX11 &&)      = default;

    static DX11 *get();

    Type                 getType() override;
    void                 prepareForOneTimeSubmits() override;
    ID3D11DeviceContext *beginOneTimeSubmitRecording();
    void                 endOneTimeSubmitRecording();
    void                 cancelOneTimeSubmitRecording();
    void                 finishOneTimeSubmits() override;
    bool                 useUnityInterfaces(IUnityInterfaces *t_unityInterfaces) override;
    IUnityGraphicsD3D11 *getUnityInterface();

    ~DX11() override = default;
};

#endif