/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2016 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef BUSTACHE_GENERATE_HPP_INCLUDED
#define BUSTACHE_GENERATE_HPP_INCLUDED

#include <bustache/model.hpp>

namespace bustache { namespace detail
{
    inline value::pointer find(object const& data, std::string const& key)
    {
        auto it = data.find(key);
        if (it != data.end())
            return it->second.get_pointer();
        return nullptr;
    }

    template<class Sink>
    struct value_printer
    {
        typedef void result_type;
        
        Sink const& sink;
        bool const escaping;

        void operator()(std::nullptr_t) const {}
        
        template<class T>
        void operator()(T data) const
        {
            sink(data);
        }

        void operator()(std::string const& data) const
        {
            auto it = data.data(), end = it + data.size();
            if (escaping)
                escape_html(it, end);
            else
                sink(it, end);
        }
        
        void operator()(array const& data) const
        {
            auto it = data.begin(), end = data.end();
            if (it != end)
            {
                visit(*this, *it);
                while (++it != end)
                {
                    literal(",");
                    visit(*this, *it);
                }
            }
        }

        void operator()(object const&) const
        {
            literal("[Object]");
        }

        void operator()(lambda0v const& data) const
        {
            visit(*this, data());
        }

        void operator()(lambda1v const& data) const
        {
            visit(*this, data({}));
        }

        template<class Sig>
        void operator()(std::function<Sig> const&) const
        {
            literal("[Function]");
        }

        void escape_html(char const* it, char const* end) const
        {
            char const* last = it;
            while (it != end)
            {
                switch (*it)
                {
                case '&': sink(last, it); literal("&amp;"); break;
                case '<': sink(last, it); literal("&lt;"); break;
                case '>': sink(last, it); literal("&gt;"); break;
                case '\\': sink(last, it); literal("&#92;"); break;
                case '"': sink(last, it); literal("&quot;"); break;
                default:  ++it; continue;
                }
                last = ++it;
            }
            sink(last, it);
        }

        template<std::size_t N>
        void literal(char const (&str)[N]) const
        {
            sink(str, str + (N - 1));
        }
    };

    struct content_scope
    {
        content_scope const* const parent;
        object const& data;

        value::pointer lookup(std::string const& key) const
        {
            if (auto pv = find(data, key))
                return pv;
            if (parent)
                return parent->lookup(key);
            return nullptr;
        }
    };

    struct content_visitor_base
    {
        using result_type = void;

        content_scope const* scope;
        value::pointer cursor;
        std::vector<ast::override_map const*> chain;
        mutable std::string key_cache;

        // Defined in src/generate.cpp.
        value::pointer resolve(std::string const& key) const;

        ast::content_list const* find_override(std::string const& key) const
        {
            for (auto pm : chain)
            {
                auto it = pm->find(key);
                if (it != pm->end())
                    return &it->second;
            }
            return nullptr;
        }
    };

    template<class ContentVisitor>
    struct variable_visitor : value_printer<typename ContentVisitor::sink_type>
    {
        using base_type = value_printer<typename ContentVisitor::sink_type>;
        
        ContentVisitor& parent;

        variable_visitor(ContentVisitor& parent, bool escaping)
          : base_type{parent.sink, escaping}, parent(parent)
        {}

        using base_type::operator();

        void operator()(lambda0f const& data) const
        {
            auto fmt(data());
            for (auto const& content : fmt.contents())
                visit(parent, content);
        }
    };

    template<class ContentVisitor>
    struct section_visitor
    {
        using result_type = bool;

        ContentVisitor& parent;
        ast::content_list const& contents;
        bool const inverted;

        bool operator()(object const& data) const
        {
            if (!inverted)
            {
                content_scope scope{parent.scope, data};
                auto old_scope = parent.scope;
                parent.scope = &scope;
                for (auto const& content : contents)
                    visit(parent, content);
                parent.scope = old_scope;
            }
            return false;
        }

        bool operator()(array const& data) const
        {
            if (inverted)
                return data.empty();

            for (auto const& val : data)
            {
                parent.cursor = val.get_pointer();
                if (auto obj = get<object>(&val))
                {
                    content_scope scope{parent.scope, *obj};
                    auto old_scope = parent.scope;
                    parent.scope = &scope;
                    for (auto const& content : contents)
                        visit(parent, content);
                    parent.scope = old_scope;
                }
                else
                {
                    for (auto const& content : contents)
                        visit(parent, content);
                }
            }
            return false;
        }

        bool operator()(bool data) const
        {
            return data ^ inverted;
        }

        // The 2 overloads below are not necessary but to suppress
        // the stupid MSVC warning.
        bool operator()(int data) const
        {
            return !!data ^ inverted;
        }

        bool operator()(double data) const
        {
            return !!data ^ inverted;
        }

        bool operator()(std::string const& data) const
        {
            return !data.empty() ^ inverted;
        }

        bool operator()(std::nullptr_t) const
        {
            return inverted;
        }

        bool operator()(lambda0v const& data) const
        {
            return inverted ? false : visit(*this, data());
        }

        bool operator()(lambda0f const& data) const
        {
            if (!inverted)
            {
                auto fmt(data());
                for (auto const& content : fmt.contents())
                    visit(parent, content);
            }
            return false;
        }

        bool operator()(lambda1v const& data) const
        {
            return inverted ? false : visit(*this, data(contents));
        }

        bool operator()(lambda1f const& data) const
        {
            if (!inverted)
            {
                auto fmt(data(contents));
                for (auto const& content : fmt.contents())
                    visit(parent, content);
            }
            return false;
        }
    };

    template<class Sink, class Context>
    struct content_visitor : content_visitor_base
    {
        using sink_type = Sink;

        Sink const& sink;
        Context const& context;
        std::string indent;
        bool needs_indent;
        bool const escaping;

        content_visitor
        (
            content_scope const& scope, value::pointer cursor,
            Sink const &sink, Context const &context, bool escaping
        )
          : content_visitor_base{&scope, cursor, {}, {}}
          , sink(sink), context(context), needs_indent(), escaping(escaping)
        {}

        void operator()(ast::text const& text)
        {
            auto i = text.begin();
            auto e = text.end();
            assert(i != e && "empty text shouldn't be in ast");
            if (indent.empty())
            {
                sink(i, e);
                return;
            }
            --e; // Don't flush indent on last newline.
            auto const ib = indent.data();
            auto const ie = ib + indent.size();
            if (needs_indent)
                sink(ib, ie);
            auto i0 = i;
            while (i != e)
            {
                if (*i++ == '\n')
                {
                    sink(i0, i);
                    sink(ib, ie);
                    i0 = i;
                }
            }
            needs_indent = *i++ == '\n';
            sink(i0, i);
        }
        
        void operator()(ast::variable const& variable)
        {
            if (auto pv = resolve(variable.key))
            {
                if (needs_indent)
                {
                    sink(indent.data(), indent.data() + indent.size());
                    needs_indent = false;
                }
                variable_visitor<content_visitor> visitor
                {
                    *this, escaping && !variable.tag
                };
                visit(visitor, *pv);
            }
        }
        
        void operator()(ast::section const& section)
        {
            bool inverted = section.tag == '^';
            auto old_cursor = cursor;
            if (auto next = resolve(section.key))
            {
                cursor = next;
                section_visitor<content_visitor> visitor
                {
                    *this, section.contents, inverted
                };
                if (!visit(visitor, *cursor))
                {
                    cursor = old_cursor;
                    return;
                }
            }
            else if (!inverted)
                return;
                
            for (auto const& content : section.contents)
                visit(*this, content);
            cursor = old_cursor;
        }
        
        void operator()(ast::partial const& partial)
        {
            auto it = context.find(partial.key);
            if (it != context.end())
            {
                if (it->second.contents().empty())
                    return;

                auto old_size = indent.size();
                auto old_chain = chain.size();
                indent += partial.indent;
                needs_indent |= !partial.indent.empty();
                if (!partial.overriders.empty())
                    chain.push_back(&partial.overriders);
                for (auto const& content : it->second.contents())
                    visit(*this, content);
                chain.resize(old_chain);
                indent.resize(old_size);
            }
        }

        void operator()(ast::block const& block)
        {
            auto pc = find_override(block.key);
            if (!pc)
                pc = &block.contents;
            for (auto const& content : *pc)
                visit(*this, content);
        }

        void operator()(ast::null) const {} // never called
    };
}}

namespace bustache
{
    template<class Sink>
    inline void generate
    (
        Sink& sink, format const& fmt, value::view const& data,
        option_type flag = normal
    )
    {
        generate(sink, fmt, data, no_context::dummy(), flag);
    }
    
    template<class Sink, class Context>
    void generate
    (
        Sink& sink, format const& fmt, value::view const& data,
        Context const& context, option_type flag = normal
    )
    {
        object const empty;
        auto obj = get<object>(&data);
        detail::content_scope scope{nullptr, obj ? *obj : empty};
        detail::content_visitor<Sink, Context> visitor{scope, data.get_pointer(), sink, context, flag};
        for (auto const& content : fmt.contents())
            visit(visitor, content);
    }
}

#endif