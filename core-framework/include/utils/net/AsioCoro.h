// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <chrono>
#include <tuple>
#include <system_error>
#include <utility>

#include "asio/ssl.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/write.hpp"
#include "asio/steady_timer.hpp"
#include "asio/this_coro.hpp"
#include "asio/use_awaitable.hpp"
#include "asio/co_spawn.hpp"
#include "asio/deferred.hpp"
#include "asio/experimental/parallel_group.hpp"
#include "asio/as_tuple.hpp"

namespace org::apache::nifi::minifi::utils::net {

constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

#if defined(__GNUC__) && __GNUC__ < 11
// [coroutines] unexpected 'warning: statement has no effect [-Wunused-value]'
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=96749
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#endif  // defined(__GNUC__) && __GNUC__ < 11
inline asio::awaitable<void> async_wait(asio::steady_timer& timer) {
  co_await timer.async_wait(utils::net::use_nothrow_awaitable);
}
#if defined(__GNUC__) && __GNUC__ < 11
#pragma GCC diagnostic pop
#endif  // defined(__GNUC__) && __GNUC__ < 11

inline asio::awaitable<void> async_wait(std::chrono::steady_clock::duration duration) {
  asio::steady_timer timer(co_await asio::this_coro::executor);  // NOLINT
  timer.expires_after(duration);
  co_await async_wait(timer);
}

template<class... Types>
asio::awaitable<std::tuple<std::error_code, Types...>> asyncOperationWithTimeout(asio::awaitable<std::tuple<std::error_code, Types...>>&& async_operation,
    std::chrono::steady_clock::duration timeout_duration) {
  auto executor = co_await asio::this_coro::executor;  // NOLINT(clang-analyzer-core.CallAndMessage)
  std::tuple<std::array<std::size_t, 2>, std::exception_ptr, std::tuple<std::error_code, Types...>, std::exception_ptr> parallel_group_result{
      co_await asio::experimental::make_parallel_group(  // NOLINT(clang-analyzer-core.CallAndMessage)
          asio::co_spawn(executor, std::move(async_operation), asio::deferred),
          asio::co_spawn(executor, async_wait(timeout_duration), asio::deferred))
      .async_wait(asio::experimental::wait_for_one_success(), asio::use_awaitable)
  };
  auto index_of_operation_which_finished_first = std::get<0>(parallel_group_result)[0];  // 0 = async_operation, 1 = async_wait(timeout_duration)
  if (index_of_operation_which_finished_first != 0) {
    std::tuple<std::error_code, Types...> result;
    std::get<0>(result) = asio::error::timed_out;
    co_return result;
  }
  co_return std::move(std::get<2>(parallel_group_result));
}
}  // namespace org::apache::nifi::minifi::utils::net
