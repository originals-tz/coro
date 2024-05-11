#include <channel.h>
#include <gtest/gtest.h>
#include "sleep.h"
#include "util.h"

using namespace coro;

Task<void> SleepTest()
{
    co_await Sleep(1, 0);
}


TEST(t, t1)
{
    auto t1 = RunTask(&SleepTest);
}