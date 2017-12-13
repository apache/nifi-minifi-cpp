/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2016 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/

#include <bustache/generate.hpp>
#include <bustache/generate/ostream.hpp>
#include <bustache/generate/string.hpp>

namespace bustache { namespace detail
{
    value::pointer content_visitor_base::resolve(std::string const& key) const
    {
        auto ki = key.begin();
        auto ke = key.end();
        if (ki == ke)
            return{};
        value::pointer pv = nullptr;
        if (*ki == '.')
        {
            if (++ki == ke)
                return cursor;
            auto k0 = ki;
            while (*ki != '.' && ++ki != ke);
            key_cache.assign(k0, ki);
            pv = find(scope->data, key_cache);
        }
        else
        {
            auto k0 = ki;
            while (ki != ke && *ki != '.') ++ki;
            key_cache.assign(k0, ki);
            pv = scope->lookup(key_cache);
        }
        if (ki == ke)
            return pv;
        if (auto obj = get<object>(pv))
        {
            auto k0 = ++ki;
            while (ki != ke)
            {
                if (*ki == '.')
                {
                    key_cache.assign(k0, ki);
                    obj = get<object>(find(*obj, key_cache));
                    if (!obj)
                        return nullptr;
                    k0 = ++ki;
                }
                else
                    ++ki;
            }
            key_cache.assign(k0, ki);
            return find(*obj, key_cache);
        }
        return nullptr;
    }
}}

namespace bustache
{
    template
    void generate_ostream
    (
        std::ostream& out, format const& fmt,
        value::view const& data, detail::any_context const& context, option_type flag
    );

    template
    void generate_string
    (
        std::string& out, format const& fmt,
        value::view const& data, detail::any_context const& context, option_type flag
    );
}