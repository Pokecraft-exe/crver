#include <cstring>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "Worker.hpp"
#include "Session.hpp"
#include "Socket.hpp"

extern void log(std::string msg);

extern std::mutex queueMutex;
extern std::condition_variable queueCV;

bool Worker::work(Worker* _this) {
    _this->active = true;
    while (!_this->should_stop) {
        _this->waiting = true;
        
        std::unique_lock<std::mutex> lock(queueMutex);
		queueCV.wait(lock, [&] { return !SocketQueue.empty() || _this->should_stop; });
        
        if (_this->should_stop) {
            lock.unlock();
			_this->active = false;
            break;
		}
        Session* sess = SocketQueue.front();
        _this->CurrentSession = sess;

        log("Worker handling new session with HTTP_ID: " + std::to_string(
        sess->NEW_CONNECTION.http.Connection));

        SocketQueue.pop();
		lock.unlock();
        _this->waiting = false;

        listener(reinterpret_cast<HTTP_Server*>(_this->s), _this->CurrentSession, _this);
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