//
// Clock.cpp
//
// Library: Foundation
// Package: DateTime
// Module:  Clock
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/Clock.h"
#include "DBPoco/Exception.h"
#include "DBPoco/Timestamp.h"
#if defined(__MACH__)
#include <mach/mach.h>
#include <mach/clock.h>
#elif defined(DB_POCO_OS_FAMILY_UNIX)
#include <time.h>
#include <unistd.h>
#endif
#include <algorithm>
#undef min
#undef max
#include <limits>


#ifndef DB_POCO_HAVE_CLOCK_GETTIME
	#if (defined(_POSIX_TIMERS) && defined(CLOCK_REALTIME)) || defined(POCO_VXWORKS) || defined(__QNX__)
		#ifndef __APPLE__ // See GitHub issue #1453 - not available before Mac OS 10.12/iOS 10
			#define DB_POCO_HAVE_CLOCK_GETTIME
		#endif
	#endif
#endif


namespace DBPoco {


const Clock::ClockVal Clock::CLOCKVAL_MIN = std::numeric_limits<Clock::ClockVal>::min();
const Clock::ClockVal Clock::CLOCKVAL_MAX = std::numeric_limits<Clock::ClockVal>::max();


Clock::Clock()
{
	update();
}


Clock::Clock(ClockVal tv)
{
	_clock = tv;
}


Clock::Clock(const Clock& other)
{
	_clock = other._clock;
}


Clock::~Clock()
{
}


Clock& Clock::operator = (const Clock& other)
{
	_clock = other._clock;
	return *this;
}


Clock& Clock::operator = (ClockVal tv)
{
	_clock = tv;
	return *this;
}


void Clock::swap(Clock& timestamp)
{
	std::swap(_clock, timestamp._clock);
}


void Clock::update()
{
#if   defined(__MACH__)

	clock_serv_t cs;
	mach_timespec_t ts;

	host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cs);
	clock_get_time(cs, &ts);
	mach_port_deallocate(mach_task_self(), cs);
	
	_clock = ClockVal(ts.tv_sec)*resolution() + ts.tv_nsec/1000;

#elif defined(DB_POCO_HAVE_CLOCK_GETTIME)

	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts))
		throw SystemException("cannot get system clock");
	_clock = ClockVal(ts.tv_sec)*resolution() + ts.tv_nsec/1000;

#else

	DBPoco::Timestamp now;
	_clock = now.epochMicroseconds();
	
#endif
}


Clock::ClockDiff Clock::accuracy()
{
#if   defined(__MACH__)

	clock_serv_t cs;
	int nanosecs;
	mach_msg_type_number_t n = 1;

	host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cs);
	clock_get_attributes(cs, CLOCK_GET_TIME_RES, (clock_attr_t)&nanosecs, &n);
	mach_port_deallocate(mach_task_self(), cs);
	
	ClockVal acc = nanosecs/1000;
	return acc > 0 ? acc : 1;

#elif defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)

	struct timespec ts;
	if (clock_getres(CLOCK_MONOTONIC, &ts))
		throw SystemException("cannot get system clock");
	
	ClockVal acc = ClockVal(ts.tv_sec)*resolution() + ts.tv_nsec/1000;
	return acc > 0 ? acc : 1;

#else

	return 1000;
	
#endif
}

	
bool Clock::monotonic()
{
#if   defined(__MACH__)

	return true;

#elif defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)

	return true;

#else

	return false;
	
#endif
}


} // namespace DBPoco
