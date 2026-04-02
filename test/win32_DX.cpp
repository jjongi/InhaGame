#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <chrono>
#include <vector>
#include <string>
#include <thread>

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- [전역 DX11 객체] ---
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.col = input.col;
    return output;
}
float4 PS(PS_INPUT input) : SV_Target {
    return input.col;
}
)";

// =========================================================
// [1단계: 컴포넌트 기저 클래스 
// =========================================================
class Component
{
public:
    class GameObject* pOwner = nullptr; // 이 기능이 누구의 것인지 저장
    bool isStarted = false;             // Start()가 실행되었는지 체크

    virtual void Start() = 0;              // 초기화
    virtual void Input() {}                // 입력 (선택사항)
    virtual void Update(float dt) = 0;     // 로직 (필수)
    virtual void Render() {}               // 그리기 (선택사항)

    virtual ~Component() {}
};

// =========================================================
// [2단계: 게임 오브젝트 클래스]
// =========================================================
class GameObject {
public:
    std::string name;
    float posX = 0.0f;
    float posY = 0.0f;
    std::vector<Component*> components;

    GameObject(std::string n) { name = n; }

    ~GameObject() {
        for (int i = 0; i < (int)components.size(); i++) {
            delete components[i];
        }
    }

    void AddComponent(Component* pComp) {
        pComp->pOwner = this;
        pComp->isStarted = false;
        components.push_back(pComp);
    }
};

// =========================================================
// [3단계: 구체적인 기능 컴포넌트들 구현]
// =========================================================

// 1. 플레이어 조종 컴포넌트
class PlayerControl : public Component {
public:
    int playerID;
    float speed;
    bool moveUp, moveDown, moveLeft, moveRight;

    PlayerControl(int id) : playerID(id) {}

    void Start() override {
        speed = 1.0f;
        moveUp = moveDown = moveLeft = moveRight = false;
        printf("[%s] PlayerControl Start!\n", pOwner->name.c_str());
    }

    void Input() override {
        if (playerID == 1) { // Player 1 (방향키)
            moveLeft = (GetAsyncKeyState(VK_LEFT) & 0x8000);
            moveRight = (GetAsyncKeyState(VK_RIGHT) & 0x8000);
            moveUp = (GetAsyncKeyState(VK_UP) & 0x8000);
            moveDown = (GetAsyncKeyState(VK_DOWN) & 0x8000);
        }
        else if (playerID == 2) { // Player 2 (WASD)
            moveLeft = (GetAsyncKeyState('A') & 0x8000);
            moveRight = (GetAsyncKeyState('D') & 0x8000);
            moveUp = (GetAsyncKeyState('W') & 0x8000);
            moveDown = (GetAsyncKeyState('S') & 0x8000);
        }
    }

    void Update(float dt) override {
        if (moveLeft)  pOwner->posX -= speed * dt;
        if (moveRight) pOwner->posX += speed * dt;
        if (moveUp)    pOwner->posY += speed * dt;
        if (moveDown)  pOwner->posY -= speed * dt;
    }
};

// 2. DX11 렌더링(그리기) 컴포넌트
class Renderer : public Component {
public:
    ID3D11Buffer* vertexBuffer = nullptr;
    float r, g, b;

    Renderer(float r, float g, float b) : r(r), g(g), b(b) {}

    ~Renderer() {
        if (vertexBuffer) vertexBuffer->Release();
    }

    void Start() override {
        // 컴포넌트가 시작될 때 자신만의 버퍼를 동적(DYNAMIC)으로 생성
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(Vertex) * 3; // 삼각형 1개 (정점 3개)
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        g_pd3dDevice->CreateBuffer(&bd, nullptr, &vertexBuffer);

        printf("[%s] Renderer Start!\n", pOwner->name.c_str());
    }

    void Update(float dt) override {} // 렌더러는 업데이트 로직이 없음

    void Render() override {
        float cx = pOwner->posX;
        float cy = pOwner->posY;
        float s = 0.15f;

        // 정점 계산 (부모의 위치 참조)
        Vertex vertices[3] = {
            { cx, cy + s, 0.5f, r, g, b, 1.0f },
            { cx + s * 0.866f, cy - s * 0.5f, 0.5f, r, g, b, 1.0f },
            { cx - s * 0.866f, cy - s * 0.5f, 0.5f, r, g, b, 1.0f }
        };

        // 버퍼에 덮어쓰기 (Map/Unmap)
        D3D11_MAPPED_SUBRESOURCE ms;
        g_pImmediateContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        memcpy(ms.pData, vertices, sizeof(vertices));
        g_pImmediateContext->Unmap(vertexBuffer, 0);

        // 파이프라인에 세팅 후 그리기
        UINT stride = sizeof(Vertex), offset = 0;
        g_pImmediateContext->IASetInputLayout(g_pInputLayout);
        g_pImmediateContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
        g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

        g_pImmediateContext->Draw(3, 0);
    }
};

// =========================================================
// [4단계: 엔진 코어 (GameLoop)]
// =========================================================
class GameLoop {
public:
    bool isRunning;
    std::vector<GameObject*> gameWorld;
    std::chrono::high_resolution_clock::time_point prevTime;
    float deltaTime;

    void Initialize() {
        isRunning = true;
        deltaTime = 0.0f;
        prevTime = std::chrono::high_resolution_clock::now();
    }

    void Input() {
        // [시스템 입력 처리] ESC 종료 및 F 풀스크린 토글
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) isRunning = false;

        static bool isFPressed = false;
        if (GetAsyncKeyState('F') & 0x8000) {
            if (!isFPressed) {
                BOOL isFullScreen = FALSE;
                g_pSwapChain->GetFullscreenState(&isFullScreen, nullptr);
                g_pSwapChain->SetFullscreenState(!isFullScreen, nullptr);
                isFPressed = true;
            }
        }
        else {
            isFPressed = false;
        }

        // 각 컴포넌트의 Input 호출
        for (int i = 0; i < (int)gameWorld.size(); i++) {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {
                gameWorld[i]->components[j]->Input();
            }
        }
    }

    void Update() {
        // [Start 호출 보장 로직]
        for (int i = 0; i < (int)gameWorld.size(); i++) {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {
                if (gameWorld[i]->components[j]->isStarted == false) {
                    gameWorld[i]->components[j]->Start();
                    gameWorld[i]->components[j]->isStarted = true;
                }
            }
        }

        // [Update 호출]
        for (int i = 0; i < (int)gameWorld.size(); i++) {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {
                gameWorld[i]->components[j]->Update(deltaTime);
            }
        }
    }

    void Render() {
        // DX11 화면 초기화
        float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
        g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

        // 각 컴포넌트 그리기
        for (int i = 0; i < (int)gameWorld.size(); i++) {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {
                gameWorld[i]->components[j]->Render();
            }
        }

        g_pSwapChain->Present(1, 0); // VSync ON
    }

    void Run() {
        MSG msg = { 0 };
        while (isRunning) {
            // [Win32 메시지 처리 (PeekMassge)]
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) isRunning = false;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else {
                std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
                std::chrono::duration<float> elapsed = currentTime - prevTime;
                deltaTime = elapsed.count();
                prevTime = currentTime;

                Input();
                Update();
                Render();

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    ~GameLoop() {
        for (int i = 0; i < (int)gameWorld.size(); i++) {
            delete gameWorld[i];
        }
    }
};

// =========================================================
// [시스템 초기화 (WinMain)]
// =========================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

void InitDX11(HWND hWnd) {
    //DX11 디바이스 및 스왑 체인 초기화
    DXGI_SWAP_CHAIN_DESC sd = { 0 };
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    // GPU와 통신할 통로(Device)와 화면(SwapChain)을 생성함.
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    // 렌더 타겟 설정 (도화지 준비)
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
    D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0.0f, 1.0f };
    g_pImmediateContext->RSSetViewports(1, &vp);

    // 3. 셰이더 컴파일 및 생성
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vShader = nullptr;
    ID3D11PixelShader* pShader = nullptr;
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);
    vsBlob->Release(); psBlob->Release();
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = L"GameEngineClass";
    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(L"GameEngineClass", L"컴포넌트 패턴 엔진 (800x600)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);

    InitDX11(hWnd);
    ShowWindow(hWnd, nCmdShow);

    // --- [게임 루프 및 객체 초기화] ---
    GameLoop gLoop;
    gLoop.Initialize();

    // Player 1 (노란색 삼각형, 방향키)
    GameObject* player1 = new GameObject("Player1");
    player1->posX = 0.3f;
    player1->AddComponent(new PlayerControl(1));
    player1->AddComponent(new Renderer(1.0f, 1.0f, 0.0f));
    gLoop.gameWorld.push_back(player1);

    // Player 2 (파란색 삼각형, WASD), 컴포넌트 추가
    GameObject* player2 = new GameObject("Player2");
    player2->posX = -0.3f;
    player2->AddComponent(new PlayerControl(2));
    player2->AddComponent(new Renderer(0.0f, 0.5f, 1.0f));
    gLoop.gameWorld.push_back(player2);

    // 엔진 시작!
    gLoop.Run();

    // --- [메모리 해제] ---
    g_pSwapChain->SetFullscreenState(FALSE, nullptr);
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return 0;
}