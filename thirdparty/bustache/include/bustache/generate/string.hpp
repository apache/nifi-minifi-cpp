/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2016 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef BUSTACHE_GENERATE_STRING_HPP_INCLUDED
#define BUSTACHE_GENERATE_STRING_HPP_INCLUDED

#include <cstdio> // for snprintf
#include <string>
#include <bustache/generate.hpp>

namespace bustache { namespace detail
{
    template<class String>
    struct string_sink
    {
        String& out;

        void operator()(char const* it, char const* end) const
        {
            out.insert(out.end(), it, end);
        }

        void operator()(int data) const
        {
            append_num("%d", data);
        }

        void operator()(double data) const
        {
            append_num("%g", data);
        }

        void operator()(bool data) const
        {
            data ? append("true") : append("false");
        }

        template<std::size_t N>
        void append(char const (&str)[N]) const
        {
            out.insert(out.end(), str, str + (N - 1));
        }

        template<class T>
        void append_num(char const* fmt, T data) const
        {
            char buf[64];
            char* p;
            auto old_size = out.size();
            auto capacity = out.capacity();
            auto bufsize = capacity - old_size;
            if (bufsize)
            {
                out.resize(capacity);
                p = &out.front() + old_size;
            }
            else
            {
                bufsize = sizeof(buf);
                p = buf;
            }
            auto n = std::snprintf(p, bufsize, fmt, data);
            if (n < 0) // error
                return;
            if (unsigned(n + 1) <= bufsize)
            {
                if (p == buf)
                {
                    out.insert(out.end(), p, p + n);
                    return;
                }
            }
            else
            {
                out.resize(old_size + n + 1); // '\0' will be written
                std::snprintf(&out.front() + old_size, n + 1, fmt, data);
            }
            out.resize(old_size + n);
        }
    };
}}

namespace bustache
{
    template<class String, class Context>
    void generate_string
    (
        String& out, format const& fmt,
        value::view const& data, Context const& context, option_type flag
    )
    {
        detail::string_sink<String> sink{out};
        generate(sink, fmt, data, context, flag);
    }

    // This is instantiated in src/generate.cpp.
    extern template
    void generate_string
    (
        std::string& out, format const& fmt,
        value::view const& data, detail::any_context const& context, option_type flag
    );
}

#endif