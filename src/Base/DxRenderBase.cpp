#include "DxRenderBase.h"
#include "../Utility/d3dx12.h"
#include "windowsx.h"

DxRenderBase::DxRenderBase(HINSTANCE Instance)
: WindowInstance(Instance)
{
	assert(DxInstance == nullptr);
	DxInstance = this;
}

LRESULT CALLBACK
MainWndProc(HWND Hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return DxRenderBase::Get()->MsgProc(Hwnd, Msg, wParam, lParam);
}

DxRenderBase* DxRenderBase::DxInstance = nullptr;

DxRenderBase* DxRenderBase::Get()
{
	return DxInstance; 
}

DxRenderBase::~DxRenderBase()
{
	if (DxDevice3D)
		FlushCommandQueue();
}

int DxRenderBase::Run()
{
	MSG msg = {0};
	GameTimer.Reset();
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			GameTimer.Tick();
			if (!AppPaused)
			{
				Update(GameTimer);
				Draw(GameTimer);
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

bool DxRenderBase::Initialize()
{
	if (InitMainWindow())
	{
		if (InitDirect3D())
		{
			OnResize();
			return true;
		}
	}

	return false;
}

void DxRenderBase::OnResize()
{
	assert(DxDevice3D);
	assert(SwapChain);
	assert(CommandAlloc);

	if (ScreenWidth == 0 || ScreenHeight == 0)
		return;

	FlushCommandQueue();
	CommandList->Reset(CommandAlloc.Get(), nullptr);
	for (int i = 0; i < 2; i++)
		SwapChainBuffer[i].Reset();
	DepthBuffer.Reset();

	ThrowIfFailed(SwapChain->ResizeBuffers(2, ScreenWidth, ScreenHeight, BackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
	CurrentBackBuffer = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle = RtvHeap->GetCPUDescriptorHandleForHeapStart();
	for(int i=0; i<2; i++)
	{
		ThrowIfFailed(SwapChain->GetBuffer(i, IID_PPV_ARGS(&SwapChainBuffer[i])));
		RtvHandle.ptr += i * RtvDescriptorSize;
		DxDevice3D->CreateRenderTargetView(SwapChainBuffer[i].Get(), nullptr, RtvHandle);
	}

	D3D12_RESOURCE_DESC DepthStencilDesc;
	DepthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DepthStencilDesc.Alignment = 0;
	DepthStencilDesc.Width = ScreenWidth;
	DepthStencilDesc.Height = ScreenHeight;
	DepthStencilDesc.DepthOrArraySize = 1;
	DepthStencilDesc.MipLevels = 1;
	DepthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	DepthStencilDesc.SampleDesc.Count = 1;
	DepthStencilDesc.SampleDesc.Quality = 0;
	DepthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	DepthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE OptClear;
	OptClear.Format = DepthStencilFormat;
	OptClear.DepthStencil.Depth = 1.0f;
	OptClear.DepthStencil.Stencil = 0;
	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(DxDevice3D->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
		&DepthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&OptClear,
		IID_PPV_ARGS(DepthBuffer.GetAddressOf())
	));
	D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc;
	DsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DsvDesc.Format = DepthStencilFormat;
	DsvDesc.Texture2D.MipSlice = 0;
	DxDevice3D->CreateDepthStencilView(DepthBuffer.Get(), &DsvDesc, DsvHeap->GetCPUDescriptorHandleForHeapStart());
	auto depthBarrier = CD3DX12_RESOURCE_BARRIER::Transition(DepthBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	CommandList->ResourceBarrier(1, &depthBarrier);

	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* CmdLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(CmdLists), CmdLists);
	FlushCommandQueue();

	Viewport = { 0.0f, 0.0f, (float)ScreenWidth, (float)ScreenHeight, 0.0f, 1.0f };	
	ScissorRect = { 0, 0, (int)ScreenWidth, (int)ScreenHeight };	
}


void DxRenderBase::Update(const GameTime& Gt)
{

}

void DxRenderBase::Draw(const GameTime& Gt)
{
}

LRESULT DxRenderBase::MsgProc(HWND Hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg) {
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			GameTimer.Pause();
			AppPaused = true;
		}
		else
		{
			GameTimer.Resume();
			AppPaused = false;
		}
		return 0;

	case WM_SIZE:
		ScreenWidth = LOWORD(lParam);
		ScreenHeight = HIWORD(lParam);
		if (DxDevice3D)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				GameTimer.Pause();
				AppPaused = true;
				Minimized = true;
				Maximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				GameTimer.Resume();
				AppPaused = false;
				Minimized = false;
				Maximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{
				if (Minimized)
				{
					GameTimer.Resume();
					AppPaused = false;
					Minimized = false;
					OnResize();
				}
				else if (Maximized)
				{
					GameTimer.Resume();
					AppPaused = false;
					Maximized = false;
					OnResize();
				}
				else if (!Resizing)
					OnResize();
			}
		}
		return 0;

	case WM_ENTERSIZEMOVE:
		AppPaused = true;
		Resizing = true;
		return 0;

	case WM_EXITSIZEMOVE:
		AppPaused = false;
		Resizing = false;
		OnResize();
		return 0;

	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_KEYDOWN:
		OnKeyboardDown(wParam);
		return 0;

	case WM_KEYUP:
		OnKeyboardUp(wParam);
		return 0;
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(Hwnd, Msg, wParam, lParam);
}

bool DxRenderBase::InitMainWindow()
{
	WNDCLASS WindowClass;
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = MainWndProc;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = WindowInstance;
	WindowClass.hIcon = LoadIcon(0, IDI_APPLICATION);
	WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
	WindowClass.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	WindowClass.lpszMenuName = 0;
	WindowClass.lpszClassName = L"DxRenderBaseWindowClass";
	RegisterClass(&WindowClass);

	RECT R = { 0, 0, (LONG)ScreenWidth, (LONG)ScreenHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	UINT Width = R.right - R.left;
	UINT Height = R.bottom - R.top;

	MainWindowHandle = CreateWindow(WindowClass.lpszClassName, WindowTitle.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
						Width, Height, 0, 0, WindowInstance, 0);

	if(!MainWindowHandle)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}
	ShowWindow(MainWindowHandle, SW_SHOW);
	UpdateWindow(MainWindowHandle);
	return true;
}

bool DxRenderBase::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG)
	Microsoft::WRL::ComPtr<ID3D12Debug> DebugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController)));
	DebugController->EnableDebugLayer();
#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&DxgiFactory)));
	auto DeviceCreationResult = D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&DxDevice3D));
	if (FAILED(DeviceCreationResult))
	{
		Microsoft::WRL::ComPtr<IDXGIAdapter> GpuSoftAdapter;
		ThrowIfFailed(DxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&GpuSoftAdapter)) );
        ThrowIfFailed(D3D12CreateDevice(GpuSoftAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&DxDevice3D)));
	}

	ThrowIfFailed(DxDevice3D->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));
	CbvSrvUavDescriptorSize = DxDevice3D->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	RtvDescriptorSize = DxDevice3D->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	DsvDescriptorSize = DxDevice3D->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	 
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS MultisampleQualityLevels;
	MultisampleQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	MultisampleQualityLevels.Format = BackBufferFormat;
	MultisampleQualityLevels.SampleCount = 4;
	MultisampleQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(DxDevice3D->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &MultisampleQualityLevels, sizeof(MultisampleQualityLevels)));
	MsaaQuality = MultisampleQualityLevels.NumQualityLevels;
	assert(MsaaQuality > 0 && "Improper Msaa quality");

	CreateCommandObjects();
	CreateSwapChain();
	CreateRtvDsvHeap();
	return true;
}

void DxRenderBase::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC CommandQueDesc = {};
	CommandQueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	CommandQueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(DxDevice3D->CreateCommandQueue(&CommandQueDesc, IID_PPV_ARGS(&CommandQueue)));
	ThrowIfFailed(DxDevice3D->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&CommandAlloc) ));
	ThrowIfFailed(DxDevice3D->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAlloc.Get(), nullptr, IID_PPV_ARGS(&CommandList)));
	CommandList->Close();
}

void DxRenderBase::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC SwapChainDesc;
	SwapChainDesc.BufferDesc.Width = ScreenWidth;
	SwapChainDesc.BufferDesc.Height = ScreenHeight;
	SwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	SwapChainDesc.BufferDesc.Format = BackBufferFormat;
	SwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	SwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.SampleDesc.Quality = 0; 
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = 2;
	SwapChainDesc.OutputWindow = MainWindowHandle;
	SwapChainDesc.Windowed = true;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(DxgiFactory->CreateSwapChain(CommandQueue.Get(), &SwapChainDesc, SwapChain.GetAddressOf() )); 
}

void DxRenderBase::CreateRtvDsvHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC RtvHeapDesc;
	RtvHeapDesc.NumDescriptors = 2;
	RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	RtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(DxDevice3D->CreateDescriptorHeap(&RtvHeapDesc, IID_PPV_ARGS(&RtvHeap)));
	D3D12_DESCRIPTOR_HEAP_DESC DsvHeapDesc;
	DsvHeapDesc.NumDescriptors = 2;  // Main depth buffer + shadow map
	DsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	DsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(DxDevice3D->CreateDescriptorHeap(&DsvHeapDesc, IID_PPV_ARGS(&DsvHeap)));
}

void DxRenderBase::FlushCommandQueue()
{
	CurrentFenceValue++;
	ThrowIfFailed( CommandQueue->Signal(Fence.Get(), CurrentFenceValue) );
	if (Fence->GetCompletedValue() < CurrentFenceValue)
	{
		HANDLE EventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

		ThrowIfFailed(Fence->SetEventOnCompletion(CurrentFenceValue, EventHandle));
		WaitForSingleObject(EventHandle, INFINITE);
		CloseHandle(EventHandle);
	}
}

float DxRenderBase::AspectRatio() const
{
	return ScreenHeight > 0 ? static_cast<float>(ScreenWidth) / ScreenHeight : 1.0f;
}

D3D12_CPU_DESCRIPTOR_HANDLE DxRenderBase::CurrentBackBufferHeapDescHandle() const
{
	D3D12_CPU_DESCRIPTOR_HANDLE Handle = RtvHeap->GetCPUDescriptorHandleForHeapStart();
	Handle.ptr += CurrentBackBuffer * RtvDescriptorSize;
	return Handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DxRenderBase::GetDsvHeapCpuHandle() const
{
	return DsvHeap->GetCPUDescriptorHandleForHeapStart();
}

ID3D12Resource* DxRenderBase::CurrentBackBufferResource() const
{
	return SwapChainBuffer[CurrentBackBuffer].Get();
}

ID3D12Resource* DxRenderBase::DepthStencilResource() const
{
	return DepthBuffer.Get();
}

