#pragma once
#include "Session.hpp"
#include <thread>
#include <vector>

class Worker {
private:
    Session* CurrentSession = nullptr;
    bool  waiting = false;
	Worker* next = nullptr;
    std::thread thread = {};
    void* s = nullptr;
public:
    bool  active = false;
    bool  should_stop = false;

    void handle(Session* session);
    bool initialize(void* s);
    bool createNewWorker();
	bool isWaiting() const { return waiting; }
	Worker* getNextWorker();
    static bool work(Worker* _this);
};

extern Worker* Workers;
extern int WorkersCount;