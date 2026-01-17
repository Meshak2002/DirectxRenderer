#pragma once
#include "../Utility/d3dUtil.h"

class CubeMapRT
{
public:
	CubeMapRT() = delete;
	CubeMapRT(ID3D12Device* aDxDevice, UINT aWidth, UINT aHeight, DXGI_FORMAT aRtFormat, DXGI_FORMAT aDsFormat);
	CubeMapRT(const CubeMapRT& CubeRT) = delete;
	CubeMapRT& operator=(const CubeMapRT CubeRT) = delete;

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE aSrvCpuHandle, CD3DX12_CPU_DESCRIPTOR_HANDLE aRtvCpuHandle[6],
		CD3DX12_CPU_DESCRIPTOR_HANDLE aDsvCpuHandle);
	void BuildResource();
	ID3D12Resource* GetRtResourcePtr();
	ID3D12Resource* GetDsResourcePtr();

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsvCpuHandle() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtvCpuHandle(size_t Index);
	D3D12_VIEWPORT GetViewport() const;
	RECT GetRect() const;
	
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> RtResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> DepthResource;
	ID3D12Device* DxDevice;
	UINT Width;
	UINT Height;
	DXGI_FORMAT RtFormat;
	DXGI_FORMAT DsFormat;

	CD3DX12_CPU_DESCRIPTOR_HANDLE SrvCpuHandle; 
	CD3DX12_CPU_DESCRIPTOR_HANDLE RtvCpuHandle[6];
	CD3DX12_CPU_DESCRIPTOR_HANDLE DsvCpuHandle;
};