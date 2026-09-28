
/**
 * Blocky — prova DirectX 11 ("Hello Triangle"). Stesso spirito della Fase 1
 * Android e del demo Vulkan (vk_main.cpp): dimostra che il toolchain
 * funziona davvero (finestra + device D3D11 + uno shader compilato +
 * un triangolo disegnato), NON e' il motore vero — nessun codice condiviso
 * con main.cpp/libblocky.dll, nessun mondo di gioco.
 *
 * Finestra creata con GLFW (GLFW_NO_API: nessun contesto OpenGL, solo la
 * finestra nativa) esattamente come vk_main.cpp fa per Vulkan — stesso
 * pattern, stessa libreria gia' nel progetto, invece di scrivere a mano
 * la window procedure Win32.
 */

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <d3d11.h>
#include <d3dcompiler.h>

#include <cmath>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

static const char* kShaderSrc = R"(
struct VSInput { float3 pos : POSITION; float3 color : COLOR; };
struct PSInput { float4 pos : SV_POSITION; float3 color : COLOR; };

PSInput VSMain(VSInput input) {
    PSInput o;
    o.pos = float4(input.pos, 1.0);
    o.color = input.color;
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return float4(input.color, 1.0);
}
)";

struct Vertex { float x, y, z; float r, g, b; };

static void fatal(const char* msg) {
    std::fprintf(stderr, "Errore: %s\n", msg);
    std::exit(1);
}

int main() {
    if (!glfwInit()) fatal("glfwInit fallita");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // niente contesto OpenGL: disegna D3D11
    GLFWwindow* win = glfwCreateWindow(960, 600, "Blocky — prova DirectX 11", nullptr, nullptr);
    if (!win) fatal("glfwCreateWindow fallita");
    HWND hwnd = glfwGetWin32Window(win);

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &scd, &swapChain, &device, &featureLevel, &ctx);
    if (FAILED(hr)) fatal("D3D11CreateDeviceAndSwapChain fallita (GPU/driver senza DirectX 11?)");

    std::printf("Device D3D11 creato — feature level 0x%x\n", featureLevel);

    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    ID3D11RenderTargetView* rtv = nullptr;
    device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    backBuffer->Release();

    ID3DBlob* vsBlob = nullptr; ID3DBlob* psBlob = nullptr; ID3DBlob* errBlob = nullptr;
    if (FAILED(D3DCompile(kShaderSrc, strlen(kShaderSrc), nullptr, nullptr, nullptr,
                           "VSMain", "vs_4_0", 0, 0, &vsBlob, &errBlob))) {
        if (errBlob) std::fprintf(stderr, "VS: %s\n", (char*)errBlob->GetBufferPointer());
        fatal("Compilazione vertex shader fallita");
    }
    if (FAILED(D3DCompile(kShaderSrc, strlen(kShaderSrc), nullptr, nullptr, nullptr,
                           "PSMain", "ps_4_0", 0, 0, &psBlob, &errBlob))) {
        if (errBlob) std::fprintf(stderr, "PS: %s\n", (char*)errBlob->GetBufferPointer());
        fatal("Compilazione pixel shader fallita");
    }

    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps);

    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    ID3D11InputLayout* layout = nullptr;
    device->CreateInputLayout(layoutDesc, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &layout);
    vsBlob->Release(); psBlob->Release();

    Vertex verts[] = {
        {  0.0f,  0.6f, 0.f,  0.85f, 0.35f, 0.95f },
        {  0.6f, -0.5f, 0.f,  0.20f, 0.55f, 0.95f },
        { -0.6f, -0.5f, 0.f,  0.30f, 0.85f, 0.65f },
    };
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(verts);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initData = { verts, 0, 0 };
    ID3D11Buffer* vbo = nullptr;
    device->CreateBuffer(&bd, &initData, &vbo);

    double t0 = glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        float t = (float)(glfwGetTime() - t0);
        float col[4] = { 0.06f, 0.09f, 0.16f, 1.f }; // stesso blu scuro del tema Blocky
        (void)t;

        D3D11_VIEWPORT vp = { 0, 0, 960, 600, 0, 1 };
        ctx->RSSetViewports(1, &vp);
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->ClearRenderTargetView(rtv, col);

        UINT stride = sizeof(Vertex), offset = 0;
        ctx->IASetInputLayout(layout);
        ctx->IASetVertexBuffers(0, 1, &vbo, &stride, &offset);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(ps, nullptr, 0);
        ctx->Draw(3, 0);

        swapChain->Present(1, 0);
    }

    vbo->Release(); layout->Release(); vs->Release(); ps->Release();
    rtv->Release(); swapChain->Release(); ctx->Release(); device->Release();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
