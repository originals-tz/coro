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
//        co_await Test3();
//        co_await CoSleep(1);
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

class Noting : public CoTask
{
public:
    Task CoHandle() override
    {
        std::cout << "cohandle begin" << std::endl;
        co_await Test1();
        std::cout << "cohandle end" << std::endl;
        co_return;
    }

    Task Test1()
    {
        std::cout << "test1 begin" << std::endl;
        co_await CoSleep(1);
        std::cout << "test2 begin" << std::endl;
        co_return;
    }
};

void TestSleep(int i)
{
    Manager mgr;
//    mgr.AddTask(std::make_shared<SleepTask>());
    mgr.AddTask(std::make_shared<Noting>());
    sleep(2);
}

int main()
{
    TestSleep(1);
    return 0;
}
