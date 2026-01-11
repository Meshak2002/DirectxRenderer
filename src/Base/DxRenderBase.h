#pragma once
#include "../Utility/d3dUtil.h"
#include "GameTime.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

class DxRenderBase
{
public:
    DxRenderBase(HINSTANCE WindowsInstance);
    DxRenderBase(const DxRenderBase& RenderBase) = delete;
    DxRenderBase& operator=(const DxRenderBase& RenderBase) = delete;
	static DxRenderBase* Get();
	static DxRenderBase* DxInstance;

    virtual ~DxRenderBase();

    int Run();
	virtual bool Initialize();

	virtual LRESULT MsgProc(HWND Hwnd, UINT Msg, WPARAM wParam, LPARAM lParam);

protected:
	HWND MainWindowHandle = nullptr;
	UINT ScreenWidth = 800;
	UINT ScreenHeight = 600;
	UINT CurrentBackBuffer = 0;
	std::wstring WindowTitle = L"DirectX Renderer";
    
	Microsoft::WRL::ComPtr<IDXGIFactory4> DxgiFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> DxDevice3D;
	Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandList;
	Microsoft::WRL::ComPtr<ID3D12Fence> Fence;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DsvHeap;
	
	UINT64 CurrentFenceValue = 0;
	RECT ScissorRect;
	D3D12_VIEWPORT Viewport;
	HINSTANCE WindowInstance;

	UINT RtvDescriptorSize = 0;
	UINT DsvDescriptorSize = 0;
	UINT CbvSrvUavDescriptorSize = 0;

	DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	virtual void OnResize();
	virtual void Update(const GameTime& Gt);
	virtual void Draw(const GameTime& Gt);

	virtual void OnKeyboardDown(WPARAM Key) { };
	virtual void OnKeyboardUp(WPARAM Key) { };
	virtual void OnMouseDown(WPARAM BtnState, int X, int Y) {};
	virtual void OnMouseUp(WPARAM BtnState, int X, int Y) {};
	virtual void OnMouseMove(WPARAM BtnState, int X, int Y) {};

	// Essential getters
	float AspectRatio() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferHeapDescHandle() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilHeapDescHandle() const;
	ID3D12Resource* CurrentBackBufferResource() const;
	ID3D12Resource* DepthStencilResource() const;
	void FlushCommandQueue();

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> SwapChainBuffer[2];
	Microsoft::WRL::ComPtr<ID3D12Resource> DepthBuffer;

	UINT MsaaQuality = 0;


	GameTime GameTimer;

	bool AppPaused = false;
	bool Minimized = false;
	bool Maximized = false;
	bool Resizing = false;

	bool InitMainWindow();
	bool InitDirect3D();
	
	void CreateSwapChain();
	void CreateCommandObjects();
	void CreateRtvDsvHeap();
};