// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "minifi-cpp/Exception.h"
#include "utils/file/AssetManager.h"

namespace org::apache::nifi::minifi::core {

using IdResolver = std::function<std::optional<std::string>(std::string_view /* category */, std::string_view /* value */)>;

class MalformedReferenceException : public Exception {
 public:
  explicit MalformedReferenceException(const std::string &message) : Exception(ExceptionType::FLOW_EXCEPTION, message) {}

  explicit MalformedReferenceException(const char *message) : Exception(ExceptionType::FLOW_EXCEPTION, message) {}
};

class UnknownCategoryException : public Exception {
 public:
  explicit UnknownCategoryException(const std::string &message) : Exception(ExceptionType::FLOW_EXCEPTION, message) {}

  explicit UnknownCategoryException(const char *message) : Exception(ExceptionType::FLOW_EXCEPTION, message) {}
};

std::string resolveIdentifier(std::string_view str, std::vector<IdResolver> resolvers) {
  std::string result;
  enum class ParseState {
    OutsideToken,
    InAtMark,
    InToken,
    InId
  } state{ParseState::OutsideToken};
  std::string category;
  std::string id;
  for (size_t offset = 0; offset < str.length(); ++offset) {
    if (state == ParseState::OutsideToken) {
      if (str[offset] == '@') {
        state = ParseState::InAtMark;
        continue;
      }
      result += str[offset];
    } else if (state == ParseState::InAtMark) {
      if (str[offset] == '@') {
        result += '@';
        state = ParseState::OutsideToken;
        continue;
      }
      if (str[offset] == '{') {
        state = ParseState::InToken;
        category.clear();
        id.clear();
        continue;
      }
      throw MalformedReferenceException(fmt::format("Malformed reference at {} in '{}'", offset, str));
    } else if (state == ParseState::InToken) {
      if (str[offset] == ':') {
        state = ParseState::InId;
        continue;
      }
      category += str[offset];
    } else if (state == ParseState::InId) {
      if (str[offset] == '}') {
        std::optional <std::string> replacement;
        for (auto &resolver: resolvers) {
          replacement = resolver(category, id);
          if (replacement) {
            break;
          }
        }
        if (!replacement) {
          throw UnknownCategoryException(fmt::format("Could not resolve '{}' in category '{}'", id, category));
        }
        result += replacement.value();
        state = ParseState::OutsideToken;
        continue;
      }
      id += str[offset];
    }
  }
  if (state != ParseState::OutsideToken) {
    throw MalformedReferenceException(fmt::format("Found invalid asset reference expression in '{}'", str));
  }
  return result;
}

class AssetException : public Exception {
 public:
  explicit AssetException(const std::string &message) : Exception(ExceptionType::ASSET_EXCEPTION, message) {}

  explicit AssetException(const char *message) : Exception(ExceptionType::ASSET_EXCEPTION, message) {}
};

IdResolver getAssetResolver(std::function<std::optional<std::filesystem::path>(std::string_view)> find_asset) {
  return [find_asset] (std::string_view category, std::string_view id) -> std::optional<std::string> {
    if (category != "asset-id") {
      return std::nullopt;
    }
    if (!find_asset) {
      throw AssetException(fmt::format("Asset manager is not available, asset path resolution of '{}' is not supported", id));
    }
    auto path = find_asset(id);
    if (!path) {
      throw AssetException(fmt::format("Failed to find asset @{{asset-id:{}}}", id));
    }
    return path.value().string();
  };
}

IdResolver getAssetResolver(utils::file::AssetManager* asset_manager) {
  if (!asset_manager) {
    return getAssetResolver(std::function<std::optional<std::filesystem::path>(std::string_view)>{nullptr});
  }
  return getAssetResolver([asset_manager] (std::string_view id) {
    return asset_manager->findAssetById(id);
  });
}

}  // namespace org::apache::nifi::minifi::core
