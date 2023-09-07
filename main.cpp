#include <iostream>
#include <latch>
#include <memory>
#include "sleep.h"
#include "task.h"

class SleepTask : public CoTask
{
public:
    Task CoHandle() override
    {
        std::cout << "start" << std::endl;
        co_await Test2();
        co_await CoSleep(1);
        std::cout << "end" << std::endl;
        co_return;
    }

    Task Test2()
    {
        std::cout << "start2" << std::endl;
        co_await Test3();
        co_await CoSleep(1);
        std::cout << "end2" << std::endl;
        co_return;
    }

    Task Test3()
    {
        std::cout << "start3" << std::endl;
        co_await CoSleep(1);
        std::cout << "end3" << std::endl;
        co_return;
    }
};

void TestSleep(int i)
{
    Manager mgr;
    mgr.AddTask(std::make_shared<SleepTask>());
    sleep(i + 1);
}

class Test1 : public CoTask
{
public:
    Task CoHandle() override
    {
        std::cout << "test1" << std::endl;
        co_await TestSub1();
        std::cout << "endtest" << std::endl;
    }

    Task TestSub1()
    {
        std::cout << "test sub 1" << std::endl;
        co_await std::suspend_always();
        std::cout << "end sub 1" << std::endl;
    }
};

class DoNothing : public CoTask
{
public:
    Task CoHandle() override
    {
        std::cout << "do nothing" << std::endl;
        co_return;
    }
};

void TestTask()
{
    Test1 test;
    auto task = test.CoHandle();
    task.Resume();
}

void Test()
{
    Manager mgr;
    mgr.AddTask(std::make_shared<DoNothing>());
    sleep(1);
}

int main()
{
    //    Test();
    TestSleep(1);
    //    TestTask();
    //    TestSleep(1);
    return 0;
}
