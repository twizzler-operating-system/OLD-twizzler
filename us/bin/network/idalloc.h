#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

template<typename T>
class id_allocator
{
  private:
	std::vector<T> stack;
	std::mutex m;
	T max = 0;

  public:
	T get()
	{
		std::lock_guard<std::mutex> _lg(m);
		if(stack.size() == 0) {
			return max++;
		}
		T t = stack.back();
		stack.pop_back();
		return t;
	}

	void put(T t)
	{
		std::lock_guard<std::mutex> _lg(m);
		stack.push_back(t);
	}
};
