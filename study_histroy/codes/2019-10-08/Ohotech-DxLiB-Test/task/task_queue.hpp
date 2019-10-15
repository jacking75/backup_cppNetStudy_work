//          Copyright hotwatermorning 2013 - 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cassert>
#include <atomic>
#include <future>
#include <limits>
#include <thread>
#include <utility>

#include "./locked_queue.hpp"
#include "./task_impl.hpp"

namespace hwm {

namespace detail { namespace ns_task {

//! @class 태스크 큐 클라스
template<template<class...> class Allocator = std::allocator>
struct task_queue_with_allocator
{
    typedef std::unique_ptr<task_base>			task_ptr_t;
	typedef Allocator<task_ptr_t>				allocator;
	typedef locked_queue<task_ptr_t, std::deque<task_ptr_t, allocator>>
												queue_type;

    //! 기본 생성자
    //! std::thread::hardware_concurrency() 만큼 스레드를 기동한다
    task_queue_with_allocator()
        :   task_queue_()
        ,   terminated_flag_(false)
        ,   task_count_(0)
        ,   wait_before_destructed_(true)
    {
        setup(
            (std::max)(std::thread::hardware_concurrency(), 1u)
            );
    }

    //! @brief 생성자
    //! @detail 인수에 지정된 값만 스레들를 기동한다
    //! @param thread_limit [in] 기동하는 인수의 수
    //! @param queue_limit [in] 큐에 유지할 수 있는 태스크 수 한계
    explicit
    task_queue_with_allocator(size_t num_threads, size_t queue_limit = ((std::numeric_limits<size_t>::max)()))
        :   task_queue_(queue_limit)
        ,   terminated_flag_(false)
        ,   task_count_(0)
        ,   wait_before_destructed_(true)
    {
        assert(num_threads >= 1);
        assert(queue_limit >= 1);
		
        setup(num_threads);
    }

    //! @brief 소멸자
    //! @detail 소멸자는 스레드 종료를 대기하지만
    //! 이 때 wait_before_destructed()가 true인 경우
    //! 큐에 쌓인 태스크 모두를 실행하고, 모든 실행이 완료하면 스레드를 종료한다.
    //! false의 경우 큐에 쌓은 채로 태스크는 실행 되지 않고 
    //! 호출 시점에서 추출한 태스크만 실행하고 스레드를 종료한다
    //! @note wait_before_destructed()는 default는 true
    ~task_queue_with_allocator()
    {
        if(wait_before_destructed()) {
            wait();
        }

		set_terminate_flag(true);
		//! Add dummy task to wake all threads,
		//! so the threads check terminate flag and terminate itself.
		for(size_t i = 0; i < num_threads(); ++i) {
			enqueue([]{});
		}

        join_threads();
    }

	size_t num_threads() const { return threads_.size(); }

    //! @brief 태스크에 새로운 처리를 추가
    //! @detail 내부의 태스크 큐가 꽉찬 경우는 큐가 빌 때까지 처리를 블럭한다
    //! @param [in] f 다른 스레드에서 실행하고 싶은 함수나 함수 오브젝트 등
    //! @param [in] f에 대해서 적용하고 싶은 인수. Movable 해야한다.
    //! @return 태스크와 shared state를 공유하는 std::future 태스크 오브젝트
    template<class F, class... Args>
    auto enqueue(F&& f, Args&& ... args) -> std::future<decltype(f(std::forward<Args>(args)...))>
    {
        typedef decltype(f(std::forward<Args>(args)...)) result_t;
        typedef std::promise<result_t> promise_t;

        promise_t promise;
        auto future(promise.get_future());

        task_ptr_t ptask =
            make_task(
                std::move(promise), std::forward<F>(f), std::forward<Args>(args)...);

        {
            task_count_lock_t lock(task_count_mutex_);
            ++task_count_;
        }

        try {
            task_queue_.enqueue(std::move(ptask));
        } catch(...) {
            task_count_lock_t lock(task_count_mutex_);
            --task_count_;
            if(task_count_ == 0) {
                c_task_.notify_all();
            }
            throw;
        }

        return future;
    }

    //! @brief 모든 태스크 실행이 끝날 때까지 대기한다.
    //! @note wait()는 태스크의 실행을 대기할 뿐으로 enqueue() 호출은 블럭하지 않는다.
    //! 그래서 wait() 호출이 완료하기 전에 enqueue()가 계속 실행되면 wait()는 처리를 반환하지 않는다. 
    void    wait() const
    {
        task_count_lock_t lock(task_count_mutex_);
        scoped_add sa(waiting_count_);
		
        c_task_.wait(lock, [this]{ return task_count_ == 0; });
    }

    //! @brief 지정시각까지 모든 태스크 실행이 다 끝날 땨까지 대기한다
    //! @tparam TimePoint std::chrono::time_point 타입으로 변환 가능한 타입
    //! @param [in] tp 모든 태스크 실행 완료를 어느 시각까지 대기할지
    //! @return 모든 태스크가 실행이 끝난 경우는 true를 반환
    //! @note wait_until()는 태스크의 실행을 대기할 뿐으로 enqueue() 호출은 블럭하지 않는다.
    //! 그래서 wait_until() 호출이 완료하기 전에 enqueue()가 계속 실행되면 wait_until()는 처리를 반환하지 않는다.
    template<class TimePoint>
    bool    wait_until(TimePoint tp) const
    {
        task_count_lock_t lock(task_count_mutex_);
        scoped_add sa(waiting_count_);

        return c_task_.wait_until(lock, tp, [this]{ return task_count_ == 0; });
    }

    //! @brief 지정 시간 내에서 모든 태스크가 실행되는 것을 대기한다
    //! @tparam Duration std::chrono::duration 타입으로 변환 가능한 타입
    //! @param [in] tp 모든 태스크의 실행 완료를 어느 시간까지 대기할지
    //! @return 모든 태스크가 실행되어 끝난 경우에 true 반환.
    //! @note wait_for()는 태스크의 실행을 대기할 뿐으로 enqueue() 호출은 블럭하지 않는다.
    //! 그래서 wait_for() 호출이 완료하기 전에 enqueue()가 계속 실행되면 wait_for()는 처리를 반환하지 않는다.
    template<class Duration>
    bool    wait_for(Duration dur) const
    {
        task_count_lock_t lock(task_count_mutex_);
        scoped_add sa(waiting_count_);

        return c_task_.wait_for(lock, dur, [this]{ return task_count_ == 0; });
    }

    //! @brief 소멸자가 호출 되었을 때 쌓여 있는 태스크 모두를 실행할 때까지 대기할지를 반환한다
    //! @return 소멸자가 호출 되었을 때 모든 태스크 실행을 다 할때까지 대기하는 경우는 true를 반환
    bool        wait_before_destructed() const
    {
        return wait_before_destructed_.load();
    }

    //! @brief 소멸자가 호출 되었을 때 쌓여 있는 태스크를 모두 실행할 때까지 대기할지 어떨지를 지정한다.
    //! @param [in] 소멸자가 호출 되었을 때 모든 태스크를 실행할 때까지 대기하고 싶다면 true를 넘기는다
    void        set_wait_before_destructed(bool state)
    {
        wait_before_destructed_.store(state);
    }

private:
	typedef std::unique_lock<std::mutex> task_count_lock_t;

    queue_type				    task_queue_;
    std::vector<std::thread>    threads_;
    std::atomic<bool>           terminated_flag_;
    std::mutex mutable          task_count_mutex_;
    size_t                      task_count_;
    std::atomic<size_t> mutable waiting_count_;
    std::condition_variable mutable c_task_;
    std::atomic<bool>           wait_before_destructed_;

    struct scoped_add
    {
        scoped_add(std::atomic<size_t> &value)
            :   v_(value)
        {
            ++v_;
        }

        ~scoped_add()
        {
            --v_;
        }

		//! 명시적으로 복사 생성자/복사 대입 연산자를 삭제
		//! 이것에 의해 암묵적으로 이동 생성자/이동 대입 연산자도 정의도지 않는다
        scoped_add(scoped_add const &) = delete;
        scoped_add& operator=(scoped_add const &) = delete;

    private:
        std::atomic<size_t> &v_;
    };

private:

    void    set_terminate_flag(bool state)
    {
        terminated_flag_.store(state);
    }

    bool    is_terminated() const
    {
        return terminated_flag_.load();
    }

    bool    is_waiting() const
    {
        return waiting_count_.load() != 0;
    }

	void	process(int thread_index)
	{
		for( ; ; ) {
			if(is_terminated()) {
				break;
			}

			task_ptr_t task = task_queue_.dequeue();

			bool should_notify = false;

			task->run();

			{
				task_count_lock_t lock(task_count_mutex_);
				--task_count_;

				if(is_waiting() && task_count_ == 0) {
					should_notify = true;
				}
			}

			if(should_notify) {
				c_task_.notify_all();
			}
		}
	}

    void    setup(size_t num_threads)
    {
		threads_.resize(num_threads);
        for(size_t i = 0; i < num_threads; ++i) {
			threads_[i] = std::thread([this, i] { process(i); });
        }
    }

    void    join_threads()
    {
        assert(is_terminated());

        for(auto &th: threads_) {
            th.join();
        }
    }
};

}}  //detail::ns_task

//! hwm::detail::ns_task 내의 task_queue 클래스를 hwm 이름 공간에서 사용할 수 있도록
using detail::ns_task::task_queue_with_allocator;
using task_queue = task_queue_with_allocator<std::allocator>;

}   //namespace hwm
