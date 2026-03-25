#include <windows.h>
#include <stdio.h>
#include <d3d11.h>
#include <d3dcompiler.h>

/* main.c에서 가져온 하위 시스템 및 콘솔 디버깅 설정 */
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- [구조체 정의 (C언어 스타일)] ---
typedef struct {
    float x, y, z;
    float r, g, b, a;
} Vertex;

// 게임의 모든 상태를 담는 바구니
typedef struct {
    float posX;
    float posY;
    BOOL isRunning;
} GameContext;

// --- [전역 변수] ---
GameContext g_Game = { 0.0f, 0.0f, TRUE };

// DirectX 11 전역 객체
ID3D11Device* g_pd3dDevice = NULL;
ID3D11DeviceContext* g_pImmediateContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_pRenderTargetView = NULL;
ID3D11VertexShader* g_pVertexShader = NULL;
ID3D11PixelShader* g_pPixelShader = NULL;
ID3D11InputLayout* g_pInputLayout = NULL;
ID3D11Buffer* g_pVertexBuffer = NULL; // 매 프레임 재생성할 버퍼

// --- [HLSL 셰이더 코드 (C언어 문자열 스타일)] ---
const char* shaderSource =
"struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };\n"
"struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };\n"
"PS_INPUT VS(VS_INPUT input) {\n"
"    PS_INPUT output;\n"
"    output.pos = float4(input.pos, 1.0f);\n"
"    output.col = input.col;\n"
"    return output;\n"
"}\n"
"float4 PS(PS_INPUT input) : SV_Target {\n"
"    return input.col;\n"
"}\n";

// --- [함수 선언] ---
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL InitDirectX(HWND hWnd);
void InitShaders();
void Update(GameContext* ctx);
void Render(GameContext* ctx);
void CleanupDevice();

// =========================================================
// WinMain: 프로그램의 입구
// =========================================================
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = L"IntegratedGameLoopC";
    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(L"IntegratedGameLoopC", L"육망성 게임 엔진 (Pure C) - Jongin", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);

    InitDirectX(hWnd);
    InitShaders();
    ShowWindow(hWnd, nCmdShow);

    printf("=== Win32 & DX11 GameLoop (C버전) 시작 ===\n");
    printf("방향키나 W, A, S, D로 별을 움직여보세요! (Q: 종료)\n\n");

    MSG msg = { 0 };
    while (g_Game.isRunning) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_Game.isRunning = FALSE;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            Update(&g_Game);
            Render(&g_Game);
            Sleep(10);
        }
    }

    CleanupDevice();
    printf("\n게임이 안전하게 종료되었습니다.\n");
    return (int)msg.wParam;
}

// =========================================================
// WndProc: 입력 수신부
// =========================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_KEYDOWN:
        printf("[EVENT] Key Pressed: Virtual Key: %lld\n", wParam);

        if (wParam == 'Q') {
            printf("  >> Q 입력 감지, 프로그램 종료 요청!\n");
            PostQuitMessage(0);
        }
        if (wParam == VK_LEFT || wParam == 'A')  g_Game.posX -= 0.02f;
        if (wParam == VK_RIGHT || wParam == 'D') g_Game.posX += 0.02f;
        if (wParam == VK_UP || wParam == 'W')    g_Game.posY += 0.02f;
        if (wParam == VK_DOWN || wParam == 'S')  g_Game.posY -= 0.02f;
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// =========================================================
// Update: 로직 계산
// =========================================================
void Update(GameContext* ctx) {
    // 추가적인 물리 엔진 로직이나 AI 업데이트가 들어가는 곳
}

// =========================================================
// Render: 렌더링 (C언어 lpVtbl 적용)
// =========================================================
void Render(GameContext* ctx) {
    // 1. 기존 버퍼 해제
    if (g_pVertexBuffer) {
        g_pVertexBuffer->lpVtbl->Release(g_pVertexBuffer);
        g_pVertexBuffer = NULL;
    }

    float s = 0.2f;
    float cx = ctx->posX;
    float cy = ctx->posY;

    Vertex vertices[] = {
        { cx, cy + s, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f },
        { cx + s * 0.86f, cy - s * 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f },
        { cx - s * 0.86f, cy - s * 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f },
        { cx, cy - s, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f },
        { cx - s * 0.86f, cy + s * 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f },
        { cx + s * 0.86f, cy + s * 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f }
    };

    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA sd = { vertices, 0, 0 };
    g_pd3dDevice->lpVtbl->CreateBuffer(g_pd3dDevice, &bd, &sd, &g_pVertexBuffer);

    // 2. 화면 지우기
    float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
    g_pImmediateContext->lpVtbl->ClearRenderTargetView(g_pImmediateContext, g_pRenderTargetView, clearColor);

    // 3. 파이프라인 연결 및 그리기
    UINT stride = sizeof(Vertex), offset = 0;
    g_pImmediateContext->lpVtbl->IASetInputLayout(g_pImmediateContext, g_pInputLayout);
    g_pImmediateContext->lpVtbl->IASetVertexBuffers(g_pImmediateContext, 0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pImmediateContext->lpVtbl->IASetPrimitiveTopology(g_pImmediateContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_pImmediateContext->lpVtbl->VSSetShader(g_pImmediateContext, g_pVertexShader, NULL, 0);
    g_pImmediateContext->lpVtbl->PSSetShader(g_pImmediateContext, g_pPixelShader, NULL, 0);

    g_pImmediateContext->lpVtbl->Draw(g_pImmediateContext, 6, 0);
    g_pSwapChain->lpVtbl->Present(g_pSwapChain, 0, 0);
}

// =========================================================
// 보조 함수: DX11 초기화 / 해제
// =========================================================
BOOL InitDirectX(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = { 0 };
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, NULL, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = NULL;
    g_pSwapChain->lpVtbl->GetBuffer(g_pSwapChain, 0, &IID_ID3D11Texture2D, (void**)&pBackBuffer);
    g_pd3dDevice->lpVtbl->CreateRenderTargetView(g_pd3dDevice, (ID3D11Resource*)pBackBuffer, NULL, &g_pRenderTargetView);
    pBackBuffer->lpVtbl->Release(pBackBuffer);

    g_pImmediateContext->lpVtbl->OMSetRenderTargets(g_pImmediateContext, 1, &g_pRenderTargetView, NULL);
    D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0.0f, 1.0f };
    g_pImmediateContext->lpVtbl->RSSetViewports(g_pImmediateContext, 1, &vp);
    return TRUE;
}

void InitShaders() {
    ID3DBlob* vsBlob = NULL, * psBlob = NULL;
    D3DCompile(shaderSource, lstrlenA(shaderSource), NULL, NULL, NULL, "VS", "vs_4_0", 0, 0, &vsBlob, NULL);
    D3DCompile(shaderSource, lstrlenA(shaderSource), NULL, NULL, NULL, "PS", "ps_4_0", 0, 0, &psBlob, NULL);

    g_pd3dDevice->lpVtbl->CreateVertexShader(g_pd3dDevice, vsBlob->lpVtbl->GetBufferPointer(vsBlob), vsBlob->lpVtbl->GetBufferSize(vsBlob), NULL, &g_pVertexShader);
    g_pd3dDevice->lpVtbl->CreatePixelShader(g_pd3dDevice, psBlob->lpVtbl->GetBufferPointer(psBlob), psBlob->lpVtbl->GetBufferSize(psBlob), NULL, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pd3dDevice->lpVtbl->CreateInputLayout(g_pd3dDevice, layout, 2, vsBlob->lpVtbl->GetBufferPointer(vsBlob), vsBlob->lpVtbl->GetBufferSize(vsBlob), &g_pInputLayout);
    vsBlob->lpVtbl->Release(vsBlob);
    psBlob->lpVtbl->Release(psBlob);
}

void CleanupDevice() {
    if (g_pVertexBuffer) g_pVertexBuffer->lpVtbl->Release(g_pVertexBuffer);
    if (g_pInputLayout) g_pInputLayout->lpVtbl->Release(g_pInputLayout);
    if (g_pVertexShader) g_pVertexShader->lpVtbl->Release(g_pVertexShader);
    if (g_pPixelShader) g_pPixelShader->lpVtbl->Release(g_pPixelShader);
    if (g_pRenderTargetView) g_pRenderTargetView->lpVtbl->Release(g_pRenderTargetView);
    if (g_pSwapChain) g_pSwapChain->lpVtbl->Release(g_pSwapChain);
    if (g_pImmediateContext) g_pImmediateContext->lpVtbl->Release(g_pImmediateContext);
    if (g_pd3dDevice) g_pd3dDevice->lpVtbl->Release(g_pd3dDevice);
}