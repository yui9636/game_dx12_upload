#pragma once

#include <windows.h>

class HighResolutionTimer
{
public:
	HighResolutionTimer() : delta_time(-1.0), paused_time(0), stopped(false)
	{
		LONGLONG counts_per_sec;
		QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&counts_per_sec));
		seconds_per_count = 1.0 / static_cast<double>(counts_per_sec);

		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time));
		base_time = this_time;
		last_time = this_time;
	}

	// Reset() が呼ばれてからの総経過時間を返す。停止中の時間は含めない。
	float TimeStamp() const  // 秒単位
	{
		// 停止中なら、停止してから経過した時間を数えない。
		// さらに以前にも pause があった場合、
		// stop_time - base_time には数えたくない pause 時間が含まれる。
		// それを補正するため、mStopTime から pause 時間を引く。
		//                     |<--paused_time: 停止累積時間-->|
		// 右方向が時間の進みを表す。
		//  base_time:基準  stop_time:停止  start_time:再開  stop_time:停止  this_time:現在

		if (stopped)
		{
			return static_cast<float>(((stop_time - paused_time) - base_time)*seconds_per_count);
		}

		// this_time - mBaseTime には pause 時間が含まれる。
		// それを数えないようにするため、
		// this_time から pause 時間を引く。
		//  (this_time - paused_time) - base_time で停止時間を除外する。
		//                     |<--paused_time: 停止累積時間-->|
		// 右方向が時間の進みを表す。
		//  base_time:基準  stop_time:停止  start_time:再開  this_time:現在
		else
		{
			return static_cast<float>(((this_time - paused_time) - base_time)*seconds_per_count);
		}
	}

	float TimeInterval() const  // 秒単位
	{
		return static_cast<float>(delta_time);
	}

	void Reset() // メッセージループ前に呼ぶ。
	{
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time));
		base_time = this_time;
		last_time = this_time;

		stop_time = 0;
		stopped = false;
	}

	void Start() // pause 解除時に呼ぶ。
	{
		LONGLONG start_time;
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&start_time));

		// stop と start の組の間に経過した時間を累積する。
		//                     |<-------d:停止していた時間------->|
		// 右方向が時間の進みを表す。
		//  base_time:基準  stop_time:停止  start_time:再開
		if (stopped)
		{
			paused_time += (start_time - stop_time);
			last_time = start_time;
			stop_time = 0;
			stopped = false;
		}
	}

	void Stop() // pause 時に呼ぶ。
	{
		if (!stopped)
		{
			QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&stop_time));
			stopped = true;
		}
	}

	void Tick() // 毎フレーム呼ぶ。
	{
		if (stopped)
		{
			delta_time = 0.0;
			return;
		}

		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time));
		// 現フレームと前フレームの時間差。
		delta_time = (this_time - last_time)*seconds_per_count;

		// 次フレームに備える。
		last_time = this_time;

		// 負値にならないようにする。DXSDK の CDXUTTimer では、
		// CPU が省電力モードに入ったり、別 processor へ移されたりすると、
		// mDeltaTime が負になることがあるとされている。
		if (delta_time < 0.0)
		{
			delta_time = 0.0;
		}
	}

private:
	double seconds_per_count;
	double delta_time;

	LONGLONG base_time;
	LONGLONG paused_time;
	LONGLONG stop_time;
	LONGLONG last_time;
	LONGLONG this_time;

	bool stopped;
};
