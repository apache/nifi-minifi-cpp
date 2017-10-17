/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2014-2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef BUSTACHE_AST_HPP_INCLUDED
#define BUSTACHE_AST_HPP_INCLUDED

#include <bustache/detail/variant.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/unordered_map.hpp>
#include <vector>
#include <string>

namespace bustache { namespace ast
{
    struct variable;
    struct section;
    class content;

    using text = boost::string_ref;

    using content_list = std::vector<content>;

    using override_map = boost::unordered_map<std::string, content_list>;

    struct null {};

    struct variable
    {
        std::string key;
        char tag = '\0';
#ifdef _MSC_VER // Workaround MSVC bug.
        variable() = default;

        explicit variable(std::string key, char tag = '\0')
          : key(std::move(key)), tag(tag)
        {}
#endif
    };

    struct block
    {
        std::string key;
        content_list contents;
    };

    struct section : block
    {
        char tag = '#';
    };

    struct partial
    {
        std::string key;
        std::string indent;
        override_map overriders;
    };

#define BUSTACHE_AST_CONTENT(X, D)                                              \
    X(0, null, D)                                                               \
    X(1, text, D)                                                               \
    X(2, variable, D)                                                           \
    X(3, section, D)                                                            \
    X(4, partial, D)                                                            \
    X(5, block, D)                                                              \
/***/

    class content : public variant_base<content>
    {
        BUSTACHE_AST_CONTENT(Zz_BUSTACHE_VARIANT_MATCH,)
    public:
        Zz_BUSTACHE_VARIANT_DECL(content, BUSTACHE_AST_CONTENT, true)

        content() noexcept : _which(0), _0() {}
    };
#undef BUSTACHE_AST_CONTENT

    inline bool is_null(content const& c)
    {
        return !c.which();
    }
}}

#endif