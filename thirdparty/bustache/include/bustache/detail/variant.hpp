/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2016-2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef BUSTACHE_DETAIL_VARIANT_HPP_INCLUDED
#define BUSTACHE_DETAIL_VARIANT_HPP_INCLUDED

#include <cassert>
#include <cstdlib>
#include <utility>
#include <stdexcept>
#include <type_traits>

namespace bustache { namespace detail
{
    template<class T>
    inline T& cast(void* data)
    {
        return *static_cast<T*>(data);
    }

    template<class T>
    inline T const& cast(void const* data)
    {
        return *static_cast<T const*>(data);
    }

    template<class T, class U>
    struct noexcept_ctor_assign
    {
        static constexpr bool value =
            std::is_nothrow_constructible<T, U>::value &&
            std::is_nothrow_assignable<T, U>::value;
    };

    struct ctor_visitor
    {
        using result_type = void;

        void* data;

        template<class T>
        void operator()(T& t) const
        {
            new(data) T(std::move(t));
        }

        template<class T>
        void operator()(T const& t) const
        {
            new(data) T(t);
        }
    };

    struct assign_visitor
    {
        using result_type = void;

        void* data;

        template<class T>
        void operator()(T& t) const
        {
            *static_cast<T*>(data) = std::move(t);
        }

        template<class T>
        void operator()(T const& t) const
        {
            *static_cast<T*>(data) = t;
        }
    };

    struct dtor_visitor
    {
        using result_type = void;

        template<class T>
        void operator()(T& t) const
        {
            t.~T();
        }
    };

    template<class T>
    struct type {};
}}

namespace bustache
{
    template<class T>
    struct variant_base {};

    template<class View>
    struct variant_ptr
    {
        variant_ptr() noexcept : _data() {}

        variant_ptr(std::nullptr_t) noexcept : _data() {}

        variant_ptr(unsigned which, void const* data) noexcept
            : _which(which), _data(data)
        {}

        explicit operator bool() const
        {
            return !!_data;
        }

        View operator*() const
        {
            return{_which, _data};
        }

        unsigned which() const
        {
            return _which;
        }

        void const* data() const
        {
            return _data;
        }

    private:

        unsigned _which;
        void const* _data;
    };

    class bad_variant_access : public std::exception
    {
    public:
        bad_variant_access() noexcept {}

        const char* what() const noexcept override
        {
            return "bustache::bad_variant_access";
        }
    };
    
    template<class Visitor, class Var>
    inline auto visit(Visitor&& visitor, variant_base<Var>& v) ->
        decltype(Var::switcher::common_ret((void*)nullptr, visitor))
    {
        auto& var = static_cast<Var&>(v);
        return Var::switcher::visit(var.which(), var.data(), visitor);
    }

    template<class Visitor, class Var>
    inline auto visit(Visitor&& visitor, variant_base<Var> const& v) ->
        decltype(Var::switcher::common_ret((void const*)nullptr, visitor))
    {
        auto& var = static_cast<Var const&>(v);
        return Var::switcher::visit(var.which(), var.data(), visitor);
    }

    // Synomym of visit (for Boost.Variant compatibility)
    template<class Visitor, class Var>
    inline auto apply_visitor(Visitor&& visitor, variant_base<Var>& v) ->
        decltype(Var::switcher::common_ret((void*)nullptr, visitor))
    {
        return visit(std::forward<Visitor>(visitor), v);
    }

    template<class Visitor, class Var>
    inline auto apply_visitor(Visitor&& visitor, variant_base<Var> const& v) ->
        decltype(Var::switcher::common_ret((void const*)nullptr, visitor))
    {
        return visit(std::forward<Visitor>(visitor), v);
    }

    template<class T, class Var>
    inline T& get(variant_base<Var>& v)
    {
        auto& var = static_cast<Var&>(v);
        if (Var::switcher::index(detail::type<T>{}) == var.which())
            return *static_cast<T*>(var.data());
        throw bad_variant_access();
    }

    template<class T, class Var>
    inline T const& get(variant_base<Var> const& v)
    {
        auto& var = static_cast<Var const&>(v);
        if (Var::switcher::index(detail::type<T>{}) == var.which())
            return *static_cast<T const*>(var.data());
        throw bad_variant_access();
    }

    template<class T, class Var>
    inline T* get(variant_base<Var>* vp)
    {
        if (vp)
        {
            auto v = static_cast<Var*>(vp);
            if (Var::switcher::index(detail::type<T>{}) == v->which())
                return static_cast<T*>(v->data());
        }
        return nullptr;
    }

    template<class T, class Var>
    inline T const* get(variant_base<Var> const* vp)
    {
        if (vp)
        {
            auto v = static_cast<Var const*>(vp);
            if (Var::switcher::index(detail::type<T>{}) == v->which())
                return static_cast<T const*>(v->data());
        }
        return nullptr;
    }

    template<class T, class Var>
    inline T const* get(variant_ptr<Var> const& vp)
    {
        if (vp)
        {
            if (Var::switcher::index(detail::type<T>{}) == vp.which())
                return static_cast<T const*>(vp.data());
        }
        return nullptr;
    }
}

#define Zz_BUSTACHE_UNREACHABLE(MSG) { assert(!MSG); std::abort(); }
#define Zz_BUSTACHE_VARIANT_SWITCH(N, U, D) case N: return v(detail::cast<U>(data));
#define Zz_BUSTACHE_VARIANT_RET(N, U, D) true ? v(detail::cast<U>(data)) :
#define Zz_BUSTACHE_VARIANT_MEMBER(N, U, D) U _##N;
#define Zz_BUSTACHE_VARIANT_CTOR(N, U, D)                                       \
D(U val) noexcept : _which(N), _##N(std::move(val)) {}
/***/
#define Zz_BUSTACHE_VARIANT_INDEX(N, U, D)                                      \
static constexpr unsigned index(detail::type<U>) { return N; }                  \
/***/
#define Zz_BUSTACHE_VARIANT_MATCH(N, U, D) static U match_type(U);
#define Zz_BUSTACHE_VARIANT_DECL(VAR, TYPES, NOEXCPET)                          \
struct switcher                                                                 \
{                                                                               \
    template<class T, class Visitor>                                            \
    static auto common_ret(T* data, Visitor& v) ->                              \
        decltype(TYPES(Zz_BUSTACHE_VARIANT_RET,) throw bad_variant_access());   \
    template<class T, class Visitor>                                            \
    static auto visit(unsigned which, T* data, Visitor& v) ->                   \
        decltype(common_ret(data, v))                                           \
    {                                                                           \
        switch (which)                                                          \
        {                                                                       \
        TYPES(Zz_BUSTACHE_VARIANT_SWITCH,)                                      \
        default: throw bad_variant_access();                                    \
        }                                                                       \
    }                                                                           \
    TYPES(Zz_BUSTACHE_VARIANT_INDEX,)                                           \
};                                                                              \
private:                                                                        \
unsigned _which;                                                                \
union                                                                           \
{                                                                               \
    char _storage[1];                                                           \
    TYPES(Zz_BUSTACHE_VARIANT_MEMBER,)                                          \
};                                                                              \
void invalidate()                                                               \
{                                                                               \
    if (valid())                                                                \
    {                                                                           \
        detail::dtor_visitor v;                                                 \
        switcher::visit(_which, data(), v);                                     \
        _which = ~0u;                                                           \
    }                                                                           \
}                                                                               \
template<class T>                                                               \
void do_init(T& other)                                                          \
{                                                                               \
    detail::ctor_visitor v{_storage};                                           \
    switcher::visit(other._which, other.data(), v);                             \
}                                                                               \
template<class T>                                                               \
void do_assign(T& other)                                                        \
{                                                                               \
    if (_which == other._which)                                                 \
    {                                                                           \
        detail::assign_visitor v{_storage};                                     \
        switcher::visit(other._which, other.data(), v);                         \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        invalidate();                                                           \
        if (other.valid())                                                      \
        {                                                                       \
            do_init(other);                                                     \
            _which = other._which;                                              \
        }                                                                       \
    }                                                                           \
}                                                                               \
public:                                                                         \
unsigned which() const                                                          \
{                                                                               \
    return _which;                                                              \
}                                                                               \
bool valid() const                                                              \
{                                                                               \
    return _which != ~0u;                                                       \
}                                                                               \
void* data()                                                                    \
{                                                                               \
    return _storage;                                                            \
}                                                                               \
void const* data() const                                                        \
{                                                                               \
    return _storage;                                                            \
}                                                                               \
VAR(VAR&& other) noexcept(NOEXCPET) : _which(other._which)                      \
{                                                                               \
    do_init(other);                                                             \
}                                                                               \
VAR(VAR const& other) : _which(other._which)                                    \
{                                                                               \
    do_init(other);                                                             \
}                                                                               \
template<class T, class U = decltype(match_type(std::declval<T>()))>            \
VAR(T&& other) noexcept(std::is_nothrow_constructible<U, T>::value)             \
  : _which(switcher::index(detail::type<U>{}))                                  \
{                                                                               \
    new(_storage) U(std::forward<T>(other));                                    \
}                                                                               \
~VAR()                                                                          \
{                                                                               \
    if (valid())                                                                \
    {                                                                           \
        detail::dtor_visitor v;                                                 \
        switcher::visit(_which, data(), v);                                     \
    }                                                                           \
}                                                                               \
template<class T, class U = decltype(match_type(std::declval<T>()))>            \
U& operator=(T&& other) noexcept(detail::noexcept_ctor_assign<U, T>::value)     \
{                                                                               \
    if (switcher::index(detail::type<U>{}) == _which)                           \
        return *static_cast<U*>(data()) = std::forward<T>(other);               \
    else                                                                        \
    {                                                                           \
        invalidate();                                                           \
        auto p = new(_storage) U(std::forward<T>(other));                       \
        _which = switcher::index(detail::type<U>{});                            \
        return *p;                                                              \
    }                                                                           \
}                                                                               \
VAR& operator=(VAR&& other) noexcept(NOEXCPET)                                  \
{                                                                               \
    do_assign(other);                                                           \
    return *this;                                                               \
}                                                                               \
VAR& operator=(VAR const& other)                                                \
{                                                                               \
    do_assign(other);                                                           \
    return *this;                                                               \
}                                                                               \
/***/

#endif