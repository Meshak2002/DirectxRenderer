#pragma once
#include "UploadBuffer.h"

template<typename PassConstBufferStruct, typename ObjConstBufferStruct>
class FrameResource
{
public:
	FrameResource(ID3D12Device* Device3D, UINT PassCount, UINT ObjCount);
	FrameResource(const FrameResource& FResource) = delete;
	FrameResource& operator=(const FrameResource& FResource) = delete;
	~FrameResource() = default;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAlloc;
	UINT64 FenceValue{0};
	std::unique_ptr<UploadBuffer<PassConstBufferStruct>> PassConstBufferRes;
	std::unique_ptr<UploadBuffer<ObjConstBufferStruct>> ObjConstBufferRes;
};

template<typename PassConstBufferStruct, typename ObjConstBufferStruct>
inline FrameResource<PassConstBufferStruct,ObjConstBufferStruct>::FrameResource(ID3D12Device* Device3D, UINT PassCount, UINT ObjCount)
{
	Device3D->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAlloc));

	PassConstBufferRes = std::make_unique<UploadBuffer<PassConstBufferStruct>>(Device3D, PassCount, true);
	ObjConstBufferRes = std::make_unique<UploadBuffer<ObjConstBufferStruct>>(Device3D, ObjCount, true);
}
