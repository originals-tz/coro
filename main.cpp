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

int main()
{
    Manager mgr;
    mgr.AddTask(std::make_shared<SleepTask>());
    sleep(4);
    return 0;
}
