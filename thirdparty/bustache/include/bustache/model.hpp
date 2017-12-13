/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2014-2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef BUSTACHE_MODEL_HPP_INCLUDED
#define BUSTACHE_MODEL_HPP_INCLUDED

#include <bustache/format.hpp>
#include <bustache/detail/variant.hpp>
#include <bustache/detail/any_context.hpp>
#include <vector>
#include <functional>
#include <boost/unordered_map.hpp>

namespace bustache
{
    class value;

    using array = std::vector<value>;

    // We use boost::unordered_map because it allows incomplete type.
    using object = boost::unordered_map<std::string, value>;

    using lambda0v = std::function<value()>;

    using lambda0f = std::function<format()>;

    using lambda1v = std::function<value(ast::content_list const&)>;

    using lambda1f = std::function<format(ast::content_list const&)>;

    namespace detail
    {
        struct bool_
        {
            bool_(bool);
        };
    }

#define BUSTACHE_VALUE(X, D)                                                    \
    X(0, std::nullptr_t, D)                                                     \
    X(1, bool, D)                                                               \
    X(2, int, D)                                                                \
    X(3, double, D)                                                             \
    X(4, std::string, D)                                                        \
    X(5, array, D)                                                              \
    X(6, lambda0v, D)                                                           \
    X(7, lambda0f, D)                                                           \
    X(8, lambda1v, D)                                                           \
    X(9, lambda1f, D)                                                           \
    X(10, object, D)                                                            \
/***/

    class value : public variant_base<value>
    {
        static std::nullptr_t match_type(std::nullptr_t);
        static int match_type(int);
        // Use a fake bool_ to prevent unintended bool conversion.
        static bool match_type(detail::bool_);
        static double match_type(double);
        static std::string match_type(std::string);
        static array match_type(array);
        static lambda0v match_type(lambda0v);
        static lambda0f match_type(lambda0f);
        static lambda1v match_type(lambda1v);
        static lambda1f match_type(lambda1f);
        static object match_type(object);
        // Need to override for `char const*`, otherwise `bool` will be chosen
        static std::string match_type(char const*);

    public:

        struct view;
        using pointer = variant_ptr<view>;

        Zz_BUSTACHE_VARIANT_DECL(value, BUSTACHE_VALUE, false)

        value() noexcept : _which(0), _0() {}

        pointer get_pointer() const
        {
            return {_which, _storage};
        }
    };

    struct value::view : variant_base<view>
    {
        using switcher = value::switcher;

#define BUSTACHE_VALUE_VIEW_CTOR(N, U, D)                                       \
        view(U const& data) noexcept : _which(N), _data(&data) {}
        BUSTACHE_VALUE(BUSTACHE_VALUE_VIEW_CTOR,)
#undef BUSTACHE_VALUE_VIEW_CTOR

        view(value const& data) noexcept
          : _which(data._which), _data(data._storage)
        {}

        view(unsigned which, void const* data) noexcept
          : _which(which), _data(data)
        {}

        unsigned which() const
        {
            return _which;
        }

        void const* data() const
        {
            return _data;
        }

        pointer get_pointer() const
        {
            return {_which, _data};
        }

    private:

        unsigned _which;
        void const* _data;
    };
#undef BUSTACHE_VALUE
}

namespace bustache
{
    // Forward decl only.
    template<class CharT, class Traits, class Context>
    void generate_ostream
    (
        std::basic_ostream<CharT, Traits>& out, format const& fmt,
        value::view const& data, Context const& context, option_type flag
    );

    // Forward decl only.
    template<class String, class Context>
    void generate_string
    (
        String& out, format const& fmt,
        value::view const& data, Context const& context, option_type flag
    );

    template<class CharT, class Traits, class T, class Context,
        typename std::enable_if<std::is_constructible<value::view, T>::value, bool>::type = true>
    inline std::basic_ostream<CharT, Traits>&
    operator<<(std::basic_ostream<CharT, Traits>& out, manipulator<T, Context> const& manip)
    {
        generate_ostream(out, manip.fmt, manip.data, detail::any_context(manip.context), manip.flag);
        return out;
    }

    template<class T, class Context,
        typename std::enable_if<std::is_constructible<value::view, T>::value, bool>::type = true>
    inline std::string to_string(manipulator<T, Context> const& manip)
    {
        std::string ret;
        generate_string(ret, manip.fmt, manip.data, detail::any_context(manip.context), manip.flag);
        return ret;
    }
}

#endif