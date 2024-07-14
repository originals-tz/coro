#include <gtest/gtest.h>
#include "sleep.h"
#include "util.h"

using namespace coro;


Task<void> Test1()
{
    std::cout << "t1 start" << std::endl;
    co_await Sleep(1, 0);
    std::cout << "t1 end" << std::endl;
}


TEST(t, t1)
{
    auto t1 = RunTask(&Test1);
}