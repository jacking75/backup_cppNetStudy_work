//          Copyright hotwatermorning 2013 - 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

namespace hwm {

namespace detail { namespace ns_task {

//! Producer/Comsumer 패턴을 구현한다
template <class T, class UnderlyingContainer = std::deque<T>>
struct locked_queue
{
	typedef T value_type;
	typedef std::queue<T, UnderlyingContainer> container;

    //! デフォルトコンストラクタ
    locked_queue()
        :   capacity((std::numeric_limits<size_t>::max)())
    {}

    //! 생성자
    //! @param capacity 동시에 큐 가능한 최대 요소 수
    explicit
    locked_queue(size_t capacity)
        :   capacity(capacity)
    {}

    //! @brief 큐에 요소를 추가한다.
	//! @detail 큐의 요소 수가 capacity를 넘은 경우는
    //! dequeue 호출에 의해 요소가 빠질 때까지 처리를 블럭한다
    //! @param x 큐에 추가하는 요소.
    void enqueue(T x) {
        std::unique_lock<std::mutex> lock(m);
        c_enq.wait(lock, [this] { return data.size() != capacity; });
        data.push(std::move(x));
        c_deq.notify_one();
    }

	//! 큐의 선두에서 요소를 빼내는 것을 시도
	/*!
		큐에 요소가 들어가 있는 경우는 선두 요소를 빼내고 true를 반환한다
		그렇지 않으면 false를 반환한다
		@return 요소를 빼 냈는가?
	*/
	bool try_dequeue(T &t)
	{
		std::unique_lock<std::mutex> lock(m);
		if(!data.empty()) {
			t = std::move(data.front());
			data.pop();
			c_enq.notify_one();
			return true;
		} else {
			return false;
		}
	}

    //! @brief 큐에서 값을 추출하거나 지정 시간까지 시도한다
    //! @param t 큐에서 추출한 값을 move 대입으로 받는 오브젝트
    //! @param tp 언제까지 dequeue 처리를 시도할지를 지정하는 오브젝트. std::chrono::time_point 타입으로 변환 가능해야 한다.
    //! @return 추출에 성공한 경우는 true를 반환.
    template<class TimePoint>
    bool try_dequeue_until(T &t, TimePoint tp)
    {
        std::unique_lock<std::mutex> lock(m);
        bool const succeeded = 
            c_deq.wait_until(lock, tp, [this] { return !data.empty(); });

        if(succeeded) {
            t = std::move(data.front());
            data.pop();
            c_enq.notify_one();
        }

        return succeeded;
    }

    //! @brief 큐에서 값을 추출하거나 지정시간동안만 시도한다.
    //! @param t 큐에서 추출한 값을 move 대입으로 받는 오브젝트
    //! @param tp 언제까지 dequeue 처리를 시도할지를 지정하는 오브젝트. std::chrono::duration 타입으로 변환 가능해야 한다.
    //! @return 추출에 성공한 경우는 true를 반환.
    template<class Duration>
    bool try_dequeue_for(T &t, Duration dur)
    {
        return try_dequeue_until(
                t,
                std::chrono::steady_clock::now() + dur);
    }

    //! @brief 큐에서 값을 추출한다.
    //! @detail 큐가 빈 경우는 요소를 얻을 때까지 처리를 블럭한다.
    T dequeue() {
        std::unique_lock<std::mutex> lock(m);
        c_deq.wait(lock, [this] { return !data.empty(); });

        T ret = std::move(data.front());
        data.pop();
        c_enq.notify_one();

        return ret;
    }

private:
    std::mutex			m;
    container			data;
    size_t				capacity;
    std::condition_variable c_enq;
    std::condition_variable c_deq;
};

}}  //namespace detail::ns_task

}   //namespace hwm
