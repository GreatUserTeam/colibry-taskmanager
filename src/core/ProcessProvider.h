#pragma once

#include "ProcessData.h"

namespace colibry::tm {

/**
 * The boundary between the Task Manager and the operating system.
 *
 * Everything the application knows about processes comes from read(), and the only thing it
 * can do to a process goes through terminate(). To run on a system that has no /proc,
 * implement this interface and return it from main().
 */
class ProcessProvider
{
public:
    struct Result
    {
        bool ok = false;
        QString error;
    };

    virtual ~ProcessProvider() = default;

    /// A fresh snapshot. Processes that vanish while it is being read are simply left out.
    virtual RawSnapshot read() = 0;

    /// Asks the process to quit (force == false) or kills it at once (force == true).
    virtual Result terminate(qint64 pid, bool force) = 0;
};

} // namespace colibry::tm
