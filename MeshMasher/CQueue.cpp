#include "CQueue.h"

void CQueue::push(Command* com) {
	std::lock_guard<std::mutex> lock(mut);
	commands.push(com);
	cv.notify_one();
}

Command* CQueue::pop() {
	std::unique_lock<std::mutex> lock(mut);
	cv.wait(lock, [this]() { return !commands.empty(); });
	auto com = commands.front();
	commands.pop();
	return com;
}