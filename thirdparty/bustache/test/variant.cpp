/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2016-2017 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <bustache/detail/variant.hpp>

struct BadCopyError {};

struct BadCopy
{
    BadCopy() = default;

    BadCopy(BadCopy&&) = default;

    BadCopy(BadCopy const&)
    {
        throw BadCopyError();
    }

    BadCopy& operator=(BadCopy const&)
    {
        throw BadCopyError();
    }

    BadCopy& operator=(BadCopy&&) = default;
};

struct A
{
    A() = default;

    A(A&& other) noexcept
    {
        other.moved = true;
    }

    A(A const&) = default;

    A& operator=(A&& other) noexcept
    {
        assigned = true;
        other.moved = true;
        return *this;
    }

    A& operator=(A const& other) noexcept
    {
        assigned = true;
        return *this;
    }

    bool assigned = false;
    bool moved = false;
};

struct GoodInt
{
    operator int() const noexcept
    {
        return 0;
    }
};

struct BadIntError {};

struct BadInt
{
    operator int() const
    {
        throw BadIntError();
    }
};

struct Visitor
{
    unsigned operator()(bool const&) const
    {
        return 0;
    }

    unsigned operator()(int const&) const
    {
        return 1;
    }

    unsigned operator()(A const&) const
    {
        return 2;
    }

    unsigned operator()(BadCopy const&) const
    {
        return 3;
    }
};

namespace bustache
{
#define VAR(X, D)                                                               \
    X(0, bool, D)                                                               \
    X(1, int, D)                                                                \
    X(2, A, D)                                                                  \
    X(3, BadCopy, D)                                                            \
/***/
    class Var : public variant_base<Var>
    {
        VAR(Zz_BUSTACHE_VARIANT_MATCH,)
    public:
        Zz_BUSTACHE_VARIANT_DECL(Var, VAR, false)

        Var() noexcept : _which(0), _0() {}
    };
#undef VAR
}

using namespace bustache;

TEST_CASE("variant-ctor")
{
    {
        Var v;
        CHECK(v.valid());
        CHECK(v.which() == 0);
        Var v2(v);
        CHECK(v.which() == 0);
    }
    {
        Var v(true);
        CHECK(v.valid());
        CHECK(v.which() == 0);
        Var v2(v);
        CHECK(v.which() == 0);
    }
    {
        Var v(0);
        CHECK(v.valid());
        CHECK(v.which() == 1);
        Var v2(v);
        CHECK(v.which() == 1);
    }
    {
        Var v(A{});
        CHECK(v.valid());
        CHECK(v.which() == 2);
        Var v2(v);
        CHECK(v.which() == 2);
    }
    {
        Var v(BadCopy{});
        CHECK(v.valid());
        CHECK(v.which() == 3);
        CHECK_THROWS_AS(Var{v}, BadCopyError);
    }
    {   // Test convertible.
        Var v(GoodInt{});
        CHECK(v.valid());
        CHECK(v.which() == 1);
    }
    {
        Var v1(A{});
        CHECK(v1.which() == 2);
        Var v2(std::move(v1));
        CHECK(v1.which() == 2);
        CHECK(v2.which() == 2);
        CHECK(get<A>(v1).moved == true);
    }
}

TEST_CASE("variant-access")
{
    Var v;
    CHECK(v.which() == 0);
    CHECK(get<bool>(&v) != nullptr);
    CHECK(get<bool>(v) == false);
    CHECK(get<int>(&v) == nullptr);
    CHECK_THROWS_AS(get<int>(v), bad_variant_access);
    v = 1024;
    CHECK(v.which() == 1);
    CHECK(get<int>(&v) != nullptr);
    CHECK(get<int>(v) == 1024);
    get<int>(v) = true;
    CHECK(v.which() == 1);
    CHECK(get<int>(v) == 1);
    v = true;
    CHECK(v.which() == 0);
    CHECK(get<bool>(v) == true);
    CHECK_THROWS_AS(get<A>(v), bad_variant_access);
    {
        REQUIRE(v.which() != 2);
        auto& a = v = A();
        CHECK(v.which() == 2);
        CHECK(get<A>(&v) != nullptr);
        CHECK(get<A>(&v) == &a);
        CHECK(!a.assigned);
    }
    {
        REQUIRE(v.which() == 2);
        auto& b = v = A();
        CHECK(v.which() == 2);
        CHECK(get<A>(&v) == &b);
        CHECK(b.assigned);
    }
}

TEST_CASE("variant-valuess-by-exception")
{
    Var v;
    CHECK(v.valid());
    CHECK_THROWS_AS(v = BadInt(), BadIntError);
    CHECK(v.which() != 0);
    CHECK(!v.valid());
    v = 42;
    CHECK(v.valid());

    Var v2(BadCopy{});
    CHECK_THROWS_AS(v = v2, BadCopyError);
    CHECK(!v.valid());
    CHECK(v2.which() == 3);
}

TEST_CASE("variant-visit")
{
    Visitor v;
    CHECK(visit(v, Var{}) == 0);
    CHECK(visit(v, Var{true}) == 0);
    CHECK(visit(v, Var{0}) == 1);
    CHECK(visit(v, Var{A{}}) == 2);
    CHECK(visit(v, Var{BadCopy{}}) == 3);
}