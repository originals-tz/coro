#include "cotask.h"
#include "executor.h"

namespace coro
{
bool CoTask::Run(coro::Executor* exec)
{
    m_task = CoHandle();
    m_task->SetExecutor(exec);
    m_task->resume();
    return m_task->is_ready();
}
}