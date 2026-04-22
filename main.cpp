#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>   

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

static const wchar_t* WINDOW_TITLE = L"Hello Triangle — DirectX 11";
static const int      WINDOW_WIDTH = 800;
static const int      WINDOW_HEIGHT = 600;
static const wchar_t* SHADER_FILE = L"shaders.hlsl";

struct Vertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
};

static HWND                     g_hWnd = nullptr;
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_pRenderTarget = nullptr;
static ID3D11Buffer* g_pVertexBuffer = nullptr;
static ID3D11VertexShader* g_pVertexShader = nullptr;
static ID3D11PixelShader* g_pPixelShader = nullptr;
static ID3D11InputLayout* g_pInputLayout = nullptr;

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
bool InitWindow(HINSTANCE hInstance);
bool InitD3D();
bool InitShaders();
bool InitGeometry();
void Render();
void Cleanup();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    if (!InitWindow(hInstance)) { MessageBoxW(nullptr, L"Window creation failed.", L"Error", MB_OK); return -1; }
    if (!InitD3D()) { MessageBoxW(nullptr, L"D3D init failed.", L"Error", MB_OK); return -1; }
    if (!InitShaders()) {
        MessageBoxW(nullptr, L"Shader compile failed.\n"
            L"Make sure shaders.hlsl is\n"
            L"in the same folder as the exe.", L"Error", MB_OK); return -1;
    }
    if (!InitGeometry()) { MessageBoxW(nullptr, L"Geometry init failed.", L"Error", MB_OK); return -1; }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

  
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            Render();  
        }
    }

    Cleanup();
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostQuitMessage(0);  
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

bool InitWindow(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"DX11WindowClass";

    if (!RegisterClassEx(&wc)) return false;

  
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    int wndW = rect.right - rect.left;
    int wndH = rect.bottom - rect.top;

    g_hWnd = CreateWindowEx(
        0, L"DX11WindowClass", WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        (screenW - wndW) / 2, (screenH - wndH) / 2,
        wndW, wndH,
        nullptr, nullptr, hInstance, nullptr
    );

    return g_hWnd != nullptr;
}

bool InitD3D()
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = WINDOW_WIDTH;
    scd.BufferDesc.Height = WINDOW_HEIGHT;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = g_hWnd;
    scd.SampleDesc.Count = 1;   
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;   
#endif

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                    
        D3D_DRIVER_TYPE_HARDWARE,   
        nullptr, flags,
        nullptr, 0,                
        D3D11_SDK_VERSION,
        &scd, &g_pSwapChain,
        &g_pDevice, &featureLevel, &g_pContext
    );
    if (FAILED(hr)) return false;

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    g_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTarget);
    pBackBuffer->Release();

    g_pContext->OMSetRenderTargets(1, &g_pRenderTarget, nullptr);

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(WINDOW_WIDTH);
    vp.Height = static_cast<float>(WINDOW_HEIGHT);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    g_pContext->RSSetViewports(1, &vp);

    return true;
}

bool InitShaders()
{
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pPSBlob = nullptr;
    ID3DBlob* pErrBlob = nullptr;

    HRESULT hr = D3DCompileFromFile(
        SHADER_FILE, nullptr, nullptr,
        "VSMain", "vs_5_0",          
        compileFlags, 0,
        &pVSBlob, &pErrBlob
    );
    if (FAILED(hr)) {
        if (pErrBlob) {
            OutputDebugStringA(static_cast<char*>(pErrBlob->GetBufferPointer()));
            pErrBlob->Release();
        }
        return false;
    }

    hr = D3DCompileFromFile(
        SHADER_FILE, nullptr, nullptr,
        "PSMain", "ps_5_0",
        compileFlags, 0,
        &pPSBlob, &pErrBlob
    );
    if (FAILED(hr)) {
        if (pErrBlob) {
            OutputDebugStringA(static_cast<char*>(pErrBlob->GetBufferPointer()));
            pErrBlob->Release();
        }
        pVSBlob->Release();
        return false;
    }

    g_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    g_pDevice->CreateInputLayout(
        layout, ARRAYSIZE(layout),
        pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(),
        &g_pInputLayout
    );

    pVSBlob->Release();
    pPSBlob->Release();
    return true;
}

bool InitGeometry()
{
   

    Vertex vertices[] = {
        
        { XMFLOAT3(0.0f,  0.8f, 0.0f), XMFLOAT4(1.0f, 0.2f, 0.2f, 1.0f) },  
        { XMFLOAT3(0.8f, -0.8f, 0.0f), XMFLOAT4(0.2f, 1.0f, 0.2f, 1.0f) },  
        { XMFLOAT3(-0.8f, -0.8f, 0.0f), XMFLOAT4(0.2f, 0.2f, 1.0f, 1.0f) },  
    };

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(vertices);
    bd.Usage = D3D11_USAGE_DEFAULT;   
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;   

    HRESULT hr = g_pDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);
    return SUCCEEDED(hr);
}

void Render()
{
    float clearColor[4] = { 0.05f, 0.05f, 0.10f, 1.0f };  
    g_pContext->ClearRenderTargetView(g_pRenderTarget, clearColor);

    g_pContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pContext->PSSetShader(g_pPixelShader, nullptr, 0);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pContext->IASetInputLayout(g_pInputLayout);
    g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_pContext->Draw(3, 0);
    g_pSwapChain->Present(1, 0);  
}

void Cleanup()
{
    if (g_pContext) g_pContext->ClearState();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pRenderTarget) g_pRenderTarget->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pContext) g_pContext->Release();
    if (g_pDevice) g_pDevice->Release();
}