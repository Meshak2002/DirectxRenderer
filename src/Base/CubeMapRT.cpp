#include "CubeMapRT.h"


CubeMapRT::CubeMapRT(ID3D12Device* aDxDevice, UINT aWidth, UINT aHeight, DXGI_FORMAT aRtFormat, DXGI_FORMAT aDsFormat)
{
	DxDevice = aDxDevice;
	Width = aWidth;
	Height = aHeight;
	RtFormat = aRtFormat;
	DsFormat = aDsFormat;

	BuildResource();
}

void CubeMapRT::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE aSrvCpuHandle, CD3DX12_CPU_DESCRIPTOR_HANDLE aRtvCpuHandle[6],
	CD3DX12_CPU_DESCRIPTOR_HANDLE aDsvCpuHandle)
{
	SrvCpuHandle = aSrvCpuHandle;
	std::copy(aRtvCpuHandle, aRtvCpuHandle + 6, std::begin(RtvCpuHandle));
	DsvCpuHandle = aDsvCpuHandle;

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc;
	SrvDesc.Format	= RtFormat;
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SrvDesc.TextureCube.MipLevels = 1;
	SrvDesc.TextureCube.MostDetailedMip = 0;
	SrvDesc.TextureCube.ResourceMinLODClamp = 0;
	DxDevice->CreateShaderResourceView(RtResource.Get(), &SrvDesc, SrvCpuHandle);

	//RT for each cube faces
	for (int i = 0; i < 6; i++)
	{
		D3D12_RENDER_TARGET_VIEW_DESC RtDesc;
		RtDesc.Format = RtFormat;
		RtDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		RtDesc.Texture2DArray.ArraySize = 1;	//Only View one element of the array
		RtDesc.Texture2DArray.FirstArraySlice = i;
		RtDesc.Texture2DArray.MipSlice = 0;
		RtDesc.Texture2DArray.PlaneSlice = 0;

		DxDevice->CreateRenderTargetView(RtResource.Get(), &RtDesc, RtvCpuHandle[i]);
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc;
	DsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	DsvDesc.Texture2D.MipSlice = 0;
	DxDevice->CreateDepthStencilView(DepthResource.Get(), &DsvDesc, DsvCpuHandle);
}

ID3D12Resource* CubeMapRT::GetRtResourcePtr()
{
	return RtResource.Get();
}

ID3D12Resource* CubeMapRT::GetDsResourcePtr()
{
	return DepthResource.Get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeMapRT::GetSrvCpuHandle() const
{
	return SrvCpuHandle;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeMapRT::GetDsvCpuHandle() const
{
	return DsvCpuHandle;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeMapRT::GetRtvCpuHandle(size_t Index)
{
	return RtvCpuHandle[Index];
}

D3D12_VIEWPORT CubeMapRT::GetViewport() const
{
	return { 0.0f, 0.0f, (float)Width, (float)Height, 0.0f, 1.0f };
}

RECT CubeMapRT::GetRect() const
{
	return { 0, 0, (int)Width, (int)Height };
}

void CubeMapRT::BuildResource()
{
	D3D12_RESOURCE_DESC RtvResDesc;
	ZeroMemory(&RtvResDesc, sizeof(D3D12_RESOURCE_DESC));
	RtvResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	RtvResDesc.Alignment = 0;
	RtvResDesc.Width = Width;
	RtvResDesc.Height = Height;
	RtvResDesc.DepthOrArraySize = 6;
	RtvResDesc.MipLevels = 1;
	RtvResDesc.Format = RtFormat;
	RtvResDesc.SampleDesc = { 1,0 };
	RtvResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	RtvResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	auto HeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_CLEAR_VALUE RtvClearValue;
	RtvClearValue.Format = RtFormat;
	RtvClearValue.Color[0] = 0.0f;  // Black - matches DirectX::Colors::Black in DrawSceneToCubeMap
	RtvClearValue.Color[1] = 0.0f;
	RtvClearValue.Color[2] = 0.0f;
	RtvClearValue.Color[3] = 1.0f;

	ThrowIfFailed(DxDevice->CreateCommittedResource(&HeapProperty, D3D12_HEAP_FLAG_NONE, &RtvResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, &RtvClearValue, IID_PPV_ARGS(&RtResource)) );
	
	D3D12_RESOURCE_DESC DsvResDesc = RtvResDesc;
	DsvResDesc.DepthOrArraySize = 1;
	DsvResDesc.Format = DsFormat;
	DsvResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE DsvClearDesc;
	DsvClearDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DsvClearDesc.DepthStencil = { 1,0 };

	ThrowIfFailed(DxDevice->CreateCommittedResource(&HeapProperty, D3D12_HEAP_FLAG_NONE, &DsvResDesc,
		D3D12_RESOURCE_STATE_COMMON, &DsvClearDesc, IID_PPV_ARGS(&DepthResource)) );
}