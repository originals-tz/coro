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
        co_await CoSleep(1);
        co_await Test2();
        std::cout << "end" << std::endl;
    }

    Task Test2()
    {
        std::cout << "start2" << std::endl;
        co_await CoSleep(1);
        co_await Test3();
        std::cout << "end2" << std::endl;
    }

    Task Test3()
    {
        std::cout << "start3" << std::endl;
        co_await CoSleep(1);
        std::cout << "end3" << std::endl;
    }
};

void TestSleep(int i)
{
    Manager mgr;
    mgr.AddTask(std::make_shared<SleepTask>());
    sleep(i);
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

void TestTask()
{
    Test1 test;
    auto task = test.CoHandle();
    task.Resume();
}

int main()
{
    TestSleep(1);
    TestSleep(3);
    TestSleep(4);
    TestTask();
    return 0;
}
