#pragma once
#include "../cache/game.h"
#include <Windows.h>
#include <d3d11.h>

class Overlay {
public:
    bool create();
    void run(Game& game);
    void destroy();

private:
    bool frame(Game& game);
    void draw(const Game& game, float width, float height);
    void sync_to_game();

    HWND hwnd_ = nullptr;
    WNDCLASSEXW wc_{};
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    IDXGISwapChain* swap_ = nullptr;
    ID3D11RenderTargetView* target_ = nullptr;
    int width_ = 1920;
    int height_ = 1080;

    bool setup_device();
    void release_device();
    void resize();
};
