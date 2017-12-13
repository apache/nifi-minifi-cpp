/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2016 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef BUSTACHE_DEBUG_HPP_INCLUDED
#define BUSTACHE_DEBUG_HPP_INCLUDED

#include <iostream>
#include <iomanip>
#include <bustache/format.hpp>

namespace bustache { namespace detail
{
    template<class CharT, class Traits>
    struct ast_printer
    {
        std::basic_ostream<CharT, Traits>& out;
        unsigned level;
        unsigned const space;

        void operator()(ast::text const& text) const
        {
            indent();
            auto i = text.begin();
            auto i0 = i;
            auto e = text.end();
            out << "text: \"";
            while (i != e)
            {
                char const* esc = nullptr;
                switch (*i)
                {
                case '\r': esc = "\\r"; break;
                case '\n': esc = "\\n"; break;
                case '\\': esc = "\\\\"; break;
                default: ++i; continue;
                }
                out.write(i0, i - i0);
                i0 = ++i;
                out << esc;
            }
            out.write(i0, i - i0);
            out << "\"\n";
        }

        void operator()(ast::variable const& variable) const
        {
            indent();
            out << "variable";
            if (variable.tag)
                out << "(&)";
            out << ": " << variable.key << "\n";
        }

        void operator()(ast::section const& section)
        {
            out;
            out << "section(" << section.tag << "): " << section.key << "\n";
            ++level;
            for (auto const& content : section.contents)
                apply_visitor(*this, content);
            --level;
        }

        void operator()(ast::partial const& partial) const
        {
            out << "partial: " << partial.key << "\n";
        }

        void operator()(ast::null) const {} // never called

        void indent() const
        {
            out << std::setw(space * level) << "";
        }
    };
}}

namespace bustache
{
    template<class CharT, class Traits>
    inline void print_ast(std::basic_ostream<CharT, Traits>& out, format const& fmt, unsigned indent = 4)
    {
        detail::ast_printer<CharT, Traits> visitor{out, 0, indent};
        for (auto const& content : fmt.contents())
            apply_visitor(visitor, content);
    }
}

#endif