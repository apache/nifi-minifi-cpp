/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <atomic>
#include <chrono>
#include <utility>
#include <iostream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <regex>
#include <functional>
#include <string>

#include "rapidjson/reader.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"

#include "utils/StringUtils.h"
#include "utils/OsUtils.h"
#include "expression/Expression.h"
#include "utils/RegexUtils.h"
#include "utils/TimeUtil.h"

#ifdef WIN32
#pragma comment(lib, "wldap32.lib" )
#pragma comment(lib, "crypt32.lib" )
#pragma comment(lib, "Ws2_32.lib")

#ifndef CURL_STATICLIB
#define CURL_STATICLIB
#endif
#endif
#include <curl/curl.h>

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <WS2tcpip.h>
#include <sddl.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "Driver.h"

#include "core/logging/LoggerFactory.h"

#include "date/tz.h"
#include "utils/net/DNS.h"
#include "utils/expected.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::expression {

Expression compile(const std::string &expr_str) {
  std::stringstream expr_str_stream(expr_str);
  Driver driver(&expr_str_stream);
  Parser parser(&driver);
  parser.parse();
  return driver.result;
}

Expression make_static(std::string val) {
  return Expression(Value(std::move(val)));
}

Expression make_dynamic(const std::function<Value(const Parameters &params, const std::vector<Expression> &sub_exprs)> &val_fn) {
  return Expression(Value(), val_fn);
}

Expression make_dynamic_attr(const std::string &attribute_id) {
  return make_dynamic([attribute_id](const Parameters &params, const std::vector<Expression>& /*sub_exprs*/) -> Value {
    std::string result;
    const auto cur_flow_file = params.flow_file;
    if (cur_flow_file && cur_flow_file->getAttribute(attribute_id, result)) {
      return Value(result);
    } else {
      auto registry = params.registry_.lock();
      if (registry && registry->getConfigurationProperty(attribute_id , result)) {
        return Value(result);
      }
    }
    return {};
  });
}

Value resolve_user_id(const std::vector<Value> &args) {
  std::string name;
  if (args.size() == 1) {
    name = args[0].asString();
    if (!name.empty()) {
      name = minifi::utils::OsUtils::userIdToUsername(name);
    }
  }

  return Value(name);
}

Value expr_hostname(const std::vector<Value> &args) {
  std::array<char, 1024> hostname{};
  gethostname(hostname.data(), 1023);

  if (!args.empty() && args[0].asBoolean()) {
    int status = 0;
    struct addrinfo hints{};
    struct addrinfo *result = nullptr;
    struct addrinfo *addr_cursor = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    status = getaddrinfo(hostname.data(), nullptr, &hints, &result);

    if (status) {
      std::string message("Failed to resolve local hostname to discover IP: ");
      message.append(gai_strerror(status));
      throw std::runtime_error(message);
    }

    for (addr_cursor = result; addr_cursor != nullptr; addr_cursor = addr_cursor->ai_next) {
      if (strlen(addr_cursor->ai_canonname) > 0) {
        std::string c_host(addr_cursor->ai_canonname);
        freeaddrinfo(result);
        return Value(c_host);
      }
    }

    freeaddrinfo(result);
  }

  return Value(std::string(hostname.data()));
}

Value expr_ip(const std::vector<Value>& /*args*/) {
  std::array<char, 1024> hostname{};
  gethostname(hostname.data(), 1023);

  int status = 0;
  std::array<char, INET6_ADDRSTRLEN> ip_str{};
  struct sockaddr_in *addr = nullptr;
  struct addrinfo hints{};
  struct addrinfo *result = nullptr;
  struct addrinfo *addr_cursor = nullptr;
  hints.ai_family = AF_INET;

  status = getaddrinfo(hostname.data(), nullptr, &hints, &result);

  if (status) {
    std::string message("Failed to resolve local hostname to discover IP: ");
    message.append(gai_strerror(status));
    throw std::runtime_error(message);
  }

  for (addr_cursor = result; addr_cursor != nullptr; addr_cursor = addr_cursor->ai_next) {
    if (addr_cursor->ai_family == AF_INET) {
      addr = reinterpret_cast<struct sockaddr_in *>(addr_cursor->ai_addr);
      inet_ntop(addr_cursor->ai_family, &(addr->sin_addr), ip_str.data(), ip_str.size());
      freeaddrinfo(result);
      return Value(std::string(ip_str.data()));
    }
  }

  freeaddrinfo(result);
  return {};
}

Value expr_reverseDnsLookup(const std::vector<Value>& args) {
  std::string ip_address_str = args[0].asString();

  std::chrono::steady_clock::duration timeout_duration = 5s;
  if (args.size() > 1) {
    timeout_duration = std::chrono::milliseconds(args[1].asUnsignedLong());
  }

  return utils::net::addressFromString(ip_address_str)
      | utils::andThen([timeout_duration](const auto& ip_address) { return utils::net::reverseDnsLookup(ip_address, timeout_duration);})
      | utils::transform([](const auto& hostname)-> Value { return Value(hostname); })
      | utils::valueOrElse([&](std::error_code error_code) {
        if (error_code.value() == asio::error::timed_out) {
          core::logging::LoggerFactory<Expression>::getLogger()->log_warn("reverseDnsLookup timed out");
          return Value(ip_address_str);
        }
        throw std::system_error(error_code);
      });
}

Value expr_uuid(const std::vector<Value>& /*args*/) {
  return Value(utils::IdGenerator::getIdGenerator()->generate().to_string());
}

Value expr_toUpper(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return Value(result);
}

Value expr_toLower(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return Value(result);
}

Value expr_substring(const std::vector<Value> &args) {
  if (args.size() < 3) {
    auto offset = gsl::narrow<size_t>(args[1].asUnsignedLong());
    return Value{args[0].asString().substr(offset)};
  } else {
    auto offset = gsl::narrow<size_t>(args[1].asUnsignedLong());
    auto count = gsl::narrow<size_t>(args[2].asUnsignedLong());
    return Value{args[0].asString().substr(offset, count)};
  }
}

Value expr_substringBefore(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  return Value(arg_0.substr(0, arg_0.find(args[1].asString())));
}

Value expr_substringBeforeLast(const std::vector<Value> &args) {
  size_t last_pos = 0;
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  while (arg_0.find(arg_1, last_pos + 1) != std::string::npos) {
    last_pos = arg_0.find(arg_1, last_pos + 1);
  }
  return Value(arg_0.substr(0, last_pos));
}

Value expr_substringAfter(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  return Value(arg_0.substr(arg_0.find(arg_1) + arg_1.length()));
}

Value expr_substringAfterLast(const std::vector<Value> &args) {
  size_t last_pos = 0;
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  while (arg_0.find(arg_1, last_pos + 1) != std::string::npos) {
    last_pos = arg_0.find(arg_1, last_pos + 1);
  }
  return Value(arg_0.substr(last_pos + arg_1.length()));
}

Value expr_getDelimitedField(const std::vector<Value> &args) {
  const auto &subject = args[0].asString();
  const auto &index = args[1].asUnsignedLong() - 1;
  char delimiter_ch = ',';

  if (args.size() > 2) {
    delimiter_ch = args[2].asString()[0];
  }

  char quote_ch = '"';

  if (args.size() > 3) {
    quote_ch = args[3].asString()[0];
  }

  char escape_ch = '\\';

  if (args.size() > 4) {
    escape_ch = args[4].asString()[0];
  }

  bool strip_chars = false;

  if (args.size() > 5) {
    strip_chars = args[5].asBoolean();
  }

  enum parse_states {
    value,
    quote
  };

  parse_states parse_state = value;
  uint64_t field_idx = 0;
  size_t field_size = 0;
  std::string result;
  result.resize(1024);

  for (size_t parse_pos = 0; parse_pos < subject.length(); parse_pos++) {
    char cur_ch = subject[parse_pos];

    if (cur_ch == escape_ch) {
      if (!strip_chars && field_idx == index) {
        field_size++;

        if (field_size >= result.size()) {
          result.resize(result.size() + 1024);
        }

        result[field_size - 1] = escape_ch;
      }
      parse_pos++;
      if (parse_pos < subject.length()) {
        cur_ch = subject[parse_pos];
      } else {
        break;
      }
    }

    switch (parse_state) {
      case value:
        if (cur_ch == delimiter_ch) {
          field_idx++;
          if (field_idx > index) {
            break;
          }
          continue;
        } else if (cur_ch == quote_ch) {
          if (!strip_chars && field_idx == index) {
            field_size++;

            if (field_size >= result.size()) {
              result.resize(result.size() + 1024);
            }

            result[field_size - 1] = quote_ch;
          }
          parse_state = quote;
          continue;
        } else if (field_idx == index) {
          field_size++;

          if (field_size >= result.size()) {
            result.resize(result.size() + 1024);
          }

          result[field_size - 1] = cur_ch;
        }
        break;
      case quote:
        if (cur_ch == quote_ch) {
          if (!strip_chars && field_idx == index) {
            field_size++;

            if (field_size >= result.size()) {
              result.resize(result.size() + 1024);
            }

            result[field_size - 1] = quote_ch;
          }
          parse_state = value;
          continue;
        } else if (field_idx == index) {
          field_size++;

          if (field_size >= result.size()) {
            result.resize(result.size() + 1024);
          }

          result[field_size - 1] = cur_ch;
        }
        break;
    }
  }

  result.resize(field_size);

  return Value(result);
}

Value expr_startsWith(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  return Value(arg_0.substr(0, arg_1.length()) == arg_1);
}

Value expr_endsWith(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  return Value(arg_0.substr(arg_0.length() - arg_1.length()) == arg_1);
}

Value expr_contains(const std::vector<Value> &args) {
  return Value(std::string::npos != args[0].asString().find(args[1].asString()));
}

Value expr_in(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  for (size_t i = 1; i < args.size(); i++) {
    if (arg_0 == args[i].asString()) {
      return Value(true);
    }
  }

  return Value(false);
}

Value expr_indexOf(const std::vector<Value> &args) {
  auto pos = args[0].asString().find(args[1].asString());

  if (pos == std::string::npos) {
    return Value(static_cast<int64_t>(-1));
  } else {
    return Value(static_cast<int64_t>(pos));
  }
}

Value expr_lastIndexOf(const std::vector<Value> &args) {
  size_t pos = std::string::npos;
  const std::string &arg_0 = args[0].asString();
  const std::string &arg_1 = args[1].asString();
  auto cur_pos = arg_0.find(arg_1, 0);

  while (cur_pos != std::string::npos) {
    pos = cur_pos;
    cur_pos = arg_0.find(arg_1, pos + 1);
  }

  if (pos == std::string::npos) {
    return Value(static_cast<int64_t>(-1));
  } else {
    return Value(static_cast<int64_t>(pos));
  }
}

Value expr_escapeJson(const std::vector<Value> &args) {
  const std::string &arg_0 = args[0].asString();
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
  writer.String(arg_0.c_str());
  std::string result(buf.GetString());
  return Value(result.substr(1, result.length() - 2));
}

Value expr_unescapeJson(const std::vector<Value> &args) {
  std::stringstream arg_0_ss;
  arg_0_ss << "[\"" << args[0].asString() << "\"]";
  rapidjson::Reader reader;
  rapidjson::Document doc;
  doc.Parse(arg_0_ss.str().c_str());
  if (doc.IsArray() && doc.Size() == 1 && doc[0].IsString()) {
    return Value(std::string(doc[0].GetString()));
  } else {
    return {};
  }
}

Value expr_escapeHtml3(const std::vector<Value> &args) {
  return Value(utils::string::replaceMap(args[0].asString(), { { "!", "&excl;" }, { "\"", "&quot;" }, { "#", "&num;" }, { "$", "&dollar;" }, { "%", "&percnt;" }, { "&", "&amp;" },
                                                  { "'", "&apos;" }, { "(", "&lpar;" }, { ")", "&rpar;" }, { "*", "&ast;" }, { "+", "&plus;" }, { ",", "&comma;" }, { "-", "&minus;" }, { ".",
                                                      "&period;" }, { "/", "&sol;" }, { ":", "&colon;" }, { ";", "&semi;" }, { "<", "&lt;" }, { "=", "&equals;" }, { ">", "&gt;" }, { "?", "&quest;" },
                                                  { "@", "&commat;" }, { "[", "&lsqb;" }, { "\\", "&bsol;" }, { "]", "&rsqb;" }, { "^", "&circ;" }, { "_", "&lowbar;" }, { "`", "&grave;" }, { "{",
                                                      "&lcub;" }, { "|", "&verbar;" }, { "}", "&rcub;" }, { "~", "&tilde;" }, { "¡", "&iexcl;" }, { "¢", "&cent;" }, { "£", "&pound;" }, { "¤",
                                                      "&curren;" }, { "¥", "&yen;" }, { "¦", "&brkbar;" }, { "§", "&sect;" }, { "¨", "&uml;" }, { "©", "&copy;" }, { "ª", "&ordf;" },
                                                  { "«", "&laquo;" }, { "¬", "&not;" }, { "®", "&reg;" }, { "¯", "&macr;" }, { "°", "&deg;" }, { "±", "&plusmn;" }, { "²", "&sup2;" },
                                                  { "³", "&sup3;" }, { "´", "&acute;" }, { "µ", "&micro;" }, { "¶", "&para;" }, { "·", "&middot;" }, { "¸", "&cedil;" }, { "¹", "&sup1;" }, { "º",
                                                      "&ordm;" }, { "»", "&raquo;;" }, { "¼", "&frac14;" }, { "½", "&frac12;" }, { "¾", "&frac34;" }, { "¿", "&iquest;" }, { "À", "&Agrave;" }, { "Á",
                                                      "&Aacute;" }, { "Â", "&Acirc;" }, { "Ã", "&Atilde;" }, { "Ä", "&Auml;" }, { "Å", "&Aring;" }, { "Æ", "&AElig;" }, { "Ç", "&Ccedil;" }, { "È",
                                                      "&Egrave;" }, { "É", "&Eacute;" }, { "Ê", "&Ecirc;" }, { "Ë", "&Euml;" }, { "Ì", "&Igrave;" }, { "Í", "&Iacute;" }, { "Î", "&Icirc;" }, { "Ï",
                                                      "&Iuml;" }, { "Ð", "&ETH;" }, { "Ñ", "&Ntilde;" }, { "Ò", "&Ograve;" }, { "Ó", "&Oacute;" }, { "Ô", "&Ocirc;" }, { "Õ", "&Otilde;" }, { "Ö",
                                                      "&Ouml;" }, { "×", "&times;" }, { "Ø", "&Oslash;" }, { "Ù", "&Ugrave;;" }, { "Ú", "&Uacute;" }, { "Û", "&Ucirc;" }, { "Ü", "&Uuml;" }, { "Ý",
                                                      "&Yacute;" }, { "Þ", "&THORN;" }, { "ß", "&szlig;" }, { "à", "&agrave;" }, { "á", "&aacute;" }, { "â", "&acirc;" }, { "ã", "&atilde;" }, { "ä",
                                                      "&auml;" }, { "å", "&aring;" }, { "æ", "&aelig;" }, { "ç", "&ccedil;" }, { "è", "&egrave;" }, { "é", "&eacute;" }, { "ê", "&ecirc;" }, { "ë",
                                                      "&euml;" }, { "ì", "&igrave;" }, { "í", "&iacute;" }, { "î", "&icirc;" }, { "ï", "&iuml;" }, { "ð", "&eth;" }, { "ñ", "&ntilde;" }, { "ò",
                                                      "&ograve;" }, { "ó", "&oacute;" }, { "ô", "&ocirc;" }, { "õ", "&otilde;" }, { "ö", "&ouml;" }, { "÷", "&divide;" }, { "ø", "&oslash;" }, { "ù",
                                                      "&ugrave;" }, { "ú", "&uacute;" }, { "û", "&ucirc;" }, { "ü", "&uuml;" }, { "ý", "&yacute;" }, { "þ", "&thorn;" }, { "ÿ", "&yuml;" } }));
}

Value expr_escapeHtml4(const std::vector<Value> &args) {
  return Value(utils::string::replaceMap(args[0].asString(), { { "!", "&excl;" }, { "\"", "&quot;" }, { "#", "&num;" }, { "$", "&dollar;" }, { "%", "&percnt;" }, { "&", "&amp;" },
                                                  { "'", "&apos;" }, { "(", "&lpar;" }, { ")", "&rpar;" }, { "*", "&ast;" }, { "+", "&plus;" }, { ",", "&comma;" }, { "-", "&minus;" }, { ".",
                                                      "&period;" }, { "/", "&sol;" }, { ":", "&colon;" }, { ";", "&semi;" }, { "<", "&lt;" }, { "=", "&equals;" }, { ">", "&gt;" }, { "?", "&quest;" },
                                                  { "@", "&commat;" }, { "[", "&lsqb;" }, { "\\", "&bsol;" }, { "]", "&rsqb;" }, { "^", "&circ;" }, { "_", "&lowbar;" }, { "`", "&grave;" }, { "{",
                                                      "&lcub;" }, { "|", "&verbar;" }, { "}", "&rcub;" }, { "~", "&tilde;" }, { "¡", "&iexcl;" }, { "¢", "&cent;" }, { "£", "&pound;" }, { "¤",
                                                      "&curren;" }, { "¥", "&yen;" }, { "¦", "&brkbar;" }, { "§", "&sect;" }, { "¨", "&uml;" }, { "©", "&copy;" }, { "ª", "&ordf;" },
                                                  { "«", "&laquo;" }, { "¬", "&not;" }, { "®", "&reg;" }, { "¯", "&macr;" }, { "°", "&deg;" }, { "±", "&plusmn;" }, { "²", "&sup2;" },
                                                  { "³", "&sup3;" }, { "´", "&acute;" }, { "µ", "&micro;" }, { "¶", "&para;" }, { "·", "&middot;" }, { "¸", "&cedil;" }, { "¹", "&sup1;" }, { "º",
                                                      "&ordm;" }, { "»", "&raquo;;" }, { "¼", "&frac14;" }, { "½", "&frac12;" }, { "¾", "&frac34;" }, { "¿", "&iquest;" }, { "À", "&Agrave;" }, { "Á",
                                                      "&Aacute;" }, { "Â", "&Acirc;" }, { "Ã", "&Atilde;" }, { "Ä", "&Auml;" }, { "Å", "&Aring;" }, { "Æ", "&AElig;" }, { "Ç", "&Ccedil;" }, { "È",
                                                      "&Egrave;" }, { "É", "&Eacute;" }, { "Ê", "&Ecirc;" }, { "Ë", "&Euml;" }, { "Ì", "&Igrave;" }, { "Í", "&Iacute;" }, { "Î", "&Icirc;" }, { "Ï",
                                                      "&Iuml;" }, { "Ð", "&ETH;" }, { "Ñ", "&Ntilde;" }, { "Ò", "&Ograve;" }, { "Ó", "&Oacute;" }, { "Ô", "&Ocirc;" }, { "Õ", "&Otilde;" }, { "Ö",
                                                      "&Ouml;" }, { "×", "&times;" }, { "Ø", "&Oslash;" }, { "Ù", "&Ugrave;;" }, { "Ú", "&Uacute;" }, { "Û", "&Ucirc;" }, { "Ü", "&Uuml;" }, { "Ý",
                                                      "&Yacute;" }, { "Þ", "&THORN;" }, { "ß", "&szlig;" }, { "à", "&agrave;" }, { "á", "&aacute;" }, { "â", "&acirc;" }, { "ã", "&atilde;" }, { "ä",
                                                      "&auml;" }, { "å", "&aring;" }, { "æ", "&aelig;" }, { "ç", "&ccedil;" }, { "è", "&egrave;" }, { "é", "&eacute;" }, { "ê", "&ecirc;" }, { "ë",
                                                      "&euml;" }, { "ì", "&igrave;" }, { "í", "&iacute;" }, { "î", "&icirc;" }, { "ï", "&iuml;" }, { "ð", "&eth;" }, { "ñ", "&ntilde;" }, { "ò",
                                                      "&ograve;" }, { "ó", "&oacute;" }, { "ô", "&ocirc;" }, { "õ", "&otilde;" }, { "ö", "&ouml;" }, { "÷", "&divide;" }, { "ø", "&oslash;" }, { "ù",
                                                      "&ugrave;" }, { "ú", "&uacute;" }, { "û", "&ucirc;" }, { "ü", "&uuml;" }, { "ý", "&yacute;" }, { "þ", "&thorn;" }, { "ÿ", "&yuml;" }, { "\u0192",
                                                      "&fnof;" }, { "\u0391", "&Alpha;" }, { "\u0392", "&Beta;" }, { "\u0393", "&Gamma;" }, { "\u0394", "&Delta;" }, { "\u0395", "&Epsilon;" }, {
                                                      "\u0396", "&Zeta;" }, { "\u0397", "&Eta;" }, { "\u0398", "&Theta;" }, { "\u0399", "&Iota;" }, { "\u039A", "&Kappa;" }, { "\u039B", "&Lambda;" }, {
                                                      "\u039C", "&Mu;" }, { "\u039D", "&Nu;" }, { "\u039E", "&Xi;" }, { "\u039F", "&Omicron;" }, { "\u03A0", "&Pi;" }, { "\u03A1", "&Rho;" }, {
                                                      "\u03A3", "&Sigma;" }, { "\u03A4", "&Tau;" }, { "\u03A5", "&Upsilon;" }, { "\u03A6", "&Phi;" }, { "\u03A7", "&Chi;" }, { "\u03A8", "&Psi;" }, {
                                                      "\u03A9", "&Omega;" }, { "\u03B1", "&alpha;" }, { "\u03B2", "&beta;" }, { "\u03B3", "&gamma;" }, { "\u03B4", "&delta;" },
                                                  { "\u03B5", "&epsilon;" }, { "\u03B6", "&zeta;" }, { "\u03B7", "&eta;" }, { "\u03B8", "&theta;" }, { "\u03B9", "&iota;" }, { "\u03BA", "&kappa;" }, {
                                                      "\u03BB", "&lambda;" }, { "\u03BC", "&mu;" }, { "\u03BD", "&nu;" }, { "\u03BE", "&xi;" }, { "\u03BF", "&omicron;" }, { "\u03C0", "&pi;" }, {
                                                      "\u03C1", "&rho;" }, { "\u03C2", "&sigmaf;" }, { "\u03C3", "&sigma;" }, { "\u03C4", "&tau;" }, { "\u03C5", "&upsilon;" }, { "\u03C6", "&phi;" }, {
                                                      "\u03C7", "&chi;" }, { "\u03C8", "&psi;" }, { "\u03C9", "&omega;" }, { "\u03D1", "&thetasym;" }, { "\u03D2", "&upsih;" }, { "\u03D6", "&piv;" }, {
                                                      "\u2022", "&bull;" }, { "\u2026", "&hellip;" }, { "\u2032", "&prime;" }, { "\u2033", "&Prime;" }, { "\u203E", "&oline;" },
                                                  { "\u2044", "&frasl;" }, { "\u2118", "&weierp;" }, { "\u2111", "&image;" }, { "\u211C", "&real;" }, { "\u2122", "&trade;" },
                                                  { "\u2135", "&alefsym;" }, { "\u2190", "&larr;" }, { "\u2191", "&uarr;" }, { "\u2192", "&rarr;" }, { "\u2193", "&darr;" }, { "\u2194", "&harr;" }, {
                                                      "\u21B5", "&crarr;" }, { "\u21D0", "&lArr;" }, { "\u21D1", "&uArr;" }, { "\u21D2", "&rArr;" }, { "\u21D3", "&dArr;" }, { "\u21D4", "&hArr;" }, {
                                                      "\u2200", "&forall;" }, { "\u2202", "&part;" }, { "\u2203", "&exist;" }, { "\u2205", "&empty;" }, { "\u2207", "&nabla;" }, { "\u2208", "&isin;" },
                                                  { "\u2209", "&notin;" }, { "\u220B", "&ni;" }, { "\u220F", "&prod;" }, { "\u2211", "&sum;" }, { "\u2212", "&minus;" }, { "\u2217", "&lowast;" }, {
                                                      "\u221A", "&radic;" }, { "\u221D", "&prop;" }, { "\u221E", "&infin;" }, { "\u2220", "&ang;" }, { "\u2227", "&and;" }, { "\u2228", "&or;" }, {
                                                      "\u2229", "&cap;" }, { "\u222A", "&cup;" }, { "\u222B", "&int;" }, { "\u2234", "&there4;" }, { "\u223C", "&sim;" }, { "\u2245", "&cong;" }, {
                                                      "\u2248", "&asymp;" }, { "\u2260", "&ne;" }, { "\u2261", "&equiv;" }, { "\u2264", "&le;" }, { "\u2265", "&ge;" }, { "\u2282", "&sub;" }, {
                                                      "\u2283", "&sup;" }, { "\u2284", "&nsub;" }, { "\u2286", "&sube;" }, { "\u2287", "&supe;" }, { "\u2295", "&oplus;" }, { "\u2297", "&otimes;" }, {
                                                      "\u22A5", "&perp;" }, { "\u22C5", "&sdot;" }, { "\u2308", "&lceil;" }, { "\u2309", "&rceil;" }, { "\u230A", "&lfloor;" },
                                                  { "\u230B", "&rfloor;" }, { "\u2329", "&lang;" }, { "\u232A", "&rang;" }, { "\u25CA", "&loz;" }, { "\u2660", "&spades;" }, { "\u2663", "&clubs;" }, {
                                                      "\u2665", "&hearts;" }, { "\u2666", "&diams;" }, { "\u0152", "&OElig;" }, { "\u0153", "&oelig;" }, { "\u0160", "&Scaron;" }, { "\u0161",
                                                      "&scaron;" }, { "\u0178", "&Yuml;" }, { "\u02C6", "&circ;" }, { "\u02DC", "&tilde;" }, { "\u2002", "&ensp;" }, { "\u2003", "&emsp;" }, { "\u2009",
                                                      "&thinsp;" }, { "\u200C", "&zwnj;" }, { "\u200D", "&zwj;" }, { "\u200E", "&lrm;" }, { "\u200F", "&rlm;" }, { "\u2013", "&ndash;" }, { "\u2014",
                                                      "&mdash;" }, { "\u2018", "&lsquo;" }, { "\u2019", "&rsquo;" }, { "\u201A", "&sbquo;" }, { "\u201C", "&ldquo;" }, { "\u201D", "&rdquo;" }, {
                                                      "\u201E", "&bdquo;" }, { "\u2020", "&dagger;" }, { "\u2021", "&Dagger;" }, { "\u2030", "&permil;" }, { "\u2039", "&lsaquo;" }, { "\u203A",
                                                      "&rsaquo;" }, { "\u20AC", "&euro;" } }));
}

Value expr_unescapeHtml3(const std::vector<Value> &args) {
  return Value(utils::string::replaceMap(args[0].asString(), { { "&excl;", "!" }, { "&quot;", "\"" }, { "&num;", "#" }, { "&dollar;", "$" }, { "&percnt;", "%" }, { "&amp;", "&" },
                                                  { "&apos;", "'" }, { "&lpar;", "(" }, { "&rpar;", ")" }, { "&ast;", "*" }, { "&plus;", "+" }, { "&comma;", "," }, { "&minus;", "-" }, { "&period;",
                                                      "." }, { "&sol;", "/" }, { "&colon;", ":" }, { "&semi;", ";" }, { "&lt;", "<" }, { "&equals;", "=" }, { "&gt;", ">" }, { "&quest;", "?" }, {
                                                      "&commat;", "@" }, { "&lsqb;", "[" }, { "&bsol;", "\\" }, { "&rsqb;", "]" }, { "&circ;", "^" }, { "&lowbar;", "_" }, { "&grave;", "`" }, {
                                                      "&lcub;", "{" }, { "&verbar;", "|" }, { "&rcub;", "}" }, { "&tilde;", "~" }, { "&iexcl;", "¡" }, { "&cent;", "¢" }, { "&pound;", "£" }, {
                                                      "&curren;", "¤" }, { "&yen;", "¥" }, { "&brkbar;", "¦" }, { "&sect;", "§" }, { "&uml;", "¨" }, { "&copy;", "©" }, { "&ordf;", "ª" }, { "&laquo;",
                                                      "«" }, { "&not;", "¬" }, { "&reg;", "®" }, { "&macr;", "¯" }, { "&deg;", "°" }, { "&plusmn;", "±" }, { "&sup2;", "²" }, { "&sup3;", "³" }, {
                                                      "&acute;", "´" }, { "&micro;", "µ" }, { "&para;", "¶" }, { "&middot;", "·" }, { "&cedil;", "¸" }, { "&sup1;", "¹" }, { "&ordm;", "º" }, {
                                                      "&raquo;;", "»" }, { "&frac14;", "¼" }, { "&frac12;", "½" }, { "&frac34;", "¾" }, { "&iquest;", "¿" }, { "&Agrave;", "À" }, { "&Aacute;", "Á" }, {
                                                      "&Acirc;", "Â" }, { "&Atilde;", "Ã" }, { "&Auml;", "Ä" }, { "&Aring;", "Å" }, { "&AElig;", "Æ" }, { "&Ccedil;", "Ç" }, { "&Egrave;", "È" }, {
                                                      "&Eacute;", "É" }, { "&Ecirc;", "Ê" }, { "&Euml;", "Ë" }, { "&Igrave;", "Ì" }, { "&Iacute;", "Í" }, { "&Icirc;", "Î" }, { "&Iuml;", "Ï" }, {
                                                      "&ETH;", "Ð" }, { "&Ntilde;", "Ñ" }, { "&Ograve;", "Ò" }, { "&Oacute;", "Ó" }, { "&Ocirc;", "Ô" }, { "&Otilde;", "Õ" }, { "&Ouml;", "Ö" }, {
                                                      "&times;", "×" }, { "&Oslash;", "Ø" }, { "&Ugrave;;", "Ù" }, { "&Uacute;", "Ú" }, { "&Ucirc;", "Û" }, { "&Uuml;", "Ü" }, { "&Yacute;", "Ý" }, {
                                                      "&THORN;", "Þ" }, { "&szlig;", "ß" }, { "&agrave;", "à" }, { "&aacute;", "á" }, { "&acirc;", "â" }, { "&atilde;", "ã" }, { "&auml;", "ä" }, {
                                                      "&aring;", "å" }, { "&aelig;", "æ" }, { "&ccedil;", "ç" }, { "&egrave;", "è" }, { "&eacute;", "é" }, { "&ecirc;", "ê" }, { "&euml;", "ë" }, {
                                                      "&igrave;", "ì" }, { "&iacute;", "í" }, { "&icirc;", "î" }, { "&iuml;", "ï" }, { "&eth;", "ð" }, { "&ntilde;", "ñ" }, { "&ograve;", "ò" }, {
                                                      "&oacute;", "ó" }, { "&ocirc;", "ô" }, { "&otilde;", "õ" }, { "&ouml;", "ö" }, { "&divide;", "÷" }, { "&oslash;", "ø" }, { "&ugrave;", "ù" }, {
                                                      "&uacute;", "ú" }, { "&ucirc;", "û" }, { "&uuml;", "ü" }, { "&yacute;", "ý" }, { "&thorn;", "þ" }, { "&yuml;", "ÿ" } }));
}

Value expr_unescapeHtml4(const std::vector<Value> &args) {
  return Value(utils::string::replaceMap(args[0].asString(), { { "&excl;", "!" }, { "&quot;", "\"" }, { "&num;", "#" }, { "&dollar;", "$" }, { "&percnt;", "%" }, { "&amp;", "&" },
                                                  { "&apos;", "'" }, { "&lpar;", "(" }, { "&rpar;", ")" }, { "&ast;", "*" }, { "&plus;", "+" }, { "&comma;", "," }, { "&minus;", "-" }, { "&period;",
                                                      "." }, { "&sol;", "/" }, { "&colon;", ":" }, { "&semi;", ";" }, { "&lt;", "<" }, { "&equals;", "=" }, { "&gt;", ">" }, { "&quest;", "?" }, {
                                                      "&commat;", "@" }, { "&lsqb;", "[" }, { "&bsol;", "\\" }, { "&rsqb;", "]" }, { "&circ;", "^" }, { "&lowbar;", "_" }, { "&grave;", "`" }, {
                                                      "&lcub;", "{" }, { "&verbar;", "|" }, { "&rcub;", "}" }, { "&tilde;", "~" }, { "&iexcl;", "¡" }, { "&cent;", "¢" }, { "&pound;", "£" }, {
                                                      "&curren;", "¤" }, { "&yen;", "¥" }, { "&brkbar;", "¦" }, { "&sect;", "§" }, { "&uml;", "¨" }, { "&copy;", "©" }, { "&ordf;", "ª" }, { "&laquo;",
                                                      "«" }, { "&not;", "¬" }, { "&reg;", "®" }, { "&macr;", "¯" }, { "&deg;", "°" }, { "&plusmn;", "±" }, { "&sup2;", "²" }, { "&sup3;", "³" }, {
                                                      "&acute;", "´" }, { "&micro;", "µ" }, { "&para;", "¶" }, { "&middot;", "·" }, { "&cedil;", "¸" }, { "&sup1;", "¹" }, { "&ordm;", "º" }, {
                                                      "&raquo;;", "»" }, { "&frac14;", "¼" }, { "&frac12;", "½" }, { "&frac34;", "¾" }, { "&iquest;", "¿" }, { "&Agrave;", "À" }, { "&Aacute;", "Á" }, {
                                                      "&Acirc;", "Â" }, { "&Atilde;", "Ã" }, { "&Auml;", "Ä" }, { "&Aring;", "Å" }, { "&AElig;", "Æ" }, { "&Ccedil;", "Ç" }, { "&Egrave;", "È" }, {
                                                      "&Eacute;", "É" }, { "&Ecirc;", "Ê" }, { "&Euml;", "Ë" }, { "&Igrave;", "Ì" }, { "&Iacute;", "Í" }, { "&Icirc;", "Î" }, { "&Iuml;", "Ï" }, {
                                                      "&ETH;", "Ð" }, { "&Ntilde;", "Ñ" }, { "&Ograve;", "Ò" }, { "&Oacute;", "Ó" }, { "&Ocirc;", "Ô" }, { "&Otilde;", "Õ" }, { "&Ouml;", "Ö" }, {
                                                      "&times;", "×" }, { "&Oslash;", "Ø" }, { "&Ugrave;;", "Ù" }, { "&Uacute;", "Ú" }, { "&Ucirc;", "Û" }, { "&Uuml;", "Ü" }, { "&Yacute;", "Ý" }, {
                                                      "&THORN;", "Þ" }, { "&szlig;", "ß" }, { "&agrave;", "à" }, { "&aacute;", "á" }, { "&acirc;", "â" }, { "&atilde;", "ã" }, { "&auml;", "ä" }, {
                                                      "&aring;", "å" }, { "&aelig;", "æ" }, { "&ccedil;", "ç" }, { "&egrave;", "è" }, { "&eacute;", "é" }, { "&ecirc;", "ê" }, { "&euml;", "ë" }, {
                                                      "&igrave;", "ì" }, { "&iacute;", "í" }, { "&icirc;", "î" }, { "&iuml;", "ï" }, { "&eth;", "ð" }, { "&ntilde;", "ñ" }, { "&ograve;", "ò" }, {
                                                      "&oacute;", "ó" }, { "&ocirc;", "ô" }, { "&otilde;", "õ" }, { "&ouml;", "ö" }, { "&divide;", "÷" }, { "&oslash;", "ø" }, { "&ugrave;", "ù" }, {
                                                      "&uacute;", "ú" }, { "&ucirc;", "û" }, { "&uuml;", "ü" }, { "&yacute;", "ý" }, { "&thorn;", "þ" }, { "&yuml;", "ÿ" }, { "&fnof;", "\u0192" }, {
                                                      "&Alpha;", "\u0391" }, { "&Beta;", "\u0392" }, { "&Gamma;", "\u0393" }, { "&Delta;", "\u0394" }, { "&Epsilon;", "\u0395" },
                                                  { "&Zeta;", "\u0396" }, { "&Eta;", "\u0397" }, { "&Theta;", "\u0398" }, { "&Iota;", "\u0399" }, { "&Kappa;", "\u039A" }, { "&Lambda;", "\u039B" }, {
                                                      "&Mu;", "\u039C" }, { "&Nu;", "\u039D" }, { "&Xi;", "\u039E" }, { "&Omicron;", "\u039F" }, { "&Pi;", "\u03A0" }, { "&Rho;", "\u03A1" }, {
                                                      "&Sigma;", "\u03A3" }, { "&Tau;", "\u03A4" }, { "&Upsilon;", "\u03A5" }, { "&Phi;", "\u03A6" }, { "&Chi;", "\u03A7" }, { "&Psi;", "\u03A8" }, {
                                                      "&Omega;", "\u03A9" }, { "&alpha;", "\u03B1" }, { "&beta;", "\u03B2" }, { "&gamma;", "\u03B3" }, { "&delta;", "\u03B4" },
                                                  { "&epsilon;", "\u03B5" }, { "&zeta;", "\u03B6" }, { "&eta;", "\u03B7" }, { "&theta;", "\u03B8" }, { "&iota;", "\u03B9" }, { "&kappa;", "\u03BA" }, {
                                                      "&lambda;", "\u03BB" }, { "&mu;", "\u03BC" }, { "&nu;", "\u03BD" }, { "&xi;", "\u03BE" }, { "&omicron;", "\u03BF" }, { "&pi;", "\u03C0" }, {
                                                      "&rho;", "\u03C1" }, { "&sigmaf;", "\u03C2" }, { "&sigma;", "\u03C3" }, { "&tau;", "\u03C4" }, { "&upsilon;", "\u03C5" }, { "&phi;", "\u03C6" }, {
                                                      "&chi;", "\u03C7" }, { "&psi;", "\u03C8" }, { "&omega;", "\u03C9" }, { "&thetasym;", "\u03D1" }, { "&upsih;", "\u03D2" }, { "&piv;", "\u03D6" }, {
                                                      "&bull;", "\u2022" }, { "&hellip;", "\u2026" }, { "&prime;", "\u2032" }, { "&Prime;", "\u2033" }, { "&oline;", "\u203E" },
                                                  { "&frasl;", "\u2044" }, { "&weierp;", "\u2118" }, { "&image;", "\u2111" }, { "&real;", "\u211C" }, { "&trade;", "\u2122" },
                                                  { "&alefsym;", "\u2135" }, { "&larr;", "\u2190" }, { "&uarr;", "\u2191" }, { "&rarr;", "\u2192" }, { "&darr;", "\u2193" }, { "&harr;", "\u2194" }, {
                                                      "&crarr;", "\u21B5" }, { "&lArr;", "\u21D0" }, { "&uArr;", "\u21D1" }, { "&rArr;", "\u21D2" }, { "&dArr;", "\u21D3" }, { "&hArr;", "\u21D4" }, {
                                                      "&forall;", "\u2200" }, { "&part;", "\u2202" }, { "&exist;", "\u2203" }, { "&empty;", "\u2205" }, { "&nabla;", "\u2207" }, { "&isin;", "\u2208" },
                                                  { "&notin;", "\u2209" }, { "&ni;", "\u220B" }, { "&prod;", "\u220F" }, { "&sum;", "\u2211" }, { "&minus;", "\u2212" }, { "&lowast;", "\u2217" }, {
                                                      "&radic;", "\u221A" }, { "&prop;", "\u221D" }, { "&infin;", "\u221E" }, { "&ang;", "\u2220" }, { "&and;", "\u2227" }, { "&or;", "\u2228" }, {
                                                      "&cap;", "\u2229" }, { "&cup;", "\u222A" }, { "&int;", "\u222B" }, { "&there4;", "\u2234" }, { "&sim;", "\u223C" }, { "&cong;", "\u2245" }, {
                                                      "&asymp;", "\u2248" }, { "&ne;", "\u2260" }, { "&equiv;", "\u2261" }, { "&le;", "\u2264" }, { "&ge;", "\u2265" }, { "&sub;", "\u2282" }, {
                                                      "&sup;", "\u2283" }, { "&nsub;", "\u2284" }, { "&sube;", "\u2286" }, { "&supe;", "\u2287" }, { "&oplus;", "\u2295" }, { "&otimes;", "\u2297" }, {
                                                      "&perp;", "\u22A5" }, { "&sdot;", "\u22C5" }, { "&lceil;", "\u2308" }, { "&rceil;", "\u2309" }, { "&lfloor;", "\u230A" },
                                                  { "&rfloor;", "\u230B" }, { "&lang;", "\u2329" }, { "&rang;", "\u232A" }, { "&loz;", "\u25CA" }, { "&spades;", "\u2660" }, { "&clubs;", "\u2663" }, {
                                                      "&hearts;", "\u2665" }, { "&diams;", "\u2666" }, { "&OElig;", "\u0152" }, { "&oelig;", "\u0153" }, { "&Scaron;", "\u0160" }, { "&scaron;",
                                                      "\u0161" }, { "&Yuml;", "\u0178" }, { "&circ;", "\u02C6" }, { "&tilde;", "\u02DC" }, { "&ensp;", "\u2002" }, { "&emsp;", "\u2003" }, { "&thinsp;",
                                                      "\u2009" }, { "&zwnj;", "\u200C" }, { "&zwj;", "\u200D" }, { "&lrm;", "\u200E" }, { "&rlm;", "\u200F" }, { "&ndash;", "\u2013" }, { "&mdash;",
                                                      "\u2014" }, { "&lsquo;", "\u2018" }, { "&rsquo;", "\u2019" }, { "&sbquo;", "\u201A" }, { "&ldquo;", "\u201C" }, { "&rdquo;", "\u201D" }, {
                                                      "&bdquo;", "\u201E" }, { "&dagger;", "\u2020" }, { "&Dagger;", "\u2021" }, { "&permil;", "\u2030" }, { "&lsaquo;", "\u2039" }, { "&rsaquo;",
                                                      "\u203A" }, { "&euro;", "\u20AC" } }));
}

Value expr_escapeXml(const std::vector<Value> &args) {
  return Value(utils::string::replaceMap(args[0].asString(), { { "\"", "&quot;" }, { "'", "&apos;" }, { "<", "&lt;" }, { ">", "&gt;" }, { "&", "&amp;" } }));
}

Value expr_unescapeXml(const std::vector<Value> &args) {
  return Value(utils::string::replaceMap(args[0].asString(), { { "&quot;", "\"" }, { "&apos;", "'" }, { "&lt;", "<" }, { "&gt;", ">" }, { "&amp;", "&" } }));
}

Value expr_escapeCsv(const std::vector<Value> &args) {
  auto result = args[0].asString();
  const std::array<char, 4> quote_req_chars = { '"', '\r', '\n', ',' };
  bool quote_required = false;

  for (const auto &c : quote_req_chars) {
    if (result.find(c) != std::string::npos) {
      quote_required = true;
      break;
    }
  }

  if (quote_required) {
    std::string quoted_result = "\"";
    quoted_result.append(utils::string::replaceMap(result, { { "\"", "\"\"" } }));
    quoted_result.append("\"");
    return Value(quoted_result);
  }

  return Value(result);
}

Value expr_format(const std::vector<Value> &args) {
  using std::chrono::milliseconds;

  date::sys_time<milliseconds> utc_time_point{milliseconds(args[0].asUnsignedLong())};
  auto format_string = args[1].asString();
  auto zone = args.size() > 2 ? date::locate_zone(args[2].asString()) : date::current_zone();

  auto zoned_time_point = date::make_zoned(zone, utc_time_point);
  std::ostringstream result_stream;
  result_stream << date::format(args[1].asString(), zoned_time_point);
  return Value(result_stream.str());
}

Value expr_toDate(const std::vector<Value> &args) {
  using std::chrono::milliseconds;
  auto input_string = args[0].asString();

  if (args.size() == 1) {
    if (auto parsed_rfc3339 = org::apache::nifi::minifi::utils::timeutils::parseRfc3339(input_string))
      return Value(int64_t{std::chrono::duration_cast<milliseconds>(parsed_rfc3339->time_since_epoch()).count()});
    else
      throw std::runtime_error(fmt::format("Failed to parse \"{}\" as an RFC3339 formatted datetime", input_string));
  }
  auto format_string = args[1].asString();
  auto zone = args.size() > 2 ? date::locate_zone(args[2].asString()) : date::current_zone();

  std::istringstream input_stream{ input_string };
  date::sys_time<milliseconds> time_point;
  date::from_stream(input_stream, format_string.c_str(), time_point);
  if (input_stream.fail() || (input_stream.peek() && !input_stream.eof()))
    throw std::runtime_error(fmt::format(R"(Failed to parse "{}", with "{}" format)", input_string, format_string));

  auto utc_zone = date::locate_zone("UTC");
  auto utc_time_point = date::make_zoned(utc_zone, time_point);
  auto zoned_time_point = date::make_zoned(zone, utc_time_point.get_local_time());
  return Value(int64_t{std::chrono::duration_cast<milliseconds>(zoned_time_point.get_sys_time().time_since_epoch()).count()});
}

Value expr_now(const std::vector<Value>& /*args*/) {
  using std::chrono::milliseconds;
  return Value(int64_t{std::chrono::duration_cast<milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()});
}

Value expr_unescapeCsv(const std::vector<Value> &args) {
  auto result = args[0].asString();

  if (result[0] == '"' && result[result.size() - 1] == '"') {
    bool quote_required = false;

    size_t quote_pos = result.find('"', 1);

    if (quote_pos != result.length() - 1) {
      quote_required = true;
    } else {
      const std::array<char, 3> quote_req_chars = { '\r', '\n', ',' };

      for (const auto &c : quote_req_chars) {
        if (result.find(c) != std::string::npos) {
          quote_required = true;
          break;
        }
      }
    }

    if (quote_required) {
      return Value(utils::string::replaceMap(result.substr(1, result.size() - 2), { { "\"\"", "\"" } }));
    }
  }

  return Value(result);
}

Value expr_urlEncode(const std::vector<Value> &args) {
  auto arg_0 = args[0].asString();
  CURL *curl = curl_easy_init();
  if (curl != nullptr) {
    char *output = curl_easy_escape(curl, arg_0.c_str(), static_cast<int>(arg_0.length()));
    if (output != nullptr) {
      auto result = std::string(output);
      curl_free(output);
      curl_easy_cleanup(curl);
      return Value(result);
    } else {
      curl_easy_cleanup(curl);
      throw std::runtime_error("cURL failed to encode URL string");
    }
  } else {
    throw std::runtime_error("Failed to initialize cURL");
  }
}

Value expr_urlDecode(const std::vector<Value> &args) {
  auto arg_0 = args[0].asString();
  CURL *curl = curl_easy_init();
  if (curl != nullptr) {
    int out_len = 0;
    char *output = curl_easy_unescape(curl, arg_0.c_str(), static_cast<int>(arg_0.length()), &out_len);
    if (output != nullptr) {
      auto result = std::string(output, static_cast<size_t>(out_len));
      curl_free(output);
      curl_easy_cleanup(curl);
      return Value(result);
    } else {
      curl_easy_cleanup(curl);
      throw std::runtime_error("cURL failed to decode URL string");
    }
  } else {
    throw std::runtime_error("Failed to initialize cURL");
  }
}

Value expr_base64Encode(const std::vector<Value> &args) {
  return Value(utils::string::to_base64(args[0].asString()));
}

Value expr_base64Decode(const std::vector<Value> &args) {
  return Value(utils::string::from_base64(args[0].asString(), utils::as_string));
}

Value expr_replace(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  const std::string &find = args[1].asString();
  const std::string &replace = args[2].asString();

  std::string::size_type match_pos = 0;
  match_pos = result.find(find, match_pos);

  while (match_pos != std::string::npos) {
    result.replace(match_pos, find.size(), replace);
    match_pos = result.find(find, match_pos + replace.size());
  }

  return Value(result);
}

Value expr_replaceFirst(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  const std::regex find(args[1].asString());
  const std::string &replace = args[2].asString();
  return Value(std::regex_replace(result, find, replace, std::regex_constants::format_first_only));
}

Value expr_replaceAll(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  const std::regex find(args[1].asString());
  const std::string &replace = args[2].asString();
  return Value(std::regex_replace(result, find, replace));
}

Value expr_replaceNull(const std::vector<Value> &args) {
  if (args[0].isNull()) {
    return args[1];
  } else {
    return args[0];
  }
}

Value expr_replaceEmpty(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  const std::regex find("^[ \n\r\t]*$");
  const std::string &replace = args[1].asString();
  return Value(std::regex_replace(result, find, replace));
}

Value expr_matches(const std::vector<Value> &args) {
  const auto &subject = args[0].asString();
  const auto expr = utils::Regex(args[1].asString());

  return Value(utils::regexMatch(subject, expr));
}

Value expr_find(const std::vector<Value> &args) {
  const auto &subject = args[0].asString();
  const auto expr = utils::Regex(args[1].asString());

  return Value(utils::regexSearch(subject, expr));
}

Value expr_trim(const std::vector<Value> &args) {
  return Value{utils::string::trim(args[0].asString())};
}

Value expr_append(const std::vector<Value> &args) {
  std::string result = args[0].asString();
  return Value(result.append(args[1].asString()));
}

Value expr_prepend(const std::vector<Value> &args) {
  std::string result = args[1].asString();
  return Value(result.append(args[0].asString()));
}

Value expr_length(const std::vector<Value> &args) {
  uint64_t len = args[0].asString().length();
  return Value(len);
}

Value expr_binary_op(const std::vector<Value> &args, long double (*ldop)(long double, long double), int64_t (*iop)(int64_t, int64_t), bool long_only = false) {
  try {
    if (!long_only && !args[0].isDecimal() && !args[1].isDecimal()) {
      return Value(iop(args[0].asSignedLong(), args[1].asSignedLong()));
    } else {
      return Value(ldop(args[0].asLongDouble(), args[1].asLongDouble()));
    }
  } catch (const std::exception &) {
    return {};
  }
}

Value expr_plus(const std::vector<Value> &args) {
  return expr_binary_op(args, [](long double a, long double b) {return a + b;}, [](int64_t a, int64_t b) {return a + b;});
}

Value expr_minus(const std::vector<Value> &args) {
  return expr_binary_op(args, [](long double a, long double b) {return a - b;}, [](int64_t a, int64_t b) {return a - b;});
}

Value expr_multiply(const std::vector<Value> &args) {
  return expr_binary_op(args, [](long double a, long double b) {return a * b;}, [](int64_t a, int64_t b) {return a * b;});
}

Value expr_divide(const std::vector<Value> &args) {
  return expr_binary_op(args, [](long double a, long double b) {return a / b;}, [](int64_t a, int64_t b) {return a / b;}, true);
}

Value expr_mod(const std::vector<Value> &args) {
  return expr_binary_op(args, [](long double a, long double b) {return std::fmod(a, b);}, [](int64_t a, int64_t b) {return a % b;});
}

Value expr_toRadix(const std::vector<Value> &args) {
  int64_t radix = args[1].asSignedLong();

  if (radix < 2 || radix > 36) {
    throw std::runtime_error("Cannot perform conversion due to invalid radix");
  }

  int pad_width = 0;

  if (args.size() > 2) {
    pad_width = static_cast<int>(args[2].asUnsignedLong());
  }

//  auto value = std::stoll(args[0].asString(), nullptr, 10);
  auto value = args[0].asSignedLong();

  std::string sign;

  if (value < 0) {
    sign = "-";
  }

  const std::array<char, 36> chars = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
    'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z' };
  std::string str_num;

  while (value) {
    str_num += chars[std::abs(value % radix)];
    value /= radix;
  }

  std::reverse(str_num.begin(), str_num.end());

  std::stringstream ss;
  ss << sign << std::setfill('0') << std::setw(pad_width) << str_num;
  return Value(ss.str());
}

Value expr_fromRadix(const std::vector<Value> &args) {
  int radix = gsl::narrow<int>(args[1].asSignedLong());

  if (radix < 2 || radix > 36) {
    throw std::runtime_error("Cannot perform conversion due to invalid radix");
  }

  return Value(std::to_string(std::stoll(args[0].asString(), nullptr, radix)));
}

Value expr_random(const std::vector<Value>& /*args*/) {
  std::random_device random_device;
  std::mt19937 generator(random_device());
  std::uniform_int_distribution<int64_t> distribution(0, LLONG_MAX);
  return Value(distribution(generator));
}

template<Value T(const std::vector<Value> &)>
Expression make_dynamic_function_incomplete(const std::string &function_name, const std::vector<Expression> &args, std::size_t num_args) {
  if (args.size() < num_args) {
    std::stringstream message_ss;
    message_ss << "Expression language function " << function_name << " called with " << args.size() << " argument(s), but " << num_args << " are required";
    throw std::runtime_error(message_ss.str());
  }

  if (!args.empty() && args[0].is_multi()) {
    std::vector<Expression> multi_args;

    for (auto it = std::next(args.begin()); it != args.end(); ++it) {
      multi_args.emplace_back(*it);
    }

    return args[0].compose_multi([=](const std::vector<Value> &args) -> Value {
      return T(args);
    },
                                 multi_args);
  } else {
    return make_dynamic([=](const Parameters &params, const std::vector<Expression>& /*sub_exprs*/) -> Value {
      std::vector<Value> evaluated_args;
      evaluated_args.reserve(args.size());
      for (const auto &arg : args) {
        evaluated_args.emplace_back(arg(params));
      }

      return T(evaluated_args);
    });
  }
}

Value expr_literal(const std::vector<Value> &args) {
  return args[0];
}

Value expr_isNull(const std::vector<Value> &args) {
  return Value(args[0].isNull());
}

Value expr_notNull(const std::vector<Value> &args) {
  return Value(!args[0].isNull());
}

Value expr_isEmpty(const std::vector<Value> &args) {
  if (args[0].isNull()) {
    return Value(true);
  }

  std::string arg_0 = args[0].asString();

  for (char c : arg_0) {
    if (c != ' ' && c != '\f' && c != '\n' && c != '\r' && c != '\t' && c != '\v') {
      return Value(false);
    }
  }

  return Value(true);
}

Value expr_equals(const std::vector<Value> &args) {
  return Value(args[0].asString() == args[1].asString());
}

Value expr_equalsIgnoreCase(const std::vector<Value> &args) {
  auto arg_0 = args[0].asString();
  auto arg_1 = args[1].asString();

  std::transform(arg_0.begin(), arg_0.end(), arg_0.begin(), ::tolower);
  std::transform(arg_1.begin(), arg_1.end(), arg_1.begin(), ::tolower);

  return Value(arg_0 == arg_1);
}

Value expr_gt(const std::vector<Value> &args) {
  if (args[0].isDecimal() && args[1].isDecimal()) {
    return Value(args[0].asLongDouble() > args[1].asLongDouble());
  } else {
    return Value(args[0].asSignedLong() > args[1].asSignedLong());
  }
}

Value expr_ge(const std::vector<Value> &args) {
  if (args[0].isDecimal() && args[1].isDecimal()) {
    return Value(args[0].asLongDouble() >= args[1].asLongDouble());
  } else {
    return Value(args[0].asSignedLong() >= args[1].asSignedLong());
  }
}

Value expr_lt(const std::vector<Value> &args) {
  if (args[0].isDecimal() && args[1].isDecimal()) {
    return Value(args[0].asLongDouble() < args[1].asLongDouble());
  } else {
    return Value(args[0].asSignedLong() < args[1].asSignedLong());
  }
}

Value expr_le(const std::vector<Value> &args) {
  if (args[0].isDecimal() && args[1].isDecimal()) {
    return Value(args[0].asLongDouble() <= args[1].asLongDouble());
  } else {
    return Value(args[0].asSignedLong() <= args[1].asSignedLong());
  }
}

Value expr_and(const std::vector<Value> &args) {
  return Value(args[0].asBoolean() && args[1].asBoolean());
}

Value expr_or(const std::vector<Value> &args) {
  return Value(args[0].asBoolean() || args[1].asBoolean());
}

Value expr_not(const std::vector<Value> &args) {
  return Value(!args[0].asBoolean());
}

Value expr_ifElse(const std::vector<Value> &args) {
  if (args[0].asBoolean()) {
    return args[1];
  } else {
    return args[2];
  }
}

Value expr_nextInt(const std::vector<Value>&) {
  static std::atomic<int64_t> counter{0};
  return Value(counter++);
}

Expression make_allAttributes(const std::string &function_name, const std::vector<Expression> &args) {
  if (args.empty()) {
    std::stringstream message_ss;
    message_ss << "Expression language function " << function_name << " called with " << args.size() << " argument(s), but " << 1 << " are required";
    throw std::runtime_error(message_ss.str());
  }

  auto result = make_dynamic([=](const Parameters &params, const std::vector<Expression> &sub_exprs) -> Value {
    std::vector<Value> evaluated_args;

    bool all_true = true;

    for (const auto &sub_expr : sub_exprs) {
      all_true = all_true && sub_expr(params).asBoolean();
    }

    return Value(all_true);
  });

  result.make_multi([=](const Parameters& /*params*/) -> std::vector<Expression> {
    std::vector<Expression> out_exprs;
    out_exprs.reserve(args.size());
    for (const auto &arg : args) {
      out_exprs.emplace_back(make_dynamic([=](const Parameters &params,
                  const std::vector<Expression>& /*sub_exprs*/) -> Value {
                std::string attr_id;
                attr_id = arg(params).asString();
                std::string attr_val;

                const auto cur_flow_file = params.flow_file;

                if (cur_flow_file && cur_flow_file->getAttribute(attr_id, attr_val)) {
                  return Value(attr_val);
                } else {
                  return {};
                }
              }));
    }

    return out_exprs;
  });

  return result;
}

Expression make_anyAttribute(const std::string &function_name, const std::vector<Expression> &args) {
  if (args.empty()) {
    std::stringstream message_ss;
    message_ss << "Expression language function " << function_name << " called with " << args.size() << " argument(s), but " << 1 << " are required";
    throw std::runtime_error(message_ss.str());
  }

  auto result = make_dynamic([=](const Parameters &params, const std::vector<Expression> &sub_exprs) -> Value {
    std::vector<Value> evaluated_args;

    bool any_true = false;

    for (const auto &sub_expr : sub_exprs) {
      any_true = any_true || sub_expr(params).asBoolean();
    }

    return Value(any_true);
  });

  result.make_multi([=](const Parameters& /*params*/) -> std::vector<Expression> {
    std::vector<Expression> out_exprs;
    out_exprs.reserve(args.size());
    for (const auto &arg : args) {
      out_exprs.emplace_back(make_dynamic([=](const Parameters &params,
                  const std::vector<Expression>& /*sub_exprs*/) -> Value {
                std::string attr_id;
                attr_id = arg(params).asString();
                std::string attr_val;

                const auto cur_flow_file = params.flow_file;

                if (cur_flow_file && cur_flow_file->getAttribute(attr_id, attr_val)) {
                  return Value(attr_val);
                } else {
                  return {};
                }
              }));
    }

    return out_exprs;
  });

  return result;
}

Expression make_allMatchingAttributes(const std::string &function_name, const std::vector<Expression> &args) {
  if (args.empty()) {
    std::stringstream message_ss;
    message_ss << "Expression language function " << function_name << " called with " << args.size() << " argument(s), but " << 1 << " are required";
    throw std::runtime_error(message_ss.str());
  }

  auto result = make_dynamic([=](const Parameters &params, const std::vector<Expression> &sub_exprs) -> Value {
    std::vector<Value> evaluated_args;

    bool all_true = !sub_exprs.empty();

    for (const auto &sub_expr : sub_exprs) {
      all_true = all_true && sub_expr(params).asBoolean();
    }

    return Value(all_true);
  });

  result.make_multi([=](const Parameters &params) -> std::vector<Expression> {
    std::vector<Expression> out_exprs;

    for (const auto &arg : args) {
      const auto attr_regex = utils::Regex(arg(params).asString());
      const auto cur_flow_file = params.flow_file;
      std::map<std::string, std::string> attrs;

      if (cur_flow_file) {
        attrs = cur_flow_file->getAttributes();
      }

      for (const auto &attr : attrs) {
        if (utils::regexMatch(attr.first, attr_regex)) {
          out_exprs.emplace_back(make_dynamic([=](const Parameters& /*params*/,
                      const std::vector<Expression>& /*sub_exprs*/) -> Value {
                    std::string attr_val;

                    if (cur_flow_file && cur_flow_file->getAttribute(attr.first, attr_val)) {
                      return Value(attr_val);
                    } else {
                      return {};
                    }
                  }));
        }
      }
    }

    return out_exprs;
  });

  return result;
}

Expression make_anyMatchingAttribute(const std::string &function_name, const std::vector<Expression> &args) {
  if (args.empty()) {
    std::stringstream message_ss;
    message_ss << "Expression language function " << function_name << " called with " << args.size() << " argument(s), but " << 1 << " are required";
    throw std::runtime_error(message_ss.str());
  }

  auto result = make_dynamic([=](const Parameters &params, const std::vector<Expression> &sub_exprs) -> Value {
    std::vector<Value> evaluated_args;

    bool any_true = false;

    for (const auto &sub_expr : sub_exprs) {
      any_true = any_true || sub_expr(params).asBoolean();
    }

    return Value(any_true);
  });

  result.make_multi([=](const Parameters &params) -> std::vector<Expression> {
    std::vector<Expression> out_exprs;

    for (const auto &arg : args) {
      const auto attr_regex = utils::Regex(arg(params).asString());
      const auto cur_flow_file = params.flow_file;
      std::map<std::string, std::string> attrs;

      if (cur_flow_file) {
        attrs = cur_flow_file->getAttributes();
      }

      for (const auto &attr : attrs) {
        if (utils::regexMatch(attr.first, attr_regex)) {
          out_exprs.emplace_back(make_dynamic([=](const Parameters& /*params*/,
                      const std::vector<Expression>& /*sub_exprs*/) -> Value {
                    std::string attr_val;

                    if (cur_flow_file && cur_flow_file->getAttribute(attr.first, attr_val)) {
                      return Value(attr_val);
                    } else {
                      return {};
                    }
                  }));
        }
      }
    }

    return out_exprs;
  });

  return result;
}

Expression make_allDelineatedValues(const std::string &function_name, const std::vector<Expression> &args) {
  if (args.size() != 2) {
    std::stringstream message_ss;
    message_ss << "Expression language function " << function_name << " called with " << args.size() << " argument(s), but " << 2 << " are required";
    throw std::runtime_error(message_ss.str());
  }

  auto result = make_dynamic([=](const Parameters &params, const std::vector<Expression> &sub_exprs) -> Value {
    std::vector<Value> evaluated_args;

    bool all_true = !sub_exprs.empty();

    for (const auto &sub_expr : sub_exprs) {
      all_true = all_true && sub_expr(params).asBoolean();
    }

    return Value(all_true);
  });

  result.make_multi([=](const Parameters &params) -> std::vector<Expression> {
    std::vector<Expression> out_exprs;

    for (const auto &val : utils::string::split(args[0](params).asString(), args[1](params).asString())) {
      out_exprs.emplace_back(make_static(val));
    }

    return out_exprs;
  });

  return result;
}

Expression make_anyDelineatedValue(const std::string &function_name, const std::vector<Expression> &args) {
  if (args.size() != 2) {
    std::stringstream message_ss;
    message_ss << "Expression language function " << function_name << " called with " << args.size() << " argument(s), but " << 2 << " are required";
    throw std::runtime_error(message_ss.str());
  }

  auto result = make_dynamic([=](const Parameters &params, const std::vector<Expression> &sub_exprs) -> Value {
    std::vector<Value> evaluated_args;

    bool any_true = false;

    for (const auto &sub_expr : sub_exprs) {
      any_true = any_true || sub_expr(params).asBoolean();
    }

    return Value(any_true);
  });

  result.make_multi([=](const Parameters &params) -> std::vector<Expression> {
    std::vector<Expression> out_exprs;

    for (const auto &val : utils::string::split(args[0](params).asString(), args[1](params).asString())) {
      out_exprs.emplace_back(make_static(val));
    }

    return out_exprs;
  });

  return result;
}

Expression make_count(const std::string &function_name, const std::vector<Expression> &args) {
  if (args.size() != 1) {
    std::stringstream message_ss;
    message_ss << "Expression language function " << function_name << " called with " << args.size() << " argument(s), but " << 1 << " are required";
    throw std::runtime_error(message_ss.str());
  }

  if (!args[0].is_multi()) {
    std::stringstream message_ss;
    message_ss << "Expression language function " << function_name << " called against singular expression.";
    throw std::runtime_error(message_ss.str());
  }

  return args[0].make_aggregate([](const Parameters &params,
      const std::vector<Expression> &sub_exprs) -> Value {
    uint64_t count = 0;
    for (const auto &sub_expr : sub_exprs) {
      if (sub_expr(params).asBoolean()) {
        count++;
      }
    }
    return Value(count);
  });
}

Expression make_join(const std::string &function_name, const std::vector<Expression> &args) {
  if (args.size() != 2) {
    std::stringstream message_ss;
    message_ss << "Expression language function " << function_name << " called with " << args.size() << " argument(s), but " << 2 << " are required";
    throw std::runtime_error(message_ss.str());
  }

  if (!args[0].is_multi()) {
    std::stringstream message_ss;
    message_ss << "Expression language function " << function_name << " called against singular expression.";
    throw std::runtime_error(message_ss.str());
  }

  const auto& delim_expr = args[1];

  return args[0].make_aggregate([delim_expr](const Parameters &params,
      const std::vector<Expression> &sub_exprs) -> Value {
    std::string delim = delim_expr(params).asString();
    std::stringstream out_ss;
    bool first = true;

    for (const auto &sub_expr : sub_exprs) {
      if (!first) {
        out_ss << delim;
      }
      out_ss << sub_expr(params).asString();
      first = false;
    }

    return Value(out_ss.str());
  });
}

Expression make_dynamic_function(const std::string &function_name, const std::vector<Expression> &args) {
  if (function_name == "hostname") {
    return make_dynamic_function_incomplete<expr_hostname>(function_name, args, 0);
  } else if (function_name == "resolve_user_id") {
    return make_dynamic_function_incomplete<resolve_user_id>(function_name, args, 0);
  } else if (function_name == "ip") {
    return make_dynamic_function_incomplete<expr_ip>(function_name, args, 0);
  } else if (function_name == "reverseDnsLookup") {
    return make_dynamic_function_incomplete<expr_reverseDnsLookup>(function_name, args, 1);
  } else if (function_name == "UUID") {
    return make_dynamic_function_incomplete<expr_uuid>(function_name, args, 0);
  } else if (function_name == "toUpper") {
    return make_dynamic_function_incomplete<expr_toUpper>(function_name, args, 1);
  } else if (function_name == "toLower") {
    return make_dynamic_function_incomplete<expr_toLower>(function_name, args, 1);
  } else if (function_name == "substring") {
    return make_dynamic_function_incomplete<expr_substring>(function_name, args, 2);
  } else if (function_name == "substringBefore") {
    return make_dynamic_function_incomplete<expr_substringBefore>(function_name, args, 2);
  } else if (function_name == "substringBeforeLast") {
    return make_dynamic_function_incomplete<expr_substringBeforeLast>(function_name, args, 2);
  } else if (function_name == "substringAfter") {
    return make_dynamic_function_incomplete<expr_substringAfter>(function_name, args, 2);
  } else if (function_name == "substringAfterLast") {
    return make_dynamic_function_incomplete<expr_substringAfterLast>(function_name, args, 2);
  } else if (function_name == "getDelimitedField") {
    return make_dynamic_function_incomplete<expr_getDelimitedField>(function_name, args, 2);
  } else if (function_name == "startsWith") {
    return make_dynamic_function_incomplete<expr_startsWith>(function_name, args, 1);
  } else if (function_name == "endsWith") {
    return make_dynamic_function_incomplete<expr_endsWith>(function_name, args, 1);
  } else if (function_name == "contains") {
    return make_dynamic_function_incomplete<expr_contains>(function_name, args, 1);
  } else if (function_name == "in") {
    return make_dynamic_function_incomplete<expr_in>(function_name, args, 1);
  } else if (function_name == "indexOf") {
    return make_dynamic_function_incomplete<expr_indexOf>(function_name, args, 1);
  } else if (function_name == "lastIndexOf") {
    return make_dynamic_function_incomplete<expr_lastIndexOf>(function_name, args, 1);
  } else if (function_name == "escapeJson") {
    return make_dynamic_function_incomplete<expr_escapeJson>(function_name, args, 0);
  } else if (function_name == "unescapeJson") {
    return make_dynamic_function_incomplete<expr_unescapeJson>(function_name, args, 0);
  } else if (function_name == "escapeXml") {
    return make_dynamic_function_incomplete<expr_escapeXml>(function_name, args, 0);
  } else if (function_name == "unescapeXml") {
    return make_dynamic_function_incomplete<expr_unescapeXml>(function_name, args, 0);
  } else if (function_name == "escapeHtml3") {
    return make_dynamic_function_incomplete<expr_escapeHtml3>(function_name, args, 0);
  } else if (function_name == "unescapeHtml3") {
    return make_dynamic_function_incomplete<expr_unescapeHtml3>(function_name, args, 0);
  } else if (function_name == "escapeHtml4") {
    return make_dynamic_function_incomplete<expr_escapeHtml4>(function_name, args, 0);
  } else if (function_name == "unescapeHtml4") {
    return make_dynamic_function_incomplete<expr_unescapeHtml4>(function_name, args, 0);
  } else if (function_name == "escapeCsv") {
    return make_dynamic_function_incomplete<expr_escapeCsv>(function_name, args, 0);
  } else if (function_name == "unescapeCsv") {
    return make_dynamic_function_incomplete<expr_unescapeCsv>(function_name, args, 0);
  } else if (function_name == "urlEncode") {
    return make_dynamic_function_incomplete<expr_urlEncode>(function_name, args, 0);
  } else if (function_name == "urlDecode") {
    return make_dynamic_function_incomplete<expr_urlDecode>(function_name, args, 0);
  } else if (function_name == "base64Encode") {
    return make_dynamic_function_incomplete<expr_base64Encode>(function_name, args, 0);
  } else if (function_name == "base64Decode") {
    return make_dynamic_function_incomplete<expr_base64Decode>(function_name, args, 0);
  } else if (function_name == "replace") {
    return make_dynamic_function_incomplete<expr_replace>(function_name, args, 2);
  } else if (function_name == "replaceFirst") {
    return make_dynamic_function_incomplete<expr_replaceFirst>(function_name, args, 2);
  } else if (function_name == "replaceAll") {
    return make_dynamic_function_incomplete<expr_replaceAll>(function_name, args, 2);
  } else if (function_name == "replaceNull") {
    return make_dynamic_function_incomplete<expr_replaceNull>(function_name, args, 1);
  } else if (function_name == "replaceEmpty") {
    return make_dynamic_function_incomplete<expr_replaceEmpty>(function_name, args, 1);
  } else if (function_name == "matches") {
    return make_dynamic_function_incomplete<expr_matches>(function_name, args, 1);
  } else if (function_name == "find") {
    return make_dynamic_function_incomplete<expr_find>(function_name, args, 1);
  } else if (function_name == "allMatchingAttributes") {
    return make_allMatchingAttributes(function_name, args);
  } else if (function_name == "anyMatchingAttribute") {
    return make_anyMatchingAttribute(function_name, args);
  } else if (function_name == "trim") {
    return make_dynamic_function_incomplete<expr_trim>(function_name, args, 0);
  } else if (function_name == "append") {
    return make_dynamic_function_incomplete<expr_append>(function_name, args, 1);
  } else if (function_name == "prepend") {
    return make_dynamic_function_incomplete<expr_prepend>(function_name, args, 1);
  } else if (function_name == "length") {
    return make_dynamic_function_incomplete<expr_length>(function_name, args, 0);
  } else if (function_name == "plus") {
    return make_dynamic_function_incomplete<expr_plus>(function_name, args, 1);
  } else if (function_name == "minus") {
    return make_dynamic_function_incomplete<expr_minus>(function_name, args, 1);
  } else if (function_name == "multiply") {
    return make_dynamic_function_incomplete<expr_multiply>(function_name, args, 1);
  } else if (function_name == "divide") {
    return make_dynamic_function_incomplete<expr_divide>(function_name, args, 1);
  } else if (function_name == "mod") {
    return make_dynamic_function_incomplete<expr_mod>(function_name, args, 1);
  } else if (function_name == "fromRadix") {
    return make_dynamic_function_incomplete<expr_fromRadix>(function_name, args, 2);
  } else if (function_name == "toRadix") {
    return make_dynamic_function_incomplete<expr_toRadix>(function_name, args, 1);
  } else if (function_name == "random") {
    return make_dynamic_function_incomplete<expr_random>(function_name, args, 0);
  } else if (function_name == "literal") {
    return make_dynamic_function_incomplete<expr_literal>(function_name, args, 1);
  } else if (function_name == "isNull") {
    return make_dynamic_function_incomplete<expr_isNull>(function_name, args, 0);
  } else if (function_name == "notNull") {
    return make_dynamic_function_incomplete<expr_notNull>(function_name, args, 0);
  } else if (function_name == "isEmpty") {
    return make_dynamic_function_incomplete<expr_isEmpty>(function_name, args, 0);
  } else if (function_name == "equals") {
    return make_dynamic_function_incomplete<expr_equals>(function_name, args, 1);
  } else if (function_name == "equalsIgnoreCase") {
    return make_dynamic_function_incomplete<expr_equalsIgnoreCase>(function_name, args, 1);
  } else if (function_name == "gt") {
    return make_dynamic_function_incomplete<expr_gt>(function_name, args, 1);
  } else if (function_name == "ge") {
    return make_dynamic_function_incomplete<expr_ge>(function_name, args, 1);
  } else if (function_name == "lt") {
    return make_dynamic_function_incomplete<expr_lt>(function_name, args, 1);
  } else if (function_name == "le") {
    return make_dynamic_function_incomplete<expr_le>(function_name, args, 1);
  } else if (function_name == "and") {
    return make_dynamic_function_incomplete<expr_and>(function_name, args, 1);
  } else if (function_name == "or") {
    return make_dynamic_function_incomplete<expr_or>(function_name, args, 1);
  } else if (function_name == "not") {
    return make_dynamic_function_incomplete<expr_not>(function_name, args, 0);
  } else if (function_name == "ifElse") {
    return make_dynamic_function_incomplete<expr_ifElse>(function_name, args, 2);
  } else if (function_name == "allAttributes") {
    return make_allAttributes(function_name, args);
  } else if (function_name == "anyAttribute") {
    return make_anyAttribute(function_name, args);
  } else if (function_name == "allDelineatedValues") {
    return make_allDelineatedValues(function_name, args);
  } else if (function_name == "anyDelineatedValue") {
    return make_anyDelineatedValue(function_name, args);
  } else if (function_name == "count") {
    return make_count(function_name, args);
  } else if (function_name == "join") {
    return make_join(function_name, args);
  } else if (function_name == "format") {
    return make_dynamic_function_incomplete<expr_format>(function_name, args, 1);
  } else if (function_name == "toDate") {
    return make_dynamic_function_incomplete<expr_toDate>(function_name, args, 0);
  } else if (function_name == "now") {
    return make_dynamic_function_incomplete<expr_now>(function_name, args, 0);
  } else if (function_name == "nextInt") {
    return make_dynamic_function_incomplete<expr_nextInt>(function_name, args, 0);
  } else {
    std::string msg("Unknown expression function: ");
    msg.append(function_name);
    throw std::runtime_error(msg);
  }
}

Expression make_function_composition(const Expression &arg, const std::vector<std::pair<std::string, std::vector<Expression>>> &chain) {
  auto expr = arg;

  for (const auto &chain_part : chain) {
    std::vector<Expression> complete_args = { expr };
    complete_args.insert(complete_args.end(), chain_part.second.begin(), chain_part.second.end());
    expr = make_dynamic_function(chain_part.first, complete_args);
  }

  return expr;
}

bool Expression::is_dynamic() const {
  return static_cast<bool>(val_fn_);
}

Expression Expression::operator+(const Expression &other_expr) const {
  if (is_dynamic() && other_expr.is_dynamic()) {
    auto val_fn = val_fn_;
    auto other_val_fn = other_expr.val_fn_;
    auto sub_expr_generator = sub_expr_generator_;
    auto other_sub_expr_generator = other_expr.sub_expr_generator_;
    return make_dynamic([val_fn,
    other_val_fn,
    sub_expr_generator,
    other_sub_expr_generator](const Parameters &params,
        const std::vector<Expression>& /*sub_exprs*/) -> Value {
      Value result = val_fn(params, sub_expr_generator(params));
      return Value(result.asString().append(other_val_fn(params, other_sub_expr_generator(params)).asString()));
    });
  } else if (is_dynamic() && !other_expr.is_dynamic()) {
    auto val_fn = val_fn_;
    auto other_val = other_expr.val_;
    auto sub_expr_generator = sub_expr_generator_;
    return make_dynamic([val_fn,
    other_val,
    sub_expr_generator](const Parameters &params,
        const std::vector<Expression>& /*sub_exprs*/) -> Value {
      Value result = val_fn(params, sub_expr_generator(params));
      return Value(result.asString().append(other_val.asString()));
    });
  } else if (!is_dynamic() && other_expr.is_dynamic()) {
    auto val = val_;
    auto other_val_fn = other_expr.val_fn_;
    auto other_sub_expr_generator = other_expr.sub_expr_generator_;
    return make_dynamic([val,
    other_val_fn,
    other_sub_expr_generator](const Parameters &params,
        const std::vector<Expression>& /*sub_exprs*/) -> Value {
      return Value(val.asString().append(other_val_fn(params, other_sub_expr_generator(params)).asString()));
    });
  } else if (!is_dynamic() && !other_expr.is_dynamic()) {
    std::string result(val_.asString());
    result.append(other_expr.val_.asString());
    return make_static(result);
  } else {
    throw std::runtime_error("Invalid function composition");
  }
}

Value Expression::operator()(const Parameters &params) const {
  if (is_dynamic()) {
    return val_fn_(params, sub_expr_generator_(params));
  } else {
    return val_;
  }
}

Expression Expression::compose_multi(const std::function<Value(const std::vector<Value> &)>& fn, const std::vector<Expression> &args) const {
  auto result = make_dynamic(val_fn_);
  auto compose_expr_generator = sub_expr_generator_;

  result.sub_expr_generator_ = [=](const Parameters &params) -> std::vector<Expression> {
    auto sub_exprs = compose_expr_generator(params);
    std::vector<Expression> out_exprs;
    out_exprs.reserve(sub_exprs.size());
    for (const auto &sub_expr : sub_exprs) {
      out_exprs.emplace_back(make_dynamic([=](const Parameters &params,
                  const std::vector<Expression>& /*sub_exprs*/) {
                std::vector<Value> evaluated_args;

                evaluated_args.emplace_back(sub_expr(params));

                for (const auto &arg : args) {
                  evaluated_args.emplace_back(arg(params));
                }

                return fn(evaluated_args);
              }));
    }
    return out_exprs;
  };

  result.is_multi_ = true;

  return result;
}

Expression Expression::make_aggregate(const std::function<Value(const Parameters &params, const std::vector<Expression> &sub_exprs)>& val_fn) const {
  auto sub_expr_generator = sub_expr_generator_;
  return make_dynamic([sub_expr_generator,
  val_fn](const Parameters &params,
      const std::vector<Expression>& /*sub_exprs*/) -> Value {
    return val_fn(params, sub_expr_generator(params));
  });
}

#ifdef WIN32
void dateSetInstall(const std::string& install) {
  utils::timeutils::dateSetInstall(install);
  date::set_install(install);
}
#endif

}  // namespace org::apache::nifi::minifi::expression
