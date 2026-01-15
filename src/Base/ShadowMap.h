#pragma once
#include "../Utility/d3dUtil.h"

class ShadowMap
{
public:
	ShadowMap() = delete;
	ShadowMap(const ShadowMap&) = delete;
	ShadowMap& operator=(ShadowMap&) = delete;
	
	ShadowMap(ID3D12Device* aDevice, UINT aWidth, UINT aHeight);
	D3D12_VIEWPORT GetViewport();
	RECT GetRect();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsvHeapCpuHandle();

	ID3D12Resource* GetResourcePtr();
	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE aSRV , CD3DX12_CPU_DESCRIPTOR_HANDLE aDSV);

private:
	UINT Width;
	UINT Height;
	CD3DX12_CPU_DESCRIPTOR_HANDLE SRV; 
	CD3DX12_CPU_DESCRIPTOR_HANDLE DSV;

	ID3D12Device* Device;
	Microsoft::WRL::ComPtr<ID3D12Resource> DepthBufferResource;
	void BuildResource();
};