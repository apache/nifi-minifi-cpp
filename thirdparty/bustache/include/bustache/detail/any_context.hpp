/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2016 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef BUSTACHE_DETAIL_ANY_CONTEXT_HPP_INCLUDED
#define BUSTACHE_DETAIL_ANY_CONTEXT_HPP_INCLUDED

#include <string>
#include <utility>

namespace bustache
{
    struct format;
}

namespace bustache { namespace detail
{
    struct any_context
    {
        using value_type = std::pair<std::string const, format>;
        using iterator = value_type const*;

        template<class Context>
        any_context(Context const& context) noexcept
            : _data(&context), _find(find_fn<Context>)
        {}

        iterator find(std::string const& key) const
        {
            return _find(_data, key);
        }

        iterator end() const
        {
            return nullptr;
        }

    private:

        template<class Context>
        static value_type const* find_fn(void const* data, std::string const& key)
        {
            auto ctx = static_cast<Context const*>(data);
            auto it = ctx->find(key);
            return it != ctx->end() ? &*it : nullptr;
        }

        void const* _data;
        value_type const* (*_find)(void const*, std::string const&);
    };
}}

#endif