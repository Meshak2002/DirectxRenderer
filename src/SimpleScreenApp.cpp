#include "SimpleScreenApp.h"


SimpleScreenApp::SimpleScreenApp(HINSTANCE ScreenInstance) : DxRenderBase(ScreenInstance)
{

}

SimpleScreenApp::~SimpleScreenApp()
{
}

void SimpleScreenApp::Draw(const GameTime& Gt)
{
	ThrowIfFailed(CommandAlloc->Reset());
	ThrowIfFailed(CommandList->Reset(CommandAlloc.Get(), nullptr));

	auto Barier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBufferResource(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	CommandList->ResourceBarrier(1, &Barier);

	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	CommandList->ClearRenderTargetView(CurrentBackBufferHeapDescHandle(), DirectX::Colors::BurlyWood, 0, nullptr);
	CommandList->ClearDepthStencilView(DepthStencilHeapDescHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1, 0, 0, nullptr);

	auto Rtv = CurrentBackBufferHeapDescHandle();
	auto Dsv = DepthStencilHeapDescHandle();
	CommandList->OMSetRenderTargets(1, &Rtv, true, &Dsv);

	auto Barier2 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBufferResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	CommandList->ResourceBarrier(1, &Barier2);

	CommandList->Close();
	ID3D12CommandList* Commands[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(Commands), Commands);

	ThrowIfFailed(SwapChain->Present(0, 0));
	CurrentBackBuffer = (CurrentBackBuffer + 1) % 2;


	FlushCommandQueue();
}
