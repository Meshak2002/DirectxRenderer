#include "GameTime.h"
#include <windows.h>

GameTime::GameTime()
	: DeltaTime(0), PrevTime(0), CurrTime(0) , Stopped(false)
{
	QueryPerformanceFrequency((LARGE_INTEGER*)&CountsPerSecond);
	SecondsPerCount = 1.0 / (double)CountsPerSecond;
}

void GameTime::Reset()
{
	DeltaTime = 0;
	PrevTime = CurrTime;
}

void GameTime::Resume()
{
	if (Stopped)
	{
		Stopped = false;
	}
}

void GameTime::Pause()
{
	Stopped = true;
}

void GameTime::Tick()
{
	if (Stopped)
	{
		DeltaTime = 0;
		return;
	}
	__int64 CurrentTicks;
	QueryPerformanceCounter((LARGE_INTEGER*)&CurrentTicks);
	CurrTime = CurrentTicks * SecondsPerCount;
	DeltaTime = (float)(CurrTime - PrevTime);
	PrevTime = CurrTime;
}

float GameTime::GetDeltaTime() const
{
	return DeltaTime;
}
