#include <cstring>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include "Worker.hpp"
#include "Session.hpp"
#include "Socket.hpp"

void Worker::handle(Session* session) {
    CurrentSession = session;
    waiting = false;
    return;
}

bool Worker::work(Worker* _this) {
    _this->active = true;
    while (!_this->should_stop) {
        if (!_this->waiting) {
            listener(reinterpret_cast<HTTP_Server*>(_this->s), _this->CurrentSession, _this);
            _this->waiting = true;
        }
        if (SocketQueue.size() > 0) {
            Session* sess = SocketQueue.back();
            _this->handle(sess);
            SocketQueue.pop();
        }
        else {
            _this->waiting = true;
        }
    }
    return true;
}

bool Worker::initialize(void* server) {
    next = nullptr;
    active = false;
    should_stop = false;
    waiting = true;
    s = server;
    thread = std::thread(&Worker::work, this);
	thread.detach();
    return true;
}

bool Worker::createNewWorker() {
    if (next != nullptr) {
        next = new Worker();
        next->initialize(s);
    }
    else {
        Worker* oldNext = next;
        next = new Worker();
        next->initialize(s);
        next->next = oldNext;
    }
    return true;
}

Worker* Worker::getNextWorker() {
	return next;
}

Worker* Workers;
int WorkersCount = 0;