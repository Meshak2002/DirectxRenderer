#pragma once

class GameTime
{
	public:
	GameTime();
	void Reset(); // Resets the timer to zero
	void Resume(); // Starts the timer
	void Pause();
	void Tick();  // Updates the timer
	float GetDeltaTime() const; // Returns time between last two ticks

private:
	double  CountsPerSecond;
	double  SecondsPerCount;
	float  DeltaTime;
	double PrevTime;
	double CurrTime;
	bool    Stopped;
};