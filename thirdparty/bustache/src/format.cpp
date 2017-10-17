/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2014-2016 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#include <cctype>
#include <utility>
#include <cstring>
#include <bustache/format.hpp>

namespace bustache { namespace parser { namespace
{
    using delim = std::pair<std::string, std::string>;

    template<class I>
    inline void skip(I& i, I e)
    {
        while (i != e && std::isspace(*i))
            ++i;
    }

    template<class I>
    inline bool parse_char(I& i, I e, char c)
    {
        if (i != e && *i == c)
        {
            ++i;
            return true;
        }
        return false;
    }

    template<class I>
    inline bool parse_lit(I& i, I e, boost::string_ref const& str)
    {
        I i0 = i;
        for (char c : str)
        {
            if (!parse_char(i, e, c))
            {
                i = i0;
                return false;
            }
        }
        return true;
    }

    template<class I>
    void expect_key(I& i, I e, delim& d, std::string& attr, bool suffix)
    {
        skip(i, e);
        I i0 = i;
        while (i != e)
        {
            I i1 = i;
            skip(i, e);
            if (!suffix || parse_char(i, e, '}'))
            {
                skip(i, e);
                if (parse_lit(i, e, d.second))
                {
                    attr.assign(i0, i1);
                    if (i0 == i1)
                        throw format_error(error_badkey);
                    return;
                }
            }
            if (i != e)
                ++i;
        }
        throw format_error(error_badkey);
    }

    template<class I>
    bool parse_content
    (
        I& i0, I& i, I e, delim& d, bool& pure,
        boost::string_ref& text, ast::content& attr,
        boost::string_ref const& section
    );

    template<class I>
    void parse_contents
    (
        I i0, I& i, I e, delim& d, bool& pure,
        ast::content_list& attr, boost::string_ref const& section
    );

    template<class I>
    I process_pure(I& i, I e, bool& pure)
    {
        I i0 = i;
        if (pure)
        {
            while (i != e)
            {
                if (*i == '\n')
                {
                    i0 = ++i;
                    break;
                }
                else if (std::isspace(*i))
                    ++i;
                else
                {
                    pure = false;
                    break;
                }
            }
        }
        return i0;
    }

    template<class I>
    inline bool expect_block(I& i, I e, delim& d, bool& pure, ast::block& attr)
    {
        expect_key(i, e, d, attr.key, false);
        I i0 = process_pure(i, e, pure);
        bool standalone = pure;
        parse_contents(i0, i, e, d, pure, attr.contents, attr.key);
        return standalone;
    }

    template<class I>
    bool expect_inheritance(I& i, I e, delim& d, bool& pure, ast::partial& attr)
    {
        expect_key(i, e, d, attr.key, false);
        I i0 = process_pure(i, e, pure);
        bool standalone = pure;
        for (boost::string_ref text;;)
        {
            ast::content a;
            auto end = parse_content(i0, i, e, d, pure, text, a, attr.key);
            if (auto p = get<ast::block>(&a))
                attr.overriders.emplace(std::move(p->key), std::move(p->contents));
            if (end)
                break;
        }
        return standalone;
    }

    template<class I>
    void expect_comment(I& i, I e, delim& d)
    {
        while (!parse_lit(i, e, d.second))
        {
            if (i == e)
                throw format_error(error_delim);
            ++i;
        }
    }

    template<class I>
    void expect_set_delim(I& i, I e, delim& d)
    {
        skip(i, e);
        I i0 = i;
        while (i != e)
        {
            if (std::isspace(*i))
                break;
            ++i;
        }
        if (i == e)
            throw format_error(error_baddelim);
        d.first.assign(i0, i);
        skip(i, e);
        i0 = i;
        I i1 = i;
        for (;; ++i)
        {
            if (i == e)
                throw format_error(error_set_delim);
            if (*i == '=')
            {
                i1 = i;
                break;
            }
            if (std::isspace(*i))
            {
                i1 = i;
                skip(++i, e);
                if (i == e || *i != '=')
                    throw format_error(error_set_delim);
                break;
            }
        }
        if (i0 == i1)
            throw format_error(error_baddelim);
        std::string new_close(i0, i1);
        skip(++i, e);
        if (!parse_lit(i, e, d.second))
            throw format_error(error_delim);
        d.second = std::move(new_close);
    }

    struct tag_result
    {
        bool is_end_section;
        bool check_standalone;
        bool is_standalone;
    };

    template<class I>
    tag_result expect_tag
    (
        I& i, I e, delim& d, bool& pure,
        ast::content& attr, boost::string_ref const& section
    )
    {
        skip(i, e);
        if (i == e)
            throw format_error(error_badkey);
        tag_result ret{};
        switch (*i)
        {
        case '#':
        case '^':
        {
            ast::section a;
            a.tag = *i;
            ret.is_standalone = expect_block(++i, e, d, pure, a);
            attr = std::move(a);
            return ret;
        }
        case '/':
            skip(++i, e);
            if (section.empty() || !parse_lit(i, e, section))
                throw format_error(error_section);
            skip(i, e);
            if (!parse_lit(i, e, d.second))
                throw format_error(error_delim);
            ret.check_standalone = pure;
            ret.is_end_section = true;
            break;
        case '!':
        {
            expect_comment(++i, e, d);
            ret.check_standalone = pure;
            break;
        }
        case '=':
        {
            expect_set_delim(++i, e, d);
            ret.check_standalone = pure;
            break;
        }
        case '>':
        {
            ast::partial a;
            expect_key(++i, e, d, a.key, false);
            attr = std::move(a);
            ret.check_standalone = pure;
            break;
        }
        case '&':
        case '{':
        {
            ast::variable a;
            a.tag = *i;
            expect_key(++i, e, d, a.key, a.tag == '{');
            attr = std::move(a);
            pure = false;
            break;
        }
        // Extensions
        case '<':
        {
            ast::partial a;
            ret.is_standalone = expect_inheritance(++i, e, d, pure, a);
            attr = std::move(a);
            return ret;
        }
        case '$':
        {
            ast::block a;
            ret.is_standalone = expect_block(++i, e, d, pure, a);
            attr = std::move(a);
            return ret;
        }
        default:
            ast::variable a;
            expect_key(i, e, d, a.key, false);
            attr = std::move(a);
            pure = false;
            break;
        }
        return ret;
    }

    // return true if it ends
    template<class I>
    bool parse_content
    (
        I& i0, I& i, I e, delim& d, bool& pure,
        boost::string_ref& text, ast::content& attr,
        boost::string_ref const& section
    )
    {
        for (I i1 = i; i != e;)
        {
            if (*i == '\n')
            {
                pure = true;
                i1 = ++i;
            }
            else if (std::isspace(*i))
                ++i;
            else
            {
                I i2 = i;
                if (parse_lit(i, e, d.first))
                {
                    tag_result tag(expect_tag(i, e, d, pure, attr, section));
                    text = boost::string_ref(i0, i1 - i0);
                    if (tag.check_standalone)
                    {
                        I i3 = i;
                        while (i != e)
                        {
                            if (*i == '\n')
                            {
                                ++i;
                                break;
                            }
                            else if (std::isspace(*i))
                                ++i;
                            else
                            {
                                pure = false;
                                text = boost::string_ref(i0, i2 - i0);
                                // For end-section, we move the current pos (i)
                                // since i0 is local to the section and is not
                                // propagated upwards.
                                (tag.is_end_section ? i : i0) = i3;
                                return tag.is_end_section;
                            }
                        }
                        tag.is_standalone = true;
                    }
                    if (!tag.is_standalone)
                        text = boost::string_ref(i0, i2 - i0);
                    else if (auto partial = get<ast::partial>(&attr))
                        partial->indent.assign(i1, i2 - i1);
                    i0 = i;
                    return i == e || tag.is_end_section;
                }
                else
                {
                    pure = false;
                    ++i;
                }
            }
        }
        text = boost::string_ref(i0, i - i0);
        return true;
    }

    template<class I>
    void parse_contents
    (
        I i0, I& i, I e, delim& d, bool& pure,
        ast::content_list& attr, boost::string_ref const& section
    )
    {
        for (;;)
        {
            boost::string_ref text;
            ast::content a;
            auto end = parse_content(i0, i, e, d, pure, text, a, section);
            if (!text.empty())
                attr.push_back(text);
            if (!is_null(a))
                attr.push_back(std::move(a));
            if (end)
                return;
        }
    }

    template<class I>
    inline void parse_start(I& i, I e, ast::content_list& attr)
    {
        delim d("{{", "}}");
        bool pure = true;
        parse_contents(i, i, e, d, pure, attr, {});
    }
}}}

namespace bustache
{
    static char const* get_error_string(error_type err)
    {
        switch (err)
        {
        case error_set_delim:
            return "format_error(error_set_delim): mismatched '='";
        case error_baddelim:
            return "format_error(error_baddelim): invalid delimiter";
        case error_delim:
            return "format_error(error_delim): mismatched delimiter";
        case error_section:
            return "format_error(error_section): mismatched end section tag";
        case error_badkey:
            return "format_error(error_badkey): invalid key";
        default:
            return "format_error";
        }
    }

    format_error::format_error(error_type err)
      : runtime_error(get_error_string(err)), _err(err)
    {}

    void format::init(char const* begin, char const* end)
    {
        parser::parse_start(begin, end, _contents);
    }

    struct accum_size
    {
        using result_type = std::size_t;

        std::size_t operator()(ast::text const& text) const
        {
            return text.size();
        }

        std::size_t operator()(ast::section const& section) const
        {
            std::size_t n = 0;
            for (auto const& content : section.contents)
                n += visit(*this, content);
            return n;
        }

        template <typename T>
        std::size_t operator()(T const&) const
        {
            return 0;
        }
    };

    std::size_t format::text_size() const
    {
        accum_size accum;
        std::size_t n = 0;
        for (auto const& content : _contents)
            n += visit(accum, content);
        return n;
    }

    struct copy_text_visitor
    {
        using result_type = void;

        char* data;

        void operator()(ast::text& text)
        {
            auto n = text.size();
            std::memcpy(data, text.data(), n);
            text = {data, n};
            data += n;
        }

        void operator()(ast::section& section)
        {
            for (auto& content : section.contents)
                visit(*this, content);
        }

        template <typename T>
        void operator()(T const&) const {}
    };

    void format::copy_text(std::size_t n)
    {
        _text.reset(new char[n]);
        copy_text_visitor visitor{_text.get()};
        for (auto& content : _contents)
            visit(visitor, content);
    }
}