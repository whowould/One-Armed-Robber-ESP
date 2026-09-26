#include "overlay.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <dwmapi.h>
#include <cmath>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool Overlay::create() {
    wc_ = { sizeof(wc_), CS_CLASSDC, wnd_proc, 0, 0, GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr, L"GuardESP", nullptr };
    RegisterClassExW(&wc_);
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc_.lpszClassName, L"GuardESP", WS_POPUP,
        0, 0, width_, height_, nullptr, nullptr, wc_.hInstance, nullptr);
    if (!hwnd_)
        return false;

    SetLayeredWindowAttributes(hwnd_, RGB(0, 0, 0), 0, LWA_COLORKEY);
    if (!setup_device())
        return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_, context_);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    return true;
}

void Overlay::destroy() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    release_device();
    if (hwnd_)
        DestroyWindow(hwnd_);
    UnregisterClassW(wc_.lpszClassName, wc_.hInstance);
}

bool Overlay::setup_device() {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd_;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 1,
            D3D11_SDK_VERSION, &desc, &swap_, &device_, &level, &context_)))
        return false;

    ID3D11Texture2D* back = nullptr;
    swap_->GetBuffer(0, IID_PPV_ARGS(&back));
    device_->CreateRenderTargetView(back, nullptr, &target_);
    back->Release();
    return target_ != nullptr;
}

void Overlay::release_device() {
    if (target_) { target_->Release(); target_ = nullptr; }
    if (swap_) { swap_->Release(); swap_ = nullptr; }
    if (context_) { context_->Release(); context_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
}

void Overlay::resize() {
    if (!swap_ || width_ < 1 || height_ < 1)
        return;
    if (target_) { target_->Release(); target_ = nullptr; }
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    swap_->ResizeBuffers(0, width_, height_, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* back = nullptr;
    swap_->GetBuffer(0, IID_PPV_ARGS(&back));
    device_->CreateRenderTargetView(back, nullptr, &target_);
    back->Release();
}

void Overlay::sync_to_game() {
    HWND game = FindWindowW(nullptr, L"One-armed robber  ");
    if (!game)
        game = FindWindowW(nullptr, L"One-armed robber");
    if (!game)
        return;
    RECT client{};
    if (!GetClientRect(game, &client))
        return;
    POINT origin{ 0, 0 };
    ClientToScreen(game, &origin);
    const int w = client.right - client.left;
    const int h = client.bottom - client.top;
    if (w < 640 || h < 480)
        return;
    if (w != width_ || h != height_) {
        width_ = w;
        height_ = h;
        resize();
    }
    SetWindowPos(hwnd_, HWND_TOPMOST, origin.x, origin.y, w, h, SWP_NOACTIVATE);
}

static bool project(const Camera& cam, const Vec3& pos, float width, float height, float& sx, float& sy) {
    const float dx = pos.x - cam.origin.x;
    const float dy = pos.y - cam.origin.y;
    const float dz = pos.z - cam.origin.z;
    const float yaw = cam.yaw * 0.0174532925f;
    const float pitch = cam.pitch * 0.0174532925f;
    const float cos_y = std::cos(yaw);
    const float sin_y = std::sin(yaw);
    const float cos_p = std::cos(pitch);
    const float sin_p = std::sin(pitch);
    const float x1 = cos_y * dx + sin_y * dy;
    const float y1 = -sin_y * dx + cos_y * dy;
    const float forward = cos_p * x1 + sin_p * dz;
    const float up = -sin_p * x1 + cos_p * dz;
    if (forward < 1.f)
        return false;
    const float focal = (width * 0.5f) / std::tan(cam.fov * 0.0174532925f * 0.5f);
    sx = width * 0.5f + (y1 / forward) * focal;
    sy = height * 0.5f - (up / forward) * focal;
    return sx > -200.f && sy > -200.f && sx < width + 200.f && sy < height + 200.f;
}

void Overlay::draw(const Game& game, float width, float height) {
    auto* dl = ImGui::GetBackgroundDrawList();
    const Camera* cam = game.camera();
    int drawn = 0;
    if (cam) {
        for (const auto& pawn : game.pawns()) {
            const float half = pawn.kind == ActorKind::Camera ? 22.f : pawn.half_height;
            const Vec3 top_w{ pawn.pos.x, pawn.pos.y, pawn.pos.z + half };
            const Vec3 bot_w{ pawn.pos.x, pawn.pos.y, pawn.pos.z - half };
            float tx, ty, bx, by;
            if (!project(*cam, top_w, width, height, tx, ty))
                continue;
            if (!project(*cam, bot_w, width, height, bx, by))
                continue;
            const float top = std::round(std::fmin(ty, by));
            const float bottom = std::round(std::fmax(ty, by));
            const float tall = bottom - top;
            const float wide = std::round(std::fmax(tall * 0.45f, 4.f));
            const float cx = std::round((tx + bx) * 0.5f);
            const float x0 = std::round(cx - wide * 0.5f);
            const float x1 = x0 + wide;
            if (wide < 4.f || tall < 8.f)
                continue;
            if (wide > width * 0.85f && tall > height * 0.85f)
                continue;
            const auto color = pawn.kind == ActorKind::Camera ? IM_COL32(90, 190, 255, 255) : IM_COL32(255, 70, 70, 255);
            const ImU32 outline = IM_COL32(1, 1, 1, 255);
            auto stroke = [&](float a, float b, float c, float d, ImU32 col) {
                dl->AddRectFilled(ImVec2(a, b), ImVec2(c, b + 1.f), col);
                dl->AddRectFilled(ImVec2(a, d - 1.f), ImVec2(c, d), col);
                dl->AddRectFilled(ImVec2(a, b), ImVec2(a + 1.f, d), col);
                dl->AddRectFilled(ImVec2(c - 1.f, b), ImVec2(c, d), col);
            };
            stroke(x0 - 1.f, top - 1.f, x1 + 1.f, bottom + 1.f, outline);
            stroke(x0, top, x1, bottom, color);
            stroke(x0 + 1.f, top + 1.f, x1 - 1.f, bottom - 1.f, outline);
            const float dx = pawn.pos.x - cam->origin.x;
            const float dy = pawn.pos.y - cam->origin.y;
            const float dz = pawn.pos.z - cam->origin.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            char label[32];
            std::snprintf(label, sizeof(label), pawn.kind == ActorKind::Camera ? "cam %.0fm" : "%.0fm", dist / 100.f);
            dl->AddText(ImVec2(x0, top - 14.f), color, label);
            ++drawn;
        }
    }
    char status[64];
    std::snprintf(status, sizeof(status), cam ? "drawn %d" : "no camera", drawn);
    dl->AddText(ImVec2(24.f, 24.f), IM_COL32(160, 255, 160, 255), status);
}

bool Overlay::frame(Game& game) {
    sync_to_game();
    game.refresh();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    draw(game, static_cast<float>(width_), static_cast<float>(height_));
    ImGui::Render();

    const float clear[4] = { 0.f, 0.f, 0.f, 1.f };
    context_->OMSetRenderTargets(1, &target_, nullptr);
    context_->ClearRenderTargetView(target_, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    swap_->Present(0, 0);
    return true;
}

void Overlay::run(Game& game) {
    MSG msg{};
    while (msg.message != WM_QUIT) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                return;
        }
        frame(game);
    }
}
