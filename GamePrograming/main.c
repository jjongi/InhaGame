#include <windows.h>
#include <stdio.h>
#include <d3d11.h>
#include <d3dcompiler.h>

// 콘솔 디버깅 및 DX11 라이브러리 링크
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib") // IID_ID3D11Texture2D 사용을 위해 필요

// --- [1. 구조체 정의] ---
typedef struct {
    float x, y, z;
    float r, g, b, a;
} Vertex;

// 게임 상태를 담는 바구니 (GameContext)
typedef struct {
    float posX;
    float posY;
    int isRunning;
} GameContext;

// --- [2. 전역 변수] ---
GameContext g_Ctx = { 0.0f, 0.0f, 1 }; // 초기 위치는 중앙(0,0), 루프 실행 상태(1)

ID3D11Device* g_pd3dDevice = NULL;
ID3D11DeviceContext* g_pImmediateContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_pRenderTargetView = NULL;
ID3D11VertexShader* g_pVertexShader = NULL;
ID3D11PixelShader* g_pPixelShader = NULL;
ID3D11InputLayout* g_pInputLayout = NULL;
ID3D11Buffer* g_pVertexBuffer = NULL;

// --- [3. HLSL 셰이더 소스 (C언어 스타일 문자열)] ---
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
void Render();

// =========================================================
// 프로그램 진입점 (WinMain)
// =========================================================
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. 윈도우 등록 및 생성
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"DX11GameLoopClass";
    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(L"DX11GameLoopClass", L"과제: GameLoop 육망성 (Pure C)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    printf("=== GameLoop 엔진 시작 ===\n방향키를 눌러 육망성을 이동하세요!\n");

    // 2. DX11 초기화 
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

    // 3. 셰이더 컴파일
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

    // --- [4. GameLoop (PeekMessage 방식)] ---
    MSG msg = { 0 };
    while (g_Ctx.isRunning) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_Ctx.isRunning = 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // 입력(Update)은 WndProc에서 처리되었으므로, 변한 데이터를 기반으로 그리기(Render)
            Render();
            Sleep(10); // CPU 과열 방지용
        }
    }

    // 자원 해제
    if (g_pVertexBuffer) g_pVertexBuffer->lpVtbl->Release(g_pVertexBuffer);
    if (g_pInputLayout) g_pInputLayout->lpVtbl->Release(g_pInputLayout);
    if (g_pVertexShader) g_pVertexShader->lpVtbl->Release(g_pVertexShader);
    if (g_pPixelShader) g_pPixelShader->lpVtbl->Release(g_pPixelShader);
    if (g_pRenderTargetView) g_pRenderTargetView->lpVtbl->Release(g_pRenderTargetView);
    if (g_pSwapChain) g_pSwapChain->lpVtbl->Release(g_pSwapChain);
    if (g_pImmediateContext) g_pImmediateContext->lpVtbl->Release(g_pImmediateContext);
    if (g_pd3dDevice) g_pd3dDevice->lpVtbl->Release(g_pd3dDevice);

    return (int)msg.wParam;
}

// =========================================================
// 입력 처리 (WndProc)
// =========================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_KEYDOWN:
        // 방향키에 따른 실시간 위치 업데이트 (DeltaTime 미사용 고정 수치)
        if (wParam == VK_LEFT)  g_Ctx.posX -= 0.05f;
        if (wParam == VK_RIGHT) g_Ctx.posX += 0.05f;
        if (wParam == VK_UP)    g_Ctx.posY += 0.05f;
        if (wParam == VK_DOWN)  g_Ctx.posY -= 0.05f;

        if (wParam == 'Q') PostQuitMessage(0);
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
// 출력 (Render)
// =========================================================
void Render() {
    // 1. 이전 프레임의 버퍼 릴리즈
    if (g_pVertexBuffer) {
        g_pVertexBuffer->lpVtbl->Release(g_pVertexBuffer);
        g_pVertexBuffer = NULL;
    }

    float cx = g_Ctx.posX;
    float cy = g_Ctx.posY;
    float s = 0.3f; // 별의 크기
    float ratio = 600.0f / 800.0f; // 가로축 축소 비율 (0.75)

    // 가로로 퍼지는 부분(X축)에만 ratio를 곱해준다!
    Vertex vertices[] = {
        // 정삼각형 (△)
        { cx, cy + s, 0.5f,                                  1.0f, 0.0f, 0.0f, 1.0f },
        { cx + (s * 0.866f * ratio), cy - s * 0.5f, 0.5f,      0.0f, 1.0f, 0.0f, 1.0f },
        { cx - (s * 0.866f * ratio), cy - s * 0.5f, 0.5f,      0.0f, 0.0f, 1.0f, 1.0f },

        // 역삼각형 (▽)
        { cx, cy - s, 0.5f,                                  1.0f, 0.0f, 0.0f, 1.0f },
        { cx - (s * 0.866f * ratio), cy + s * 0.5f, 0.5f,      0.0f, 1.0f, 0.0f, 1.0f },
        { cx + (s * 0.866f * ratio), cy + s * 0.5f, 0.5f,      0.0f, 0.0f, 1.0f, 1.0f }
    };

    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->lpVtbl->CreateBuffer(g_pd3dDevice, &bd, &initData, &g_pVertexBuffer);

    // 3. 화면 지우기
    float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
    g_pImmediateContext->lpVtbl->ClearRenderTargetView(g_pImmediateContext, g_pRenderTargetView, clearColor);

    // 4. 파이프라인 세팅 및 그리기
    UINT stride = sizeof(Vertex), offset = 0;
    g_pImmediateContext->lpVtbl->IASetInputLayout(g_pImmediateContext, g_pInputLayout);
    g_pImmediateContext->lpVtbl->IASetVertexBuffers(g_pImmediateContext, 0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pImmediateContext->lpVtbl->IASetPrimitiveTopology(g_pImmediateContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_pImmediateContext->lpVtbl->VSSetShader(g_pImmediateContext, g_pVertexShader, NULL, 0);
    g_pImmediateContext->lpVtbl->PSSetShader(g_pImmediateContext, g_pPixelShader, NULL, 0);

    g_pImmediateContext->lpVtbl->Draw(g_pImmediateContext, 6, 0);

    // 5. 버퍼 교체(스왑)
    g_pSwapChain->lpVtbl->Present(g_pSwapChain, 0, 0);
}