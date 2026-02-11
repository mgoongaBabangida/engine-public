#include "stdafx.h"
#include "Timer.h"
#include <thread>
#include <future>

namespace math {

	//------------------------------------------------------------
	Timer::Timer(std::function<bool()> c) :callable(c), active(false), m_period(0)
	{

	}

	//------------------------------------------------------------
	bool Timer::start(unsigned int _period)
	{
		m_period = _period;
		bool fls = false;
		if (!active.compare_exchange_strong(fls, true))
		{
			return false;
		}
		else
		{
			clock.start();
			std::function<bool()> func = [this]()->bool
			{
				while (active)
				{
					if (clock.timeElapsedLastFrameMsc() >= m_period)
					{
						this->callable();
						this->clock.newFrame();
						//@todo add some sleeping
					}
				}
				return true;
			};

			fut = std::async(func);
			return true;
		}
	}

	//------------------------------------------------------------
	bool Timer::stop()
	{
		bool is_true = true;
		if (active.compare_exchange_strong(is_true, false))
		{
			if(fut.valid())
				fut.get();
			else
				return false;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------
	void Timer::setPeriod(unsigned int period)
	{
		m_period = period;
	}

	//------------------------------------------------------------
	Timer::~Timer()
	{
		this->stop();
	}
}