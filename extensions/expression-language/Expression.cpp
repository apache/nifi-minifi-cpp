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

#include <utility>
#include <iostream>
#include <iomanip>
#include <random>
#include <algorithm>

#include "rapidjson/reader.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"

#include <utils/StringUtils.h>
#include <expression/Expression.h>
#include <regex>
#include <curl/curl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "Driver.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace expression {

Expression compile(const std::string &expr_str) {
  std::stringstream expr_str_stream(expr_str);
  Driver driver(&expr_str_stream);
  Parser parser(&driver);
  parser.parse();
  return driver.result;
}

Expression make_static(std::string val) {
  return Expression(Value(val));
}

Expression make_dynamic(const std::function<Value(const Parameters &params)> &val_fn) {
  return Expression(Value(), val_fn);
}

Expression make_dynamic_attr(const std::string &attribute_id) {
  return make_dynamic([attribute_id](const Parameters &params) -> Value {
    std::string result;
    if (params.flow_file.lock()->getAttribute(attribute_id, result)) {
      return Value(result);
    } else {
      return Value();
    }
  });
}

Value expr_hostname(const std::vector<Value> &args) {
  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);

  if (args.size() > 0 && args[0].asBoolean()) {
    int status;
    struct addrinfo hints, *result, *addr_cursor;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    status = getaddrinfo(hostname, nullptr, &hints, &result);

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

  return Value(std::string(hostname));
}

Value expr_ip(const std::vector<Value> &args) {
  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);

  int status;
  char ip_str[INET6_ADDRSTRLEN];
  struct sockaddr_in *addr;
  struct addrinfo hints, *result, *addr_cursor;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;

  status = getaddrinfo(hostname, nullptr, &hints, &result);

  if (status) {
    std::string message("Failed to resolve local hostname to discover IP: ");
    message.append(gai_strerror(status));
    throw std::runtime_error(message);
  }

  for (addr_cursor = result; addr_cursor != nullptr; addr_cursor = addr_cursor->ai_next) {
    if (addr_cursor->ai_family == AF_INET) {
      addr = reinterpret_cast<struct sockaddr_in *>(addr_cursor->ai_addr);
      inet_ntop(addr_cursor->ai_family, &(addr->sin_addr), ip_str, sizeof(ip_str));
      freeaddrinfo(result);
      return Value(std::string(ip_str));
    }
  }

  freeaddrinfo(result);
  return Value();
}

Value expr_uuid(const std::vector<Value> &args) {
  uuid_t uuid;
  utils::IdGenerator::getIdGenerator()->generate(uuid);
  char uuid_str[37];
  uuid_unparse_lower(uuid, uuid_str);
  return Value(std::string(uuid_str));
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
    return Value(args[0].asString().substr(args[1].asUnsignedLong()));
  } else {
    return Value(args[0].asString().substr(args[1].asUnsignedLong(), args[2].asUnsignedLong()));
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
    return Value(static_cast<int64_t >(-1));
  } else {
    return Value(static_cast<int64_t >(pos));
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
    return Value(static_cast<int64_t >(-1));
  } else {
    return Value(static_cast<int64_t >(pos));
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
    return Value();
  }
}

Value expr_escapeHtml3(const std::vector<Value> &args) {
  return Value(utils::StringUtils::replaceMap(
      args[0].asString(),
      {
          {"!", "&excl;"},
          {"\"", "&quot;"},
          {"#", "&num;"},
          {"$", "&dollar;"},
          {"%", "&percnt;"},
          {"&", "&amp;"},
          {"'", "&apos;"},
          {"(", "&lpar;"},
          {")", "&rpar;"},
          {"*", "&ast;"},
          {"+", "&plus;"},
          {",", "&comma;"},
          {"-", "&minus;"},
          {".", "&period;"},
          {"/", "&sol;"},
          {":", "&colon;"},
          {";", "&semi;"},
          {"<", "&lt;"},
          {"=", "&equals;"},
          {">", "&gt;"},
          {"?", "&quest;"},
          {"@", "&commat;"},
          {"[", "&lsqb;"},
          {"\\", "&bsol;"},
          {"]", "&rsqb;"},
          {"^", "&circ;"},
          {"_", "&lowbar;"},
          {"`", "&grave;"},
          {"{", "&lcub;"},
          {"|", "&verbar;"},
          {"}", "&rcub;"},
          {"~", "&tilde;"},
          {"¡", "&iexcl;"},
          {"¢", "&cent;"},
          {"£", "&pound;"},
          {"¤", "&curren;"},
          {"¥", "&yen;"},
          {"¦", "&brkbar;"},
          {"§", "&sect;"},
          {"¨", "&uml;"},
          {"©", "&copy;"},
          {"ª", "&ordf;"},
          {"«", "&laquo;"},
          {"¬", "&not;"},
          {"®", "&reg;"},
          {"¯", "&macr;"},
          {"°", "&deg;"},
          {"±", "&plusmn;"},
          {"²", "&sup2;"},
          {"³", "&sup3;"},
          {"´", "&acute;"},
          {"µ", "&micro;"},
          {"¶", "&para;"},
          {"·", "&middot;"},
          {"¸", "&cedil;"},
          {"¹", "&sup1;"},
          {"º", "&ordm;"},
          {"»", "&raquo;;"},
          {"¼", "&frac14;"},
          {"½", "&frac12;"},
          {"¾", "&frac34;"},
          {"¿", "&iquest;"},
          {"À", "&Agrave;"},
          {"Á", "&Aacute;"},
          {"Â", "&Acirc;"},
          {"Ã", "&Atilde;"},
          {"Ä", "&Auml;"},
          {"Å", "&Aring;"},
          {"Æ", "&AElig;"},
          {"Ç", "&Ccedil;"},
          {"È", "&Egrave;"},
          {"É", "&Eacute;"},
          {"Ê", "&Ecirc;"},
          {"Ë", "&Euml;"},
          {"Ì", "&Igrave;"},
          {"Í", "&Iacute;"},
          {"Î", "&Icirc;"},
          {"Ï", "&Iuml;"},
          {"Ð", "&ETH;"},
          {"Ñ", "&Ntilde;"},
          {"Ò", "&Ograve;"},
          {"Ó", "&Oacute;"},
          {"Ô", "&Ocirc;"},
          {"Õ", "&Otilde;"},
          {"Ö", "&Ouml;"},
          {"×", "&times;"},
          {"Ø", "&Oslash;"},
          {"Ù", "&Ugrave;;"},
          {"Ú", "&Uacute;"},
          {"Û", "&Ucirc;"},
          {"Ü", "&Uuml;"},
          {"Ý", "&Yacute;"},
          {"Þ", "&THORN;"},
          {"ß", "&szlig;"},
          {"à", "&agrave;"},
          {"á", "&aacute;"},
          {"â", "&acirc;"},
          {"ã", "&atilde;"},
          {"ä", "&auml;"},
          {"å", "&aring;"},
          {"æ", "&aelig;"},
          {"ç", "&ccedil;"},
          {"è", "&egrave;"},
          {"é", "&eacute;"},
          {"ê", "&ecirc;"},
          {"ë", "&euml;"},
          {"ì", "&igrave;"},
          {"í", "&iacute;"},
          {"î", "&icirc;"},
          {"ï", "&iuml;"},
          {"ð", "&eth;"},
          {"ñ", "&ntilde;"},
          {"ò", "&ograve;"},
          {"ó", "&oacute;"},
          {"ô", "&ocirc;"},
          {"õ", "&otilde;"},
          {"ö", "&ouml;"},
          {"÷", "&divide;"},
          {"ø", "&oslash;"},
          {"ù", "&ugrave;"},
          {"ú", "&uacute;"},
          {"û", "&ucirc;"},
          {"ü", "&uuml;"},
          {"ý", "&yacute;"},
          {"þ", "&thorn;"},
          {"ÿ", "&yuml;"}
      }));
}

Value expr_escapeHtml4(const std::vector<Value> &args) {
  return Value(utils::StringUtils::replaceMap(
      args[0].asString(),
      {
          {"!", "&excl;"},
          {"\"", "&quot;"},
          {"#", "&num;"},
          {"$", "&dollar;"},
          {"%", "&percnt;"},
          {"&", "&amp;"},
          {"'", "&apos;"},
          {"(", "&lpar;"},
          {")", "&rpar;"},
          {"*", "&ast;"},
          {"+", "&plus;"},
          {",", "&comma;"},
          {"-", "&minus;"},
          {".", "&period;"},
          {"/", "&sol;"},
          {":", "&colon;"},
          {";", "&semi;"},
          {"<", "&lt;"},
          {"=", "&equals;"},
          {">", "&gt;"},
          {"?", "&quest;"},
          {"@", "&commat;"},
          {"[", "&lsqb;"},
          {"\\", "&bsol;"},
          {"]", "&rsqb;"},
          {"^", "&circ;"},
          {"_", "&lowbar;"},
          {"`", "&grave;"},
          {"{", "&lcub;"},
          {"|", "&verbar;"},
          {"}", "&rcub;"},
          {"~", "&tilde;"},
          {"¡", "&iexcl;"},
          {"¢", "&cent;"},
          {"£", "&pound;"},
          {"¤", "&curren;"},
          {"¥", "&yen;"},
          {"¦", "&brkbar;"},
          {"§", "&sect;"},
          {"¨", "&uml;"},
          {"©", "&copy;"},
          {"ª", "&ordf;"},
          {"«", "&laquo;"},
          {"¬", "&not;"},
          {"®", "&reg;"},
          {"¯", "&macr;"},
          {"°", "&deg;"},
          {"±", "&plusmn;"},
          {"²", "&sup2;"},
          {"³", "&sup3;"},
          {"´", "&acute;"},
          {"µ", "&micro;"},
          {"¶", "&para;"},
          {"·", "&middot;"},
          {"¸", "&cedil;"},
          {"¹", "&sup1;"},
          {"º", "&ordm;"},
          {"»", "&raquo;;"},
          {"¼", "&frac14;"},
          {"½", "&frac12;"},
          {"¾", "&frac34;"},
          {"¿", "&iquest;"},
          {"À", "&Agrave;"},
          {"Á", "&Aacute;"},
          {"Â", "&Acirc;"},
          {"Ã", "&Atilde;"},
          {"Ä", "&Auml;"},
          {"Å", "&Aring;"},
          {"Æ", "&AElig;"},
          {"Ç", "&Ccedil;"},
          {"È", "&Egrave;"},
          {"É", "&Eacute;"},
          {"Ê", "&Ecirc;"},
          {"Ë", "&Euml;"},
          {"Ì", "&Igrave;"},
          {"Í", "&Iacute;"},
          {"Î", "&Icirc;"},
          {"Ï", "&Iuml;"},
          {"Ð", "&ETH;"},
          {"Ñ", "&Ntilde;"},
          {"Ò", "&Ograve;"},
          {"Ó", "&Oacute;"},
          {"Ô", "&Ocirc;"},
          {"Õ", "&Otilde;"},
          {"Ö", "&Ouml;"},
          {"×", "&times;"},
          {"Ø", "&Oslash;"},
          {"Ù", "&Ugrave;;"},
          {"Ú", "&Uacute;"},
          {"Û", "&Ucirc;"},
          {"Ü", "&Uuml;"},
          {"Ý", "&Yacute;"},
          {"Þ", "&THORN;"},
          {"ß", "&szlig;"},
          {"à", "&agrave;"},
          {"á", "&aacute;"},
          {"â", "&acirc;"},
          {"ã", "&atilde;"},
          {"ä", "&auml;"},
          {"å", "&aring;"},
          {"æ", "&aelig;"},
          {"ç", "&ccedil;"},
          {"è", "&egrave;"},
          {"é", "&eacute;"},
          {"ê", "&ecirc;"},
          {"ë", "&euml;"},
          {"ì", "&igrave;"},
          {"í", "&iacute;"},
          {"î", "&icirc;"},
          {"ï", "&iuml;"},
          {"ð", "&eth;"},
          {"ñ", "&ntilde;"},
          {"ò", "&ograve;"},
          {"ó", "&oacute;"},
          {"ô", "&ocirc;"},
          {"õ", "&otilde;"},
          {"ö", "&ouml;"},
          {"÷", "&divide;"},
          {"ø", "&oslash;"},
          {"ù", "&ugrave;"},
          {"ú", "&uacute;"},
          {"û", "&ucirc;"},
          {"ü", "&uuml;"},
          {"ý", "&yacute;"},
          {"þ", "&thorn;"},
          {"ÿ", "&yuml;"},
          {"\u0192", "&fnof;"},
          {"\u0391", "&Alpha;"},
          {"\u0392", "&Beta;"},
          {"\u0393", "&Gamma;"},
          {"\u0394", "&Delta;"},
          {"\u0395", "&Epsilon;"},
          {"\u0396", "&Zeta;"},
          {"\u0397", "&Eta;"},
          {"\u0398", "&Theta;"},
          {"\u0399", "&Iota;"},
          {"\u039A", "&Kappa;"},
          {"\u039B", "&Lambda;"},
          {"\u039C", "&Mu;"},
          {"\u039D", "&Nu;"},
          {"\u039E", "&Xi;"},
          {"\u039F", "&Omicron;"},
          {"\u03A0", "&Pi;"},
          {"\u03A1", "&Rho;"},
          {"\u03A3", "&Sigma;"},
          {"\u03A4", "&Tau;"},
          {"\u03A5", "&Upsilon;"},
          {"\u03A6", "&Phi;"},
          {"\u03A7", "&Chi;"},
          {"\u03A8", "&Psi;"},
          {"\u03A9", "&Omega;"},
          {"\u03B1", "&alpha;"},
          {"\u03B2", "&beta;"},
          {"\u03B3", "&gamma;"},
          {"\u03B4", "&delta;"},
          {"\u03B5", "&epsilon;"},
          {"\u03B6", "&zeta;"},
          {"\u03B7", "&eta;"},
          {"\u03B8", "&theta;"},
          {"\u03B9", "&iota;"},
          {"\u03BA", "&kappa;"},
          {"\u03BB", "&lambda;"},
          {"\u03BC", "&mu;"},
          {"\u03BD", "&nu;"},
          {"\u03BE", "&xi;"},
          {"\u03BF", "&omicron;"},
          {"\u03C0", "&pi;"},
          {"\u03C1", "&rho;"},
          {"\u03C2", "&sigmaf;"},
          {"\u03C3", "&sigma;"},
          {"\u03C4", "&tau;"},
          {"\u03C5", "&upsilon;"},
          {"\u03C6", "&phi;"},
          {"\u03C7", "&chi;"},
          {"\u03C8", "&psi;"},
          {"\u03C9", "&omega;"},
          {"\u03D1", "&thetasym;"},
          {"\u03D2", "&upsih;"},
          {"\u03D6", "&piv;"},
          {"\u2022", "&bull;"},
          {"\u2026", "&hellip;"},
          {"\u2032", "&prime;"},
          {"\u2033", "&Prime;"},
          {"\u203E", "&oline;"},
          {"\u2044", "&frasl;"},
          {"\u2118", "&weierp;"},
          {"\u2111", "&image;"},
          {"\u211C", "&real;"},
          {"\u2122", "&trade;"},
          {"\u2135", "&alefsym;"},
          {"\u2190", "&larr;"},
          {"\u2191", "&uarr;"},
          {"\u2192", "&rarr;"},
          {"\u2193", "&darr;"},
          {"\u2194", "&harr;"},
          {"\u21B5", "&crarr;"},
          {"\u21D0", "&lArr;"},
          {"\u21D1", "&uArr;"},
          {"\u21D2", "&rArr;"},
          {"\u21D3", "&dArr;"},
          {"\u21D4", "&hArr;"},
          {"\u2200", "&forall;"},
          {"\u2202", "&part;"},
          {"\u2203", "&exist;"},
          {"\u2205", "&empty;"},
          {"\u2207", "&nabla;"},
          {"\u2208", "&isin;"},
          {"\u2209", "&notin;"},
          {"\u220B", "&ni;"},
          {"\u220F", "&prod;"},
          {"\u2211", "&sum;"},
          {"\u2212", "&minus;"},
          {"\u2217", "&lowast;"},
          {"\u221A", "&radic;"},
          {"\u221D", "&prop;"},
          {"\u221E", "&infin;"},
          {"\u2220", "&ang;"},
          {"\u2227", "&and;"},
          {"\u2228", "&or;"},
          {"\u2229", "&cap;"},
          {"\u222A", "&cup;"},
          {"\u222B", "&int;"},
          {"\u2234", "&there4;"},
          {"\u223C", "&sim;"},
          {"\u2245", "&cong;"},
          {"\u2248", "&asymp;"},
          {"\u2260", "&ne;"},
          {"\u2261", "&equiv;"},
          {"\u2264", "&le;"},
          {"\u2265", "&ge;"},
          {"\u2282", "&sub;"},
          {"\u2283", "&sup;"},
          {"\u2284", "&nsub;"},
          {"\u2286", "&sube;"},
          {"\u2287", "&supe;"},
          {"\u2295", "&oplus;"},
          {"\u2297", "&otimes;"},
          {"\u22A5", "&perp;"},
          {"\u22C5", "&sdot;"},
          {"\u2308", "&lceil;"},
          {"\u2309", "&rceil;"},
          {"\u230A", "&lfloor;"},
          {"\u230B", "&rfloor;"},
          {"\u2329", "&lang;"},
          {"\u232A", "&rang;"},
          {"\u25CA", "&loz;"},
          {"\u2660", "&spades;"},
          {"\u2663", "&clubs;"},
          {"\u2665", "&hearts;"},
          {"\u2666", "&diams;"},
          {"\u0152", "&OElig;"},
          {"\u0153", "&oelig;"},
          {"\u0160", "&Scaron;"},
          {"\u0161", "&scaron;"},
          {"\u0178", "&Yuml;"},
          {"\u02C6", "&circ;"},
          {"\u02DC", "&tilde;"},
          {"\u2002", "&ensp;"},
          {"\u2003", "&emsp;"},
          {"\u2009", "&thinsp;"},
          {"\u200C", "&zwnj;"},
          {"\u200D", "&zwj;"},
          {"\u200E", "&lrm;"},
          {"\u200F", "&rlm;"},
          {"\u2013", "&ndash;"},
          {"\u2014", "&mdash;"},
          {"\u2018", "&lsquo;"},
          {"\u2019", "&rsquo;"},
          {"\u201A", "&sbquo;"},
          {"\u201C", "&ldquo;"},
          {"\u201D", "&rdquo;"},
          {"\u201E", "&bdquo;"},
          {"\u2020", "&dagger;"},
          {"\u2021", "&Dagger;"},
          {"\u2030", "&permil;"},
          {"\u2039", "&lsaquo;"},
          {"\u203A", "&rsaquo;"},
          {"\u20AC", "&euro;"}
      }));
}

Value expr_unescapeHtml3(const std::vector<Value> &args) {
  return Value(utils::StringUtils::replaceMap(
      args[0].asString(),
      {
          {"&excl;", "!"},
          {"&quot;", "\""},
          {"&num;", "#"},
          {"&dollar;", "$"},
          {"&percnt;", "%"},
          {"&amp;", "&"},
          {"&apos;", "'"},
          {"&lpar;", "("},
          {"&rpar;", ")"},
          {"&ast;", "*"},
          {"&plus;", "+"},
          {"&comma;", ","},
          {"&minus;", "-"},
          {"&period;", "."},
          {"&sol;", "/"},
          {"&colon;", ":"},
          {"&semi;", ";"},
          {"&lt;", "<"},
          {"&equals;", "="},
          {"&gt;", ">"},
          {"&quest;", "?"},
          {"&commat;", "@"},
          {"&lsqb;", "["},
          {"&bsol;", "\\"},
          {"&rsqb;", "]"},
          {"&circ;", "^"},
          {"&lowbar;", "_"},
          {"&grave;", "`"},
          {"&lcub;", "{"},
          {"&verbar;", "|"},
          {"&rcub;", "}"},
          {"&tilde;", "~"},
          {"&iexcl;", "¡"},
          {"&cent;", "¢"},
          {"&pound;", "£"},
          {"&curren;", "¤"},
          {"&yen;", "¥"},
          {"&brkbar;", "¦"},
          {"&sect;", "§"},
          {"&uml;", "¨"},
          {"&copy;", "©"},
          {"&ordf;", "ª"},
          {"&laquo;", "«"},
          {"&not;", "¬"},
          {"&reg;", "®"},
          {"&macr;", "¯"},
          {"&deg;", "°"},
          {"&plusmn;", "±"},
          {"&sup2;", "²"},
          {"&sup3;", "³"},
          {"&acute;", "´"},
          {"&micro;", "µ"},
          {"&para;", "¶"},
          {"&middot;", "·"},
          {"&cedil;", "¸"},
          {"&sup1;", "¹"},
          {"&ordm;", "º"},
          {"&raquo;;", "»"},
          {"&frac14;", "¼"},
          {"&frac12;", "½"},
          {"&frac34;", "¾"},
          {"&iquest;", "¿"},
          {"&Agrave;", "À"},
          {"&Aacute;", "Á"},
          {"&Acirc;", "Â"},
          {"&Atilde;", "Ã"},
          {"&Auml;", "Ä"},
          {"&Aring;", "Å"},
          {"&AElig;", "Æ"},
          {"&Ccedil;", "Ç"},
          {"&Egrave;", "È"},
          {"&Eacute;", "É"},
          {"&Ecirc;", "Ê"},
          {"&Euml;", "Ë"},
          {"&Igrave;", "Ì"},
          {"&Iacute;", "Í"},
          {"&Icirc;", "Î"},
          {"&Iuml;", "Ï"},
          {"&ETH;", "Ð"},
          {"&Ntilde;", "Ñ"},
          {"&Ograve;", "Ò"},
          {"&Oacute;", "Ó"},
          {"&Ocirc;", "Ô"},
          {"&Otilde;", "Õ"},
          {"&Ouml;", "Ö"},
          {"&times;", "×"},
          {"&Oslash;", "Ø"},
          {"&Ugrave;;", "Ù"},
          {"&Uacute;", "Ú"},
          {"&Ucirc;", "Û"},
          {"&Uuml;", "Ü"},
          {"&Yacute;", "Ý"},
          {"&THORN;", "Þ"},
          {"&szlig;", "ß"},
          {"&agrave;", "à"},
          {"&aacute;", "á"},
          {"&acirc;", "â"},
          {"&atilde;", "ã"},
          {"&auml;", "ä"},
          {"&aring;", "å"},
          {"&aelig;", "æ"},
          {"&ccedil;", "ç"},
          {"&egrave;", "è"},
          {"&eacute;", "é"},
          {"&ecirc;", "ê"},
          {"&euml;", "ë"},
          {"&igrave;", "ì"},
          {"&iacute;", "í"},
          {"&icirc;", "î"},
          {"&iuml;", "ï"},
          {"&eth;", "ð"},
          {"&ntilde;", "ñ"},
          {"&ograve;", "ò"},
          {"&oacute;", "ó"},
          {"&ocirc;", "ô"},
          {"&otilde;", "õ"},
          {"&ouml;", "ö"},
          {"&divide;", "÷"},
          {"&oslash;", "ø"},
          {"&ugrave;", "ù"},
          {"&uacute;", "ú"},
          {"&ucirc;", "û"},
          {"&uuml;", "ü"},
          {"&yacute;", "ý"},
          {"&thorn;", "þ"},
          {"&yuml;", "ÿ"}
      }));
}

Value expr_unescapeHtml4(const std::vector<Value> &args) {
  return Value(utils::StringUtils::replaceMap(
      args[0].asString(),
      {
          {"&excl;", "!"},
          {"&quot;", "\""},
          {"&num;", "#"},
          {"&dollar;", "$"},
          {"&percnt;", "%"},
          {"&amp;", "&"},
          {"&apos;", "'"},
          {"&lpar;", "("},
          {"&rpar;", ")"},
          {"&ast;", "*"},
          {"&plus;", "+"},
          {"&comma;", ","},
          {"&minus;", "-"},
          {"&period;", "."},
          {"&sol;", "/"},
          {"&colon;", ":"},
          {"&semi;", ";"},
          {"&lt;", "<"},
          {"&equals;", "="},
          {"&gt;", ">"},
          {"&quest;", "?"},
          {"&commat;", "@"},
          {"&lsqb;", "["},
          {"&bsol;", "\\"},
          {"&rsqb;", "]"},
          {"&circ;", "^"},
          {"&lowbar;", "_"},
          {"&grave;", "`"},
          {"&lcub;", "{"},
          {"&verbar;", "|"},
          {"&rcub;", "}"},
          {"&tilde;", "~"},
          {"&iexcl;", "¡"},
          {"&cent;", "¢"},
          {"&pound;", "£"},
          {"&curren;", "¤"},
          {"&yen;", "¥"},
          {"&brkbar;", "¦"},
          {"&sect;", "§"},
          {"&uml;", "¨"},
          {"&copy;", "©"},
          {"&ordf;", "ª"},
          {"&laquo;", "«"},
          {"&not;", "¬"},
          {"&reg;", "®"},
          {"&macr;", "¯"},
          {"&deg;", "°"},
          {"&plusmn;", "±"},
          {"&sup2;", "²"},
          {"&sup3;", "³"},
          {"&acute;", "´"},
          {"&micro;", "µ"},
          {"&para;", "¶"},
          {"&middot;", "·"},
          {"&cedil;", "¸"},
          {"&sup1;", "¹"},
          {"&ordm;", "º"},
          {"&raquo;;", "»"},
          {"&frac14;", "¼"},
          {"&frac12;", "½"},
          {"&frac34;", "¾"},
          {"&iquest;", "¿"},
          {"&Agrave;", "À"},
          {"&Aacute;", "Á"},
          {"&Acirc;", "Â"},
          {"&Atilde;", "Ã"},
          {"&Auml;", "Ä"},
          {"&Aring;", "Å"},
          {"&AElig;", "Æ"},
          {"&Ccedil;", "Ç"},
          {"&Egrave;", "È"},
          {"&Eacute;", "É"},
          {"&Ecirc;", "Ê"},
          {"&Euml;", "Ë"},
          {"&Igrave;", "Ì"},
          {"&Iacute;", "Í"},
          {"&Icirc;", "Î"},
          {"&Iuml;", "Ï"},
          {"&ETH;", "Ð"},
          {"&Ntilde;", "Ñ"},
          {"&Ograve;", "Ò"},
          {"&Oacute;", "Ó"},
          {"&Ocirc;", "Ô"},
          {"&Otilde;", "Õ"},
          {"&Ouml;", "Ö"},
          {"&times;", "×"},
          {"&Oslash;", "Ø"},
          {"&Ugrave;;", "Ù"},
          {"&Uacute;", "Ú"},
          {"&Ucirc;", "Û"},
          {"&Uuml;", "Ü"},
          {"&Yacute;", "Ý"},
          {"&THORN;", "Þ"},
          {"&szlig;", "ß"},
          {"&agrave;", "à"},
          {"&aacute;", "á"},
          {"&acirc;", "â"},
          {"&atilde;", "ã"},
          {"&auml;", "ä"},
          {"&aring;", "å"},
          {"&aelig;", "æ"},
          {"&ccedil;", "ç"},
          {"&egrave;", "è"},
          {"&eacute;", "é"},
          {"&ecirc;", "ê"},
          {"&euml;", "ë"},
          {"&igrave;", "ì"},
          {"&iacute;", "í"},
          {"&icirc;", "î"},
          {"&iuml;", "ï"},
          {"&eth;", "ð"},
          {"&ntilde;", "ñ"},
          {"&ograve;", "ò"},
          {"&oacute;", "ó"},
          {"&ocirc;", "ô"},
          {"&otilde;", "õ"},
          {"&ouml;", "ö"},
          {"&divide;", "÷"},
          {"&oslash;", "ø"},
          {"&ugrave;", "ù"},
          {"&uacute;", "ú"},
          {"&ucirc;", "û"},
          {"&uuml;", "ü"},
          {"&yacute;", "ý"},
          {"&thorn;", "þ"},
          {"&yuml;", "ÿ"},
          {"&fnof;", "\u0192"},
          {"&Alpha;", "\u0391"},
          {"&Beta;", "\u0392"},
          {"&Gamma;", "\u0393"},
          {"&Delta;", "\u0394"},
          {"&Epsilon;", "\u0395"},
          {"&Zeta;", "\u0396"},
          {"&Eta;", "\u0397"},
          {"&Theta;", "\u0398"},
          {"&Iota;", "\u0399"},
          {"&Kappa;", "\u039A"},
          {"&Lambda;", "\u039B"},
          {"&Mu;", "\u039C"},
          {"&Nu;", "\u039D"},
          {"&Xi;", "\u039E"},
          {"&Omicron;", "\u039F"},
          {"&Pi;", "\u03A0"},
          {"&Rho;", "\u03A1"},
          {"&Sigma;", "\u03A3"},
          {"&Tau;", "\u03A4"},
          {"&Upsilon;", "\u03A5"},
          {"&Phi;", "\u03A6"},
          {"&Chi;", "\u03A7"},
          {"&Psi;", "\u03A8"},
          {"&Omega;", "\u03A9"},
          {"&alpha;", "\u03B1"},
          {"&beta;", "\u03B2"},
          {"&gamma;", "\u03B3"},
          {"&delta;", "\u03B4"},
          {"&epsilon;", "\u03B5"},
          {"&zeta;", "\u03B6"},
          {"&eta;", "\u03B7"},
          {"&theta;", "\u03B8"},
          {"&iota;", "\u03B9"},
          {"&kappa;", "\u03BA"},
          {"&lambda;", "\u03BB"},
          {"&mu;", "\u03BC"},
          {"&nu;", "\u03BD"},
          {"&xi;", "\u03BE"},
          {"&omicron;", "\u03BF"},
          {"&pi;", "\u03C0"},
          {"&rho;", "\u03C1"},
          {"&sigmaf;", "\u03C2"},
          {"&sigma;", "\u03C3"},
          {"&tau;", "\u03C4"},
          {"&upsilon;", "\u03C5"},
          {"&phi;", "\u03C6"},
          {"&chi;", "\u03C7"},
          {"&psi;", "\u03C8"},
          {"&omega;", "\u03C9"},
          {"&thetasym;", "\u03D1"},
          {"&upsih;", "\u03D2"},
          {"&piv;", "\u03D6"},
          {"&bull;", "\u2022"},
          {"&hellip;", "\u2026"},
          {"&prime;", "\u2032"},
          {"&Prime;", "\u2033"},
          {"&oline;", "\u203E"},
          {"&frasl;", "\u2044"},
          {"&weierp;", "\u2118"},
          {"&image;", "\u2111"},
          {"&real;", "\u211C"},
          {"&trade;", "\u2122"},
          {"&alefsym;", "\u2135"},
          {"&larr;", "\u2190"},
          {"&uarr;", "\u2191"},
          {"&rarr;", "\u2192"},
          {"&darr;", "\u2193"},
          {"&harr;", "\u2194"},
          {"&crarr;", "\u21B5"},
          {"&lArr;", "\u21D0"},
          {"&uArr;", "\u21D1"},
          {"&rArr;", "\u21D2"},
          {"&dArr;", "\u21D3"},
          {"&hArr;", "\u21D4"},
          {"&forall;", "\u2200"},
          {"&part;", "\u2202"},
          {"&exist;", "\u2203"},
          {"&empty;", "\u2205"},
          {"&nabla;", "\u2207"},
          {"&isin;", "\u2208"},
          {"&notin;", "\u2209"},
          {"&ni;", "\u220B"},
          {"&prod;", "\u220F"},
          {"&sum;", "\u2211"},
          {"&minus;", "\u2212"},
          {"&lowast;", "\u2217"},
          {"&radic;", "\u221A"},
          {"&prop;", "\u221D"},
          {"&infin;", "\u221E"},
          {"&ang;", "\u2220"},
          {"&and;", "\u2227"},
          {"&or;", "\u2228"},
          {"&cap;", "\u2229"},
          {"&cup;", "\u222A"},
          {"&int;", "\u222B"},
          {"&there4;", "\u2234"},
          {"&sim;", "\u223C"},
          {"&cong;", "\u2245"},
          {"&asymp;", "\u2248"},
          {"&ne;", "\u2260"},
          {"&equiv;", "\u2261"},
          {"&le;", "\u2264"},
          {"&ge;", "\u2265"},
          {"&sub;", "\u2282"},
          {"&sup;", "\u2283"},
          {"&nsub;", "\u2284"},
          {"&sube;", "\u2286"},
          {"&supe;", "\u2287"},
          {"&oplus;", "\u2295"},
          {"&otimes;", "\u2297"},
          {"&perp;", "\u22A5"},
          {"&sdot;", "\u22C5"},
          {"&lceil;", "\u2308"},
          {"&rceil;", "\u2309"},
          {"&lfloor;", "\u230A"},
          {"&rfloor;", "\u230B"},
          {"&lang;", "\u2329"},
          {"&rang;", "\u232A"},
          {"&loz;", "\u25CA"},
          {"&spades;", "\u2660"},
          {"&clubs;", "\u2663"},
          {"&hearts;", "\u2665"},
          {"&diams;", "\u2666"},
          {"&OElig;", "\u0152"},
          {"&oelig;", "\u0153"},
          {"&Scaron;", "\u0160"},
          {"&scaron;", "\u0161"},
          {"&Yuml;", "\u0178"},
          {"&circ;", "\u02C6"},
          {"&tilde;", "\u02DC"},
          {"&ensp;", "\u2002"},
          {"&emsp;", "\u2003"},
          {"&thinsp;", "\u2009"},
          {"&zwnj;", "\u200C"},
          {"&zwj;", "\u200D"},
          {"&lrm;", "\u200E"},
          {"&rlm;", "\u200F"},
          {"&ndash;", "\u2013"},
          {"&mdash;", "\u2014"},
          {"&lsquo;", "\u2018"},
          {"&rsquo;", "\u2019"},
          {"&sbquo;", "\u201A"},
          {"&ldquo;", "\u201C"},
          {"&rdquo;", "\u201D"},
          {"&bdquo;", "\u201E"},
          {"&dagger;", "\u2020"},
          {"&Dagger;", "\u2021"},
          {"&permil;", "\u2030"},
          {"&lsaquo;", "\u2039"},
          {"&rsaquo;", "\u203A"},
          {"&euro;", "\u20AC"}
      }));
}

Value expr_escapeXml(const std::vector<Value> &args) {
  return Value(utils::StringUtils::replaceMap(
      args[0].asString(),
      {
          {"\"", "&quot;"},
          {"'", "&apos;"},
          {"<", "&lt;"},
          {">", "&gt;"},
          {"&", "&amp;"}
      }));
}

Value expr_unescapeXml(const std::vector<Value> &args) {
  return Value(utils::StringUtils::replaceMap(
      args[0].asString(),
      {
          {"&quot;", "\""},
          {"&apos;", "'"},
          {"&lt;", "<"},
          {"&gt;", ">"},
          {"&amp;", "&"}
      }));
}

Value expr_escapeCsv(const std::vector<Value> &args) {
  auto result = args[0].asString();
  const char quote_req_chars[] = {'"', '\r', '\n', ','};
  bool quote_required = false;

  for (const auto &c : quote_req_chars) {
    if (result.find(c) != std::string::npos) {
      quote_required = true;
      break;
    }
  }

  if (quote_required) {
    std::string quoted_result = "\"";
    quoted_result.append(utils::StringUtils::replaceMap(result, {{"\"", "\"\""}}));
    quoted_result.append("\"");
    return Value(quoted_result);
  }

  return Value(result);
}

Value expr_unescapeCsv(const std::vector<Value> &args) {
  auto result = args[0].asString();

  if (result[0] == '"' && result[result.size() - 1] == '"') {
    bool quote_required = false;

    size_t quote_pos = result.find('"', 1);

    if (quote_pos != result.length() - 1) {
      quote_required = true;
    } else {
      const char quote_req_chars[] = {'\r', '\n', ','};

      for (const auto &c : quote_req_chars) {
        if (result.find(c) != std::string::npos) {
          quote_required = true;
          break;
        }
      }
    }

    if (quote_required) {
      return Value(utils::StringUtils::replaceMap(result.substr(1, result.size() - 2), {{"\"\"", "\""}}));
    }
  }

  return Value(result);
}

Value expr_urlEncode(const std::vector<Value> &args) {
  auto arg_0 = args[0].asString();
  CURL * curl = curl_easy_init();
  if (curl != nullptr) {
    char *output = curl_easy_escape(curl,
                                    arg_0.c_str(),
                                    static_cast<int>(arg_0.length()));
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
  CURL * curl = curl_easy_init();
  if (curl != nullptr) {
    int out_len;
    char *output = curl_easy_unescape(curl,
                                      arg_0.c_str(),
                                      static_cast<int>(arg_0.length()),
                                      &out_len);
    if (output != nullptr) {
      auto result = std::string(output, static_cast<unsigned long>(out_len));
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

#ifdef EXPRESSION_LANGUAGE_USE_REGEX

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
  const std::regex expr = std::regex(args[1].asString());

  return Value(std::regex_match(subject.begin(), subject.end(), expr));
}

Value expr_find(const std::vector<Value> &args) {
  const auto &subject = args[0].asString();
  const std::regex expr = std::regex(args[1].asString());

  return Value(std::regex_search(subject.begin(), subject.end(), expr));
}

#endif  // EXPRESSION_LANGUAGE_USE_REGEX

Value expr_binary_op(const std::vector<Value> &args,
                     long double (*ldop)(long double, long double),
                     int64_t (*iop)(int64_t, int64_t),
                     bool long_only = false) {
  try {
    if (!long_only && !args[0].isDecimal() && !args[1].isDecimal()) {
      return Value(iop(args[0].asSignedLong(), args[1].asSignedLong()));
    } else {
      return Value(ldop(args[0].asLongDouble(), args[1].asLongDouble()));
    }
  } catch (const std::exception &e) {
    return Value();
  }
}

Value expr_plus(const std::vector<Value> &args) {
  return expr_binary_op(args,
                        [](long double a, long double b) { return a + b; },
                        [](int64_t a, int64_t b) { return a + b; });
}

Value expr_minus(const std::vector<Value> &args) {
  return expr_binary_op(args,
                        [](long double a, long double b) { return a - b; },
                        [](int64_t a, int64_t b) { return a - b; });
}

Value expr_multiply(const std::vector<Value> &args) {
  return expr_binary_op(args,
                        [](long double a, long double b) { return a * b; },
                        [](int64_t a, int64_t b) { return a * b; });
}

Value expr_divide(const std::vector<Value> &args) {
  return expr_binary_op(args,
                        [](long double a, long double b) { return a / b; },
                        [](int64_t a, int64_t b) { return a / b; },
                        true);
}

Value expr_mod(const std::vector<Value> &args) {
  return expr_binary_op(args,
                        [](long double a, long double b) { return std::fmod(a, b); },
                        [](int64_t a, int64_t b) { return a % b; });
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

  const char chars[] =
      "0123456789ab"
      "cdefghijklmn"
      "opqrstuvwxyz";
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
  auto radix = args[1].asSignedLong();

  if (radix < 2 || radix > 36) {
    throw std::runtime_error("Cannot perform conversion due to invalid radix");
  }

  return Value(std::to_string(std::stoll(args[0].asString(), nullptr, radix)));
}

Value expr_random(const std::vector<Value> &args) {
  std::random_device random_device;
  std::mt19937 generator(random_device());
  std::uniform_int_distribution<int64_t> distribution(0, LLONG_MAX);
  return Value(distribution(generator));
}

template<Value T(const std::vector<Value> &)>
Expression make_dynamic_function_incomplete(const std::string &function_name,
                                            const std::vector<Expression> &args,
                                            std::size_t num_args) {

  if (args.size() < num_args) {
    std::stringstream message_ss;
    message_ss << "Expression language function "
               << function_name
               << " called with "
               << args.size()
               << " argument(s), but "
               << num_args
               << " are required";
    throw std::runtime_error(message_ss.str());
  }

  auto result = make_dynamic([=](const Parameters &params) -> Value {
    std::vector<Value> evaluated_args;

    for (const auto &arg : args) {
      evaluated_args.emplace_back(arg(params));
    }

    return T(evaluated_args);
  });

  return result;
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
    if (c != ' '
        && c != '\f'
        && c != '\n'
        && c != '\r'
        && c != '\t'
        && c != '\v') {
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

Expression make_dynamic_function(const std::string &function_name,
                                 const std::vector<Expression> &args) {
  if (function_name == "hostname") {
    return make_dynamic_function_incomplete<expr_hostname>(function_name, args, 0);
  } else if (function_name == "ip") {
    return make_dynamic_function_incomplete<expr_ip>(function_name, args, 0);
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
#ifdef EXPRESSION_LANGUAGE_USE_REGEX
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
#endif  // EXPRESSION_LANGUAGE_USE_REGEX
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
  } else {
    std::string msg("Unknown expression function: ");
    msg.append(function_name);
    throw std::runtime_error(msg);
  }
}

Expression make_function_composition(const Expression &arg,
                                     const std::vector<std::pair<std::string, std::vector<Expression>>> &chain) {

  auto expr = arg;

  for (const auto &chain_part : chain) {
    std::vector<Expression> complete_args = {expr};
    complete_args.insert(complete_args.end(), chain_part.second.begin(), chain_part.second.end());
    expr = make_dynamic_function(chain_part.first, complete_args);
  }

  return expr;
}

bool Expression::isDynamic() const {
  if (val_fn_) {
    return true;
  } else {
    return false;
  }
}

Expression Expression::operator+(const Expression &other_expr) const {
  if (isDynamic() && other_expr.isDynamic()) {
    auto val_fn = val_fn_;
    auto other_val_fn = other_expr.val_fn_;
    return make_dynamic([val_fn, other_val_fn](const Parameters &params) -> Value {
      Value result = val_fn(params);
      return Value(result.asString().append(other_val_fn(params).asString()));
    });
  } else if (isDynamic() && !other_expr.isDynamic()) {
    auto val_fn = val_fn_;
    auto other_val = other_expr.val_;
    return make_dynamic([val_fn, other_val](const Parameters &params) -> Value {
      Value result = val_fn(params);
      return Value(result.asString().append(other_val.asString()));
    });
  } else if (!isDynamic() && other_expr.isDynamic()) {
    auto val = val_;
    auto other_val_fn = other_expr.val_fn_;
    return make_dynamic([val, other_val_fn](const Parameters &params) -> Value {
      Value result(val);
      return Value(result.asString().append(other_val_fn(params).asString()));
    });
  } else if (!isDynamic() && !other_expr.isDynamic()) {
    std::string result(val_.asString());
    result.append(other_expr.val_.asString());
    return make_static(result);
  } else {
    throw std::runtime_error("Invalid function composition");
  }
}

Value Expression::operator()(const Parameters &params) const {
  if (isDynamic()) {
    return val_fn_(params);
  } else {
    return val_;
  }
}

} /* namespace expression */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
