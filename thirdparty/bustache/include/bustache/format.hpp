/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2014-2016 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef BUSTACHE_FORMAT_HPP_INCLUDED
#define BUSTACHE_FORMAT_HPP_INCLUDED

#include <bustache/ast.hpp>
#include <stdexcept>
#include <memory>

namespace bustache
{
    struct format;
    
    using option_type = bool;
    constexpr option_type normal = false;
    constexpr option_type escape_html = true;

    template<class T, class Context>
    struct manipulator
    {
        format const& fmt;
        T const& data;
        Context const& context;
        option_type const flag;
    };

    struct no_context
    {
        using value_type = std::pair<std::string const, format>;
        using iterator = value_type const*;
        
        constexpr iterator find(std::string const&) const
        {
            return nullptr;
        }
        
        constexpr iterator end() const
        {
            return nullptr;
        }

        static no_context const& dummy()
        {
            static no_context const _{};
            return _;
        }
    };

    enum error_type
    {
        error_set_delim,
        error_baddelim,
        error_delim,
        error_section,
        error_badkey
    };

    class format_error : public std::runtime_error
    {
        error_type _err;

    public:
        explicit format_error(error_type err);

        error_type code() const
        {
            return _err;
        }
    };
    
    struct format
    {
        format() = default;

        format(char const* begin, char const* end)
        {
            init(begin, end);
        }
        
        template<class Source>
        explicit format(Source const& source)
        {
            init(source.data(), source.data() + source.size());
        }
        
        template<class Source>
        explicit format(Source const&& source)
        {
            init(source.data(), source.data() + source.size());
            copy_text(text_size());
        }

        template<std::size_t N>
        explicit format(char const (&source)[N])
        {
            init(source, source + (N - 1));
        }

        explicit format(ast::content_list contents, bool copytext = true)
          : _contents(std::move(contents))
        {
            if (copytext)
                copy_text(text_size());
        }

        format(format&& other) noexcept
          : _contents(std::move(other._contents)), _text(std::move(other._text))
        {}

        format(format const& other) : _contents(other._contents)
        {
            if (other._text)
                copy_text(text_size());
        }

        template<class T>
        manipulator<T, no_context>
        operator()(T const& data, option_type flag = normal) const
        {
            return {*this, data, no_context::dummy(), flag};
        }
        
        template<class T, class Context>
        manipulator<T, Context>
        operator()(T const& data, Context const& context, option_type flag = normal) const
        {
            return {*this, data, context, flag};
        }
        
        ast::content_list const& contents() const
        {
            return _contents;
        }
        
    private:
        
        void init(char const* begin, char const* end);
        std::size_t text_size() const;
        void copy_text(std::size_t n);

        ast::content_list _contents;
        std::unique_ptr<char[]> _text;
    };

    inline namespace literals
    {
        inline format operator"" _fmt(char const* str, std::size_t n)
        {
            return format(str, str + n);
        }
    }
}

#endif