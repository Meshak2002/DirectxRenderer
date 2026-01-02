#pragma once
#include "Base/DxRenderBase.h"


class SimpleScreenApp : public DxRenderBase
{
public:
	SimpleScreenApp(HINSTANCE Instance);
	SimpleScreenApp(const SimpleScreenApp& ScreenApp)=delete;
	SimpleScreenApp& operator=(SimpleScreenApp & ScreenApp)=delete;

	virtual ~SimpleScreenApp() override;

	virtual void Draw(const GameTime& Gt) override;


};
