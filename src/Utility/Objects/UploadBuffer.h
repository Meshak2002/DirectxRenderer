#pragma once
#include "../d3dUtil.h"

template<typename DataType>
class UploadBuffer
{
public:

	UploadBuffer(ID3D12Device* Device , UINT TotalDataCount , bool bISConstBuffer)
	{
		TotalElementsCount = TotalDataCount;
		if (bISConstBuffer)
			PerDataSize = d3dUtil::CalcConstantBufferByteSize(sizeof(DataType));
		else
			PerDataSize = sizeof(DataType);

		TotalDataSize = PerDataSize * TotalElementsCount;
		auto HeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD) ;
		auto ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(TotalDataSize);
		ThrowIfFailed( Device->CreateCommittedResource(&HeapProperty ,D3D12_HEAP_FLAG_NONE,
			&ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Resource))  );

		ThrowIfFailed( Resource->Map(0, nullptr,  reinterpret_cast<void**>(&ResourceMap)));

	}
	UploadBuffer(const UploadBuffer& UBuffer) = delete;
	UploadBuffer& operator=(const UploadBuffer& UBuffer) = delete;
	~UploadBuffer()
	{
		if (Resource != nullptr)
		{
			Resource->Unmap(0, nullptr);
			ResourceMap = nullptr;
		}
	}

	
	D3D12_GPU_VIRTUAL_ADDRESS GetResourceGpuAddress() const;
	void CopyData(UINT ElementIndex , const DataType& Data);


private:
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource;

	BYTE* ResourceMap;
	UINT PerDataSize;
	UINT TotalDataSize;
	UINT TotalElementsCount;
};



template<typename DataType>
inline D3D12_GPU_VIRTUAL_ADDRESS UploadBuffer<DataType>::GetResourceGpuAddress() const
{
	return Resource->GetGPUVirtualAddress();
}

template<typename DataType>
inline void UploadBuffer<DataType>::CopyData(UINT ElementIndex, const DataType& Data)
{
	assert(ElementIndex < TotalElementsCount);
	memcpy(&ResourceMap[ElementIndex*PerDataSize], &Data, PerDataSize);
}
