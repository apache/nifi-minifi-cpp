// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <version>
#include <utility>
#include <iterator>

#ifdef _LIBCPP_VERSION
#include <functional>
#elif __cpp_lib_boyer_moore_searcher < 201603L
#include <experimental/functional>
#else
#include <functional>
#endif

namespace org::apache::nifi::minifi::utils {

// platform dependent workaround for boyer_moore_searcher
template<typename It, typename Hash = std::hash<typename std::iterator_traits<It>::value_type>, typename BinaryPredicate = std::equal_to<>>
class Searcher {
 public:
  template<typename It2>
  std::pair<It2, It2> operator()(It2 begin, It2 end) const {
    return impl_(std::move(begin), std::move(end));
  }

#ifdef _LIBCPP_VERSION

 public:
  Searcher(It begin, It end, Hash /*hash*/ = {}, BinaryPredicate pred = {})
      : impl_(std::move(begin), std::move(end), std::move(pred)) {}

 private:
  // fallback to default_searcher due to libcxx bug
  std::default_searcher<It, BinaryPredicate> impl_;

#elif __cpp_lib_boyer_moore_searcher < 201603L

 public:
  Searcher(It begin, It end, Hash hash = {}, BinaryPredicate pred = {})
      : impl_(std::move(begin), std::move(end), std::move(hash), std::move(pred)) {}

 private:
  std::experimental::boyer_moore_searcher<It, Hash, BinaryPredicate> impl_;

#else

 public:
  Searcher(It begin, It end, Hash hash = {}, BinaryPredicate pred = {})
      : impl_(std::move(begin), std::move(end), std::move(hash), std::move(pred)) {}

 private:
  std::boyer_moore_searcher<It, Hash, BinaryPredicate> impl_;
#endif
};

}  // namespace org::apache::nifi::minifi::utils
