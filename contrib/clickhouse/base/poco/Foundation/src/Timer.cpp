//
// Timer.cpp
//
// Library: Foundation
// Package: Threading
// Module:  Timer
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/Timer.h"
#include "DBPoco/ThreadPool.h"
#include "DBPoco/Exception.h"
#include "DBPoco/ErrorHandler.h"


namespace DBPoco {


Timer::Timer(long startInterval, long periodicInterval): 
	_startInterval(startInterval), 
	_periodicInterval(periodicInterval),
	_skipped(0),
	_pCallback(0)
{
	DB_poco_assert (startInterval >= 0 && periodicInterval >= 0);
}


Timer::~Timer()
{
	try
	{
		stop();
	}
	catch (...)
	{
		DB_poco_unexpected();
	}
}


void Timer::start(const AbstractTimerCallback& method)
{
	start(method, Thread::PRIO_NORMAL, ThreadPool::defaultPool());
}


void Timer::start(const AbstractTimerCallback& method, Thread::Priority priority)
{
	start(method, priority, ThreadPool::defaultPool());
}


void Timer::start(const AbstractTimerCallback& method, ThreadPool& threadPool)
{
	start(method, Thread::PRIO_NORMAL, threadPool);
}


void Timer::start(const AbstractTimerCallback& method, Thread::Priority priority, ThreadPool& threadPool)
{
	Clock nextInvocation;
	nextInvocation += static_cast<Clock::ClockVal>(_startInterval)*1000;
	
	FastMutex::ScopedLock lock(_mutex);	

	if (_pCallback)
	{
		throw DBPoco::IllegalStateException("Timer already running");
	}

	_nextInvocation = nextInvocation;
	_pCallback = method.clone();
	_wakeUp.reset();
	try
	{
		threadPool.startWithPriority(priority, *this);
	}
	catch (...)
	{
		delete _pCallback;
		_pCallback = 0;
		throw;
	}
}


void Timer::stop()
{
	FastMutex::ScopedLock lock(_mutex);
	if (_pCallback)
	{
		_periodicInterval = 0;
		_mutex.unlock();
		_wakeUp.set();
		_done.wait(); // warning: deadlock if called from timer callback
		_mutex.lock();
		delete _pCallback;
		_pCallback = 0;
	}
}


void Timer::restart()
{
	FastMutex::ScopedLock lock(_mutex);
	if (_pCallback)
	{
		_wakeUp.set();
	}
}


void Timer::restart(long milliseconds)
{
	DB_poco_assert (milliseconds >= 0);
	FastMutex::ScopedLock lock(_mutex);
	if (_pCallback)
	{
		_periodicInterval = milliseconds;
		_wakeUp.set();
	}
}


long Timer::getStartInterval() const
{
	FastMutex::ScopedLock lock(_mutex);
	return _startInterval;
}


void Timer::setStartInterval(long milliseconds)
{
	DB_poco_assert (milliseconds >= 0);
	FastMutex::ScopedLock lock(_mutex);
	_startInterval = milliseconds;
}


long Timer::getPeriodicInterval() const
{
	FastMutex::ScopedLock lock(_mutex);
	return _periodicInterval;
}


void Timer::setPeriodicInterval(long milliseconds)
{
	DB_poco_assert (milliseconds >= 0);
	FastMutex::ScopedLock lock(_mutex);
	_periodicInterval = milliseconds;
}


void Timer::run()
{
	DBPoco::Clock now;
	long interval(0);
	do
	{
		long sleep(0);
		do
		{
			now.update();
			sleep = static_cast<long>((_nextInvocation - now)/1000);
			if (sleep < 0)
			{
				if (interval == 0)
				{
					sleep = 0;
					break;
				}
				_nextInvocation += static_cast<Clock::ClockVal>(interval)*1000;
				++_skipped;
			}
		}
		while (sleep < 0);

		if (_wakeUp.tryWait(sleep))
		{
			DBPoco::FastMutex::ScopedLock lock(_mutex);
			_nextInvocation.update();
			interval = _periodicInterval;
		}
		else
		{
			try
			{
				_pCallback->invoke(*this);
			}
			catch (DBPoco::Exception& exc)
			{
				DBPoco::ErrorHandler::handle(exc);
			}
			catch (std::exception& exc)
			{
				DBPoco::ErrorHandler::handle(exc);
			}
			catch (...)
			{
				DBPoco::ErrorHandler::handle();
			}
			DBPoco::FastMutex::ScopedLock lock(_mutex);
			interval = _periodicInterval;
		}
		_nextInvocation += static_cast<Clock::ClockVal>(interval)*1000;
		_skipped = 0;
	}
	while (interval > 0);
	_done.set();
}


long Timer::skipped() const
{
	return _skipped;
}


AbstractTimerCallback::AbstractTimerCallback()
{
}


AbstractTimerCallback::AbstractTimerCallback(const AbstractTimerCallback& callback)
{
}


AbstractTimerCallback::~AbstractTimerCallback()
{
}


AbstractTimerCallback& AbstractTimerCallback::operator = (const AbstractTimerCallback& callback)
{
	return *this;
}


} // namespace DBPoco
