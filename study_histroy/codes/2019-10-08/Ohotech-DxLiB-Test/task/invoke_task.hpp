//          Copyright hotwatermorning 2013 - 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <future>
#include <utility>

namespace hwm {

namespace detail { namespace ns_task {

template<class F, class... Args>
void invoke_task(std::promise<void> &promise, F &&f, Args &&... args)
{
	auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    bound();
    promise.set_value();
}

template<class Ret, class F, class... Args>
void invoke_task(std::promise<Ret> &promise, F &&f, Args &&... args)
{
	auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    promise.set_value(bound());
}

}}  //namespace detail::ns_task

}   //namespace hwm

