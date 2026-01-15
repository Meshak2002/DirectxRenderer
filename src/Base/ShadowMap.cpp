#include "ShadowMap.h"

ShadowMap::ShadowMap(ID3D12Device* aDevice, UINT aWidth, UINT aHeight)
{
	Width = aWidth;
	Height = aHeight;
	Device = aDevice;
	BuildResource();
}

D3D12_VIEWPORT ShadowMap::GetViewport()
{
	return { 0.0f, 0.0f, (float)Width, (float)Height, 0.0f, 1.0f };
}

RECT ShadowMap::GetRect()
{
	return { 0, 0, (int)Width, (int)Height };
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::GetDsvHeapCpuHandle()
{
	return DSV;
}

ID3D12Resource* ShadowMap::GetResourcePtr()
{
	return DepthBufferResource.Get();
}

void ShadowMap::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE aSrvCpuHandle, CD3DX12_CPU_DESCRIPTOR_HANDLE aDsvCpuHandle)
{
	// Store the handles for later use
	SRV = aSrvCpuHandle;
	DSV = aDsvCpuHandle;

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SrvDesc.Texture2D.MipLevels = 1;
	SrvDesc.Texture2D.MostDetailedMip = 0;
	SrvDesc.Texture2D.ResourceMinLODClamp = 0;
	SrvDesc.Texture2D.PlaneSlice = 0;
	Device->CreateShaderResourceView(DepthBufferResource.Get(), &SrvDesc, aSrvCpuHandle);

	D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
	DsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	DsvDesc.Texture2D.MipSlice = 0;
	Device->CreateDepthStencilView(DepthBufferResource.Get(), &DsvDesc, aDsvCpuHandle);
}

void ShadowMap::BuildResource()
{
	D3D12_RESOURCE_DESC DsvResDesc;
	ZeroMemory(&DsvResDesc, sizeof(D3D12_RESOURCE_DESC));
	DsvResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DsvResDesc.Alignment = 0;
	DsvResDesc.Width     = Width;
	DsvResDesc.Height    = Height;
	DsvResDesc.DepthOrArraySize = 1;
	DsvResDesc.MipLevels = 1;
	DsvResDesc.Format    = DXGI_FORMAT_R24G8_TYPELESS;
	DsvResDesc.SampleDesc= {1,0};
	DsvResDesc.Layout    = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	DsvResDesc.Flags     = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE DsvClearVal;
	DsvClearVal.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;		//@TODO
	DsvClearVal.DepthStencil = {1 , 0};

	auto HeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	Device->CreateCommittedResource(&HeapProperty, D3D12_HEAP_FLAG_NONE, &DsvResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, &DsvClearVal, IID_PPV_ARGS(&DepthBufferResource));
}

