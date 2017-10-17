#include <benchmark/benchmark.h>
#include <bustache/model.hpp>
#include <mstch/mstch.hpp>
#include <mustache.hpp>

static char tmp[] =
R"(<h1>{{header}}</h1>
{{#bug}}
{{/bug}}

{{# items}}
  {{#first}}
    <li><strong>{{name}}</strong></li>
  {{/first}}
  {{#link}}
    <li><a {{>href}}>{{name}}</a></li>
  {{/link}}
{{ /items}}

{{#empty}}
  <p>The list is empty.</p>
{{/ empty }}

{{=[ ]=}}

[#array]([.])[/array]

[#items]
[count]->[count]->[count]
[/items]

[a.b.c] == [#a][#b][c][/b][/a]

<div class="comments">
    <h3>[header]</h3>
    <ul>
        [#comments]
        <li class="comment">
            <h5>[name]</h5>
            <p>[body]</p>
        </li>
        <!--[count]-->
        [/comments]
    </ul>
</div>)";

static void bustache_usage(benchmark::State& state)
{
    using namespace bustache;

    boost::unordered_map<std::string, bustache::format> context
    {
        {"href", "href=\"{{url}}\""_fmt}
    };

    int n = 0;
    object data
    {
        {"header", "Colors"},
        {"items",
            array
            {
                object
                {
                    {"name", "red"},
                    {"first", true},
                    {"url", "#Red"}
                },
                object
                {
                    {"name", "green"},
                    {"link", true},
                    {"url", "#Green"}
                },
                object
                {
                    {"name", "blue"},
                    {"link", true},
                    {"url", "#Blue"}
                }
            }
        },
        {"empty", false},
        {"count", [&n] { return ++n; }},
        {"array", array{1, 2, 3}},
        {"a", object{{"b", object{{"c", true}}}}},
        {"comments",
            array
            {
                object
                {
                    {"name", "Joe"},
                    {"body", "<html> should be escaped"}
                },
                object
                {
                    {"name", "Sam"},
                    {"body", "{{mustache}} can be seen"}
                },
                object
                {
                    {"name", "New"},
                    {"body", "break\nup"}
                }
            }
        }
    };

    format fmt(tmp);

    while (state.KeepRunning())
    {
        n = 0;
        to_string(fmt(data, context, escape_html));
    }
}

static void mstch_usage(benchmark::State& state)
{
    using namespace mstch;
    using namespace std::string_literals;

    std::map<std::string, std::string> context
    {
        {"href", "href=\"{{url}}\""}
    };

    int n = 0;
    map data
    {
        {"header", "Colors"s},
        {"items",
            array
            {
                map
                {
                    {"name", "red"s},
                    {"first", true},
                    {"url", "#Red"s}
                },
                map
                {
                    {"name", "green"s},
                    {"link", true},
                    {"url", "#Green"s}
                },
                map
                {
                    {"name", "blue"s},
                    {"link", true},
                    {"url", "#Blue"s}
                }
            }
        },
        {"empty", false},
        {"count", lambda{[&n]() -> node { return ++n; }}},
        {"array", array{1, 2, 3}},
        {"a", map{{"b", map{{"c", true}}}}},
        {"comments",
            array
            {
                map
                {
                    {"name", "Joe"s},
                    {"body", "<html> should be escaped"s}
                },
                map
                {
                    {"name", "Sam"s},
                    {"body", "{{mustache}} can be seen"s}
                },
                map
                {
                    {"name", "New"s},
                    {"body", "break\nup"s}
                }
            }
        }
    };

    while (state.KeepRunning())
    {
        n = 0;
        render(tmp, data, context);
    }
}

static void kainjow_usage(benchmark::State& state)
{
    using namespace Kainjow;
    using Data = Mustache::Data;

    int n = 0;
    Data data;
    data.set("header", "Colors");
    {
        Data d1, d2, d3;
        d1.set("name", "red");
        d1.set("first", Data::Type::True);
        d1.set("url", "#Red");
        d2.set("name", "green");
        d2.set("link", Data::Type::True);
        d2.set("url", "#Green");
        d3.set("name", "blue");
        d3.set("link", Data::Type::True);
        d3.set("url", "#Blue");
        data.set("items", Data::ListType{d1, d2, d3});
    }
    data.set("empty", Data::Type::False);
    data.set("count", Data::LambdaType{[&n](const std::string&) { return std::to_string(++n); }});
    data.set("array", Data::ListType{"1", "2", "3"});
    data.set("a", {"b",{"c", "true"}});
    {
        Data d1, d2, d3;
        d1.set("name", "Joe");
        d1.set("body", "<html> should be escaped");
        d2.set("name", "Sam");
        d2.set("body", "{{mustache}} can be seen");
        d3.set("name", "New");
        d3.set("body", "break\nup");
        data.set("comments", Data::ListType{d1, d2, d3});
    }
    data.set("href", Data::PartialType{[]() { return "href=\"{{url}}\""; }});

    Mustache fmt(tmp);

    while (state.KeepRunning())
    {
        n = 0;
        fmt.render(data);
    }
}

BENCHMARK(bustache_usage);
BENCHMARK(mstch_usage);
BENCHMARK(kainjow_usage);

BENCHMARK_MAIN();