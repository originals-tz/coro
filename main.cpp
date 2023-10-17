#include <iostream>
#include <latch>
#include <memory>
#include "sleep.h"
#include "task.h"
#include "db.h"

class SleepTask : public CoTask
{
public:
    Task<void> CoHandle() override
    {
        std::cout << "start" << std::endl;
        co_await Test2();
        co_await CoSleep(1);
        std::cout << "end" << std::endl;
        co_return;
    }

    Task<void> Test2()
    {
        std::cout << "start2" << std::endl;
        std::cout <<  co_await Test3() << std::endl;
        co_await CoSleep(1);
        std::cout << "end2" << std::endl;
        co_return;
    }

    Task<int> Test3()
    {
        std::cout << "start3" << std::endl;
        co_await CoSleep(1);
        std::cout << "end3" << std::endl;
        co_return 10;
    }
};

class Noting : public CoTask
{
public:
    Task<void> CoHandle() override
    {
        std::cout << "cohandle begin" << std::endl;
        co_await Test1();
        std::cout << "cohandle end" << std::endl;
        co_return;
    }

    Task<void> Test1()
    {
        std::cout << "test1 begin" << std::endl;
        co_await CoSleep(1);
        std::cout << "test2 begin" << std::endl;
        co_return;
    }
};

class TestMYSQL : public CoTask
{
public:
    Task<void> CoHandle() override
    {
//        DB db("", 3309, "", "", "");
//        std::cout << "begin" << std::endl;
//        co_await db.Connect();
//        co_await db.Query("select * from secumain limit 10000");
//        co_await db.CoStoreResult();
//        std::cout << "end" << std::endl;
        co_return;
    }
};

void Test(int i)
{
    Manager mgr;
    mgr.AddTask(std::make_shared<SleepTask>());
//    mgr.AddTask(std::make_shared<TestMYSQL>());
    sleep(6);
}

int main()
{
    mysql_library_init(0, nullptr, nullptr);
    Test(1);
    return 0;
}
