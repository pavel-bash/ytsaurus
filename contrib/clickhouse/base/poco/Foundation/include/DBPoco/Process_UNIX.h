//
// Process_UNIX.h
//
// Library: Foundation
// Package: Processes
// Module:  Process
//
// Definition of the ProcessImpl class for Unix.
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef DB_Foundation_Process_UNIX_INCLUDED
#define DB_Foundation_Process_UNIX_INCLUDED


#include <map>
#include <vector>
#include <unistd.h>
#include "DBPoco/Foundation.h"
#include "DBPoco/RefCountedObject.h"


namespace DBPoco
{


class Pipe;


class Foundation_API ProcessHandleImpl : public RefCountedObject
{
public:
    ProcessHandleImpl(pid_t pid);
    ~ProcessHandleImpl();

    pid_t id() const;
    int wait() const;

private:
    pid_t _pid;
};


class Foundation_API ProcessImpl
{
public:
    typedef pid_t PIDImpl;
    typedef std::vector<std::string> ArgsImpl;
    typedef std::map<std::string, std::string> EnvImpl;

    static PIDImpl idImpl();
    static void timesImpl(long & userTime, long & kernelTime);
    static ProcessHandleImpl * launchImpl(
        const std::string & command,
        const ArgsImpl & args,
        const std::string & initialDirectory,
        Pipe * inPipe,
        Pipe * outPipe,
        Pipe * errPipe,
        const EnvImpl & env);
    static void killImpl(ProcessHandleImpl & handle);
    static void killImpl(PIDImpl pid);
    static bool isRunningImpl(const ProcessHandleImpl & handle);
    static bool isRunningImpl(PIDImpl pid);
    static void requestTerminationImpl(PIDImpl pid);

private:
    static ProcessHandleImpl * launchByForkExecImpl(
        const std::string & command,
        const ArgsImpl & args,
        const std::string & initialDirectory,
        Pipe * inPipe,
        Pipe * outPipe,
        Pipe * errPipe,
        const EnvImpl & env);
};


} // namespace DBPoco


#endif // DB_Foundation_Process_UNIX_INCLUDED
