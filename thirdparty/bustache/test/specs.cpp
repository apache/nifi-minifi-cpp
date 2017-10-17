/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2016 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <bustache/model.hpp>
#include <boost/unordered_map.hpp>

using namespace bustache;
using context = boost::unordered_map<std::string, format>;

TEST_CASE("interpolation")
{
    object const empty;

    // No Interpolation
    CHECK(to_string("Hello from {Mustache}!"_fmt(empty)) == "Hello from {Mustache}!");

    // Basic Interpolation
    CHECK(to_string("Hello, {{subject}}!"_fmt(object{{"subject", "world"}})) == "Hello, world!");

    // HTML Escaping
    CHECK(to_string("These characters should be HTML escaped: {{forbidden}}"_fmt(object{{"forbidden", "& \" < >"}}, escape_html))
        == "These characters should be HTML escaped: &amp; &quot; &lt; &gt;");

    // Triple Mustache
    CHECK(to_string("These characters should not be HTML escaped: {{{forbidden}}}"_fmt(object{{"forbidden", "& \" < >"}}, escape_html))
        == "These characters should not be HTML escaped: & \" < >");

    // Ampersand
    CHECK(to_string("These characters should not be HTML escaped: {{&forbidden}}"_fmt(object{{"forbidden", "& \" < >"}}, escape_html))
        == "These characters should not be HTML escaped: & \" < >");

    // Basic Integer Interpolation
    CHECK(to_string(R"("{{mph}} miles an hour!")"_fmt(object{{"mph", 85}})) == R"("85 miles an hour!")");

    // Triple Mustache Integer Interpolation
    CHECK(to_string(R"("{{{mph}}} miles an hour!")"_fmt(object{{"mph", 85}})) == R"("85 miles an hour!")");

    // Ampersand Integer Interpolation
    CHECK(to_string(R"("{{&mph}} miles an hour!")"_fmt(object{{"mph", 85}})) == R"("85 miles an hour!")");

    // Basic Decimal Interpolation
    CHECK(to_string(R"("{{power}} jiggawatts!")"_fmt(object{{"power", 1.21}})) == R"("1.21 jiggawatts!")");

    // Triple Decimal Interpolation
    CHECK(to_string(R"("{{{power}}} jiggawatts!")"_fmt(object{{"power", 1.21}})) == R"("1.21 jiggawatts!")");

    // Ampersand Decimal Interpolation
    CHECK(to_string(R"("{{&power}} jiggawatts!")"_fmt(object{{"power", 1.21}})) == R"("1.21 jiggawatts!")");

    // Context Misses
    {
        // Basic Context Miss Interpolation
        CHECK(to_string("I ({{cannot}}) be seen!"_fmt(empty)) == "I () be seen!");

        // Triple Mustache Context Miss Interpolation
        CHECK(to_string("I ({{{cannot}}}) be seen!"_fmt(empty)) == "I () be seen!");

        // Ampersand Context Miss Interpolation
        CHECK(to_string("I ({{&cannot}}) be seen!"_fmt(empty)) == "I () be seen!");
    }

    // Dotted Names
    {
        // Dotted Names - Basic Interpolation
        CHECK(to_string(R"("{{person.name}}" == "{{#person}}{{name}}{{/person}}")"_fmt(object{{"person", object{{"name", "Joe"}}}})) == R"("Joe" == "Joe")");

        // Dotted Names - Triple Mustache Interpolation
        CHECK(to_string(R"("{{{person.name}}}" == "{{#person}}{{name}}{{/person}}")"_fmt(object{{"person", object{{"name", "Joe"}}}})) == R"("Joe" == "Joe")");

        // Dotted Names - Ampersand Interpolation
        CHECK(to_string(R"("{{&person.name}}" == "{{#person}}{{name}}{{/person}}")"_fmt(object{{"person", object{{"name", "Joe"}}}})) == R"("Joe" == "Joe")");

        // Dotted Names - Arbitrary Depth
        CHECK(to_string(R"("{{a.b.c.d.e.name}}" == "Phil")"_fmt(
            object{{"a", object{{"b", object{{"c", object{{"d", object{{"e", object{{"name", "Phil"}}}}}}}}}}}}))
            == R"("Phil" == "Phil")");

        // Dotted Names - Broken Chains
        CHECK(to_string(R"("{{a.b.c}}" == "")"_fmt(empty)) == R"("" == "")");

        // Dotted Names - Broken Chain Resolution
        CHECK(to_string(R"("{{a.b.c.name}}" == "")"_fmt(
            object{
                {"a", object{{"b", empty}}},
                {"c", object{{"name", "Jim"}}}
            })) == R"("" == "")");

        // Dotted Names - Initial Resolution
        CHECK(to_string(R"("{{#a}}{{b.c.d.e.name}}{{/a}}" == "Phil")"_fmt(
            object{
                {"a", object{{"b", object{{"c", object{{"d", object{{"e", object{{"name", "Phil"}}}}}}}}}}},
                {"c", object{{"c", object{{"d", object{{"e", object{{"name", "Wrong"}}}}}}}}}
        })) == R"("Phil" == "Phil")");

        // Dotted Names - Context Precedence
        CHECK(to_string("{{#a}}{{b.c}}{{/a}}"_fmt(object{{"b", empty}, {"c", "ERROR"}})) == "");
    }

    object s{{"string", "---"}};

    // Whitespace Sensitivity
    {
        // Interpolation - Surrounding Whitespace
        CHECK(to_string("| {{string}} |"_fmt(s)) == "| --- |");

        // Triple Mustache - Surrounding Whitespace
        CHECK(to_string("| {{{string}}} |"_fmt(s)) == "| --- |");

        // Ampersand - Surrounding Whitespace
        CHECK(to_string("| {{&string}} |"_fmt(s)) == "| --- |");

        // Interpolation - Standalone
        CHECK(to_string("  {{string}}\n"_fmt(s)) == "  ---\n");

        // Triple Mustache - Standalone
        CHECK(to_string("  {{{string}}}\n"_fmt(s)) == "  ---\n");

        // Ampersand - Standalone
        CHECK(to_string("  {{&string}}\n"_fmt(s)) == "  ---\n");
    }

    // Whitespace Insensitivity
    {
        // Interpolation With Padding
        CHECK(to_string("|{{ string }}|"_fmt(s)) == "|---|");

        // Triple Mustache With Padding
        CHECK(to_string("|{{{ string }}}|"_fmt(s)) == "|---|");

        // Ampersand With Padding
        CHECK(to_string("|{{& string }}|"_fmt(s)) == "|---|");
    }
}

TEST_CASE("sections")
{
    object const empty;

    // Truthy
    CHECK(to_string(R"("{{#boolean}}This should be rendered.{{/boolean}}")"_fmt(object{{"boolean", true}}))
        == R"("This should be rendered.")");

    // Falsey
    CHECK(to_string(R"("{{#boolean}}This should not be rendered.{{/boolean}}")"_fmt(object{{"boolean", false}}))
        == R"("")");

    // Context
    CHECK(to_string(R"("{{#context}}Hi {{name}}.{{/context}}")"_fmt(object{{"context", object{{"name", "Joe"}}}}))
        == R"("Hi Joe.")");

    // Deeply Nested Contexts
    CHECK(to_string(
        "{{#a}}\n"
        "{{one}}\n"
        "{{#b}}\n"
        "{{one}}{{two}}{{one}}\n"
        "{{#c}}\n"
        "{{one}}{{two}}{{three}}{{two}}{{one}}\n"
        "{{#d}}\n"
        "{{one}}{{two}}{{three}}{{four}}{{three}}{{two}}{{one}}\n"
        "{{#e}}\n"
        "{{one}}{{two}}{{three}}{{four}}{{five}}{{four}}{{three}}{{two}}{{one}}\n"
        "{{/e}}\n"
        "{{one}}{{two}}{{three}}{{four}}{{three}}{{two}}{{one}}\n"
        "{{/d}}\n"
        "{{one}}{{two}}{{three}}{{two}}{{one}}\n"
        "{{/c}}\n"
        "{{one}}{{two}}{{one}}\n"
        "{{/b}}\n"
        "{{one}}\n"
        "{{/a}}"_fmt(object{
            {"a", object{{"one", 1}}},
            {"b", object{{"two", 2}}},
            {"c", object{{"three", 3}}},
            {"d", object{{"four", 4}}},
            {"e", object{{"five", 5}}}}))
        == 
        "1\n"
        "121\n"
        "12321\n"
        "1234321\n"
        "123454321\n"
        "1234321\n"
        "12321\n"
        "121\n"
        "1\n");

    // List
    CHECK(to_string(R"("{{#list}}{{item}}{{/list}}")"_fmt(object{{"list", array{object{{"item", 1}}, object{{"item", 2}}, object{{"item", 3}}}}}))
        == R"("123")");

    // Empty List
    CHECK(to_string(R"("{{#list}}Yay lists!{{/list}}")"_fmt(object{{"list", array{}}})) == R"("")");

    // Doubled
    CHECK(to_string(
        "{{#bool}}\n"
        "* first\n"
        "{{/bool}}\n"
        "* {{two}}\n"
        "{{#bool}}\n"
        "* third\n"
        "{{/bool}}"_fmt(object{{"bool", true}, {"two", "second"}}))
        == 
        "* first\n"
        "* second\n"
        "* third\n");

    // Nested (Truthy)
    CHECK(to_string("| A {{#bool}}B {{#bool}}C{{/bool}} D{{/bool}} E |"_fmt(object{{"bool", true}})) == "| A B C D E |");

    // Nested (Falsey)
    CHECK(to_string("| A {{#bool}}B {{#bool}}C{{/bool}} D{{/bool}} E |"_fmt(object{{"bool", false}})) == "| A  E |");

    // Context Misses
    CHECK(to_string("[{{#missing}}Found key 'missing'!{{/missing}}]"_fmt(empty)) == "[]");

    // Implicit Iterators
    {
        // Implicit Iterator - String
        CHECK(to_string(R"#("{{#list}}({{.}}){{/list}}")#"_fmt(object{{"list", array{1, 2, 3, 4, 5}}})) == R"#("(1)(2)(3)(4)(5)")#");

        // Implicit Iterator - Decimal
        CHECK(to_string(R"#("{{#list}}({{.}}){{/list}}")#"_fmt(object{{"list", array{1.1, 2.2, 3.3, 4.4, 5.5}}})) == R"#("(1.1)(2.2)(3.3)(4.4)(5.5)")#");

        // Implicit Iterator - Array
        CHECK(to_string(R"#("{{#list}}({{#.}}{{.}}{{/.}}){{/list}}")#"_fmt(object{{"list", array{array{1, 2, 3}, array{"a", "b", "c"}}}})) == R"#("(123)(abc)")#");
    }

    // Dotted Names
    {
        // Dotted Names - Truthy
        CHECK(to_string(R"("{{#a.b.c}}Here{{/a.b.c}}" == "Here")"_fmt(object{{"a", object{{"b", object{{"c", true}}}}}})) == R"("Here" == "Here")");

        // Dotted Names - Falsey
        CHECK(to_string(R"("{{#a.b.c}}Here{{/a.b.c}}" == "")"_fmt(object{{"a", object{{"b", object{{"c", false}}}}}})) == R"("" == "")");

        // Dotted Names - Broken Chains
        CHECK(to_string(R"("{{#a.b.c}}Here{{/a.b.c}}" == "")"_fmt(object{{"a", empty}})) == R"("" == "")");
    }

    object const o{{"boolean", true}};

    // Whitespace Sensitivity
    {
        // Surrounding Whitespace
        CHECK(to_string(" | {{#boolean}}\t|\t{{/boolean}} | \n"_fmt(o)) == " | \t|\t | \n");

        // Internal Whitespace
        CHECK(to_string(" | {{#boolean}} {{! Important Whitespace }}\n {{/boolean}} | \n"_fmt(o)) == " |  \n  | \n");

        // Indented Inline Sections
        CHECK(to_string(" {{#boolean}}YES{{/boolean}}\n {{#boolean}}GOOD{{/boolean}}\n"_fmt(o)) == " YES\n GOOD\n");

        // Standalone Lines
        CHECK(to_string(
            "| This Is\n"
            "{{#boolean}}\n"
            "|\n"
            "{{/boolean}}\n"
            "| A Line"_fmt(o))
            ==
            "| This Is\n"
            "|\n"
            "| A Line");

        // Indented Standalone Lines
        CHECK(to_string(
            "| This Is\n"
            "  {{#boolean}}\n"
            "|\n"
            "  {{/boolean}}\n"
            "| A Line"_fmt(o))
            ==
            "| This Is\n"
            "|\n"
            "| A Line");

        //  Standalone Line Endings
        CHECK(to_string("|\r\n{{#boolean}}\r\n{{/boolean}}\r\n|"_fmt(o)) == "|\r\n|");

        // Standalone Without Previous Line
        CHECK(to_string("  {{#boolean}}\n#{{/boolean}}\n/"_fmt(o)) == "#\n/");

        // Standalone Without Newline
        CHECK(to_string("#{{#boolean}}\n/\n  {{/boolean}}"_fmt(o)) == "#\n/\n");
    }

    // Whitespace Insensitivity
    {
        CHECK(to_string("|{{# boolean }}={{/ boolean }}|"_fmt(o)) == "|=|");
    }
}

TEST_CASE("inverted")
{
    object const empty;

    // Falsey
    CHECK(to_string(R"("{{^boolean}}This should be rendered.{{/boolean}}")"_fmt(object{{"boolean", false}})) == R"("This should be rendered.")");

    // Truthy
    CHECK(to_string(R"("{{^boolean}}This should not be rendered.{{/boolean}}")"_fmt(object{{"boolean", true}})) == R"("")");

    // Context
    CHECK(to_string(R"("{{^context}}Hi {{name}}.{{/context}}")"_fmt(object{{"context", object{{"name", "Joe"}}}})) == R"("")");

    // List
    CHECK(to_string(R"("{{^list}}{{n}}{{/list}}")"_fmt(object{{"list", array{object{{"n", 1}}, object{{"n", 2}}, object{{"n", 3}}}}})) == R"("")");

    // Empty List
    CHECK(to_string(R"("{{^list}}Yay lists!{{/list}}")"_fmt(object{{"list", array{}}})) == R"("Yay lists!")");

    // Doubled
    CHECK(to_string(
        "{{^bool}}\n"
        "* first\n"
        "{{/bool}}\n"
        "* {{two}}\n"
        "{{^bool}}\n"
        "* third\n"
        "{{/bool}}"_fmt(object{{"bool", false}, {"two", "second"}}))
        ==
        "* first\n"
        "* second\n"
        "* third\n");

    // Nested (Falsey)
    CHECK(to_string("| A {{^bool}}B {{^bool}}C{{/bool}} D{{/bool}} E |"_fmt(object{{"bool", false}})) == "| A B C D E |");

    // Nested (Truthy)
    CHECK(to_string("| A {{^bool}}B {{^bool}}C{{/bool}} D{{/bool}} E |"_fmt(object{{"bool", true}})) == "| A  E |");

    // Context Misses
    CHECK(to_string("[{{^missing}}Cannot find key 'missing'!{{/missing}}]"_fmt(empty)) == "[Cannot find key 'missing'!]");

    // Dotted Names
    {
        // Dotted Names - Truthy
        CHECK(to_string(R"("{{^a.b.c}}Not Here{{/a.b.c}}" == "")"_fmt(object{{"a", object{{"b", object{{"c", true}}}}}})) == R"("" == "")");

        // Dotted Names - Falsey
        CHECK(to_string(R"("{{^a.b.c}}Not Here{{/a.b.c}}" == "Not Here")"_fmt(object{{"a", object{{"b", object{{"c", false}}}}}})) == R"("Not Here" == "Not Here")");

        // Dotted Names - Broken Chains
        CHECK(to_string(R"("{{^a.b.c}}Not Here{{/a.b.c}}" == "Not Here")"_fmt(object{{"a", empty}})) == R"("Not Here" == "Not Here")");
    }

    object const o{{"boolean", false}};

    // Whitespace Sensitivity
    {
        // Surrounding Whitespace
        CHECK(to_string(" | {{^boolean}}\t|\t{{/boolean}} | \n"_fmt(o)) == " | \t|\t | \n");

        // Internal Whitespace
        CHECK(to_string(" | {{^boolean}} {{! Important Whitespace }}\n {{/boolean}} | \n"_fmt(o)) == " |  \n  | \n");

        // Indented Inline Sections
        CHECK(to_string(" {{^boolean}}YES{{/boolean}}\n {{^boolean}}GOOD{{/boolean}}\n"_fmt(o)) == " YES\n GOOD\n");

        // Standalone Lines
        CHECK(to_string(
            "| This Is\n"
            "{{^boolean}}\n"
            "|\n"
            "{{/boolean}}\n"
            "| A Line"_fmt(o))
            ==
            "| This Is\n"
            "|\n"
            "| A Line");

        // Indented Standalone Lines
        CHECK(to_string(
            "| This Is\n"
            "  {{^boolean}}\n"
            "|\n"
            "  {{/boolean}}\n"
            "| A Line"_fmt(o))
            ==
            "| This Is\n"
            "|\n"
            "| A Line");

        //  Standalone Line Endings
        CHECK(to_string("|\r\n{{^boolean}}\r\n{{/boolean}}\r\n|"_fmt(o)) == "|\r\n|");

        // Standalone Without Previous Line
        CHECK(to_string("  {{^boolean}}\n#{{/boolean}}\n/"_fmt(o)) == "#\n/");

        // Standalone Without Newline
        CHECK(to_string("#{{^boolean}}\n/\n  {{/boolean}}"_fmt(o)) == "#\n/\n");
    }

    // Whitespace Insensitivity
    {
        CHECK(to_string("|{{^ boolean }}={{/ boolean }}|"_fmt(o)) == "|=|");
    }
}

TEST_CASE("delimiters")
{
    // Pair Behavior
    CHECK(to_string("{{=<% %>=}}(<%text%>)"_fmt(object{{"text", "Hey!"}})) == "(Hey!)");

    // Special Characters
    CHECK(to_string("({{=[ ]=}}[text])"_fmt(object{{"text", "It worked!"}})) == "(It worked!)");

    // Sections
    CHECK(to_string(
        "[\n"
        "{{#section}}\n"
        "  {{data}}\n"
        "  |data|\n"
        "{{/section}}\n"
        "{{= | | =}}\n"
        "|#section|\n"
        "  {{data}}\n"
        "  |data|\n"
        "|/section|\n"
        "]"_fmt(object{{"section", true}, {"data", "I got interpolated."}}))
        == 
        "[\n"
        "  I got interpolated.\n"
        "  |data|\n"
        "  {{data}}\n"
        "  I got interpolated.\n"
        "]");

    // Inverted Sections
    CHECK(to_string(
        "[\n"
        "{{^section}}\n"
        "  {{data}}\n"
        "  |data|\n"
        "{{/section}}\n"
        "{{= | | =}}\n"
        "|^section|\n"
        "  {{data}}\n"
        "  |data|\n"
        "|/section|\n"
        "]"_fmt(object{{"section", false},{"data", "I got interpolated."}}))
        ==
        "[\n"
        "  I got interpolated.\n"
        "  |data|\n"
        "  {{data}}\n"
        "  I got interpolated.\n"
        "]");

    // Partial Inheritence
    CHECK(to_string(
        "[ {{>include}} ]\n"
        "{{= | | =}}\n"
        "[ |>include| ]"_fmt(object{{"value", "yes"}}, context{{"include", ".{{value}}."_fmt}}))
        ==
        "[ .yes. ]\n"
        "[ .yes. ]");

    // Post-Partial Behavior
    CHECK(to_string(
        "[ {{>include}} ]\n"
        "[ .{{value}}.  .|value|. ]"_fmt(object{{"value", "yes"}}, context{{"include", ".{{value}}. {{= | | =}} .|value|."_fmt}}))
        ==
        "[ .yes.  .yes. ]\n"
        "[ .yes.  .|value|. ]");

    object const empty;

    // Whitespace Sensitivity
    {
        // Surrounding Whitespace
        CHECK(to_string("| {{=@ @=}} |"_fmt(empty)) == "|  |");

        // Outlying Whitespace (Inline)
        CHECK(to_string(" | {{=@ @=}}\n"_fmt(empty)) == " | \n");

        // Standalone Tag
        CHECK(to_string(
            "Begin.\n"
            "{{=@ @=}}\n"
            "End."_fmt(empty))
            == 
            "Begin.\n"
            "End.");

        // Indented Standalone Tag
        CHECK(to_string(
            "Begin.\n"
            "  {{=@ @=}}\n"
            "End."_fmt(empty))
            ==
            "Begin.\n"
            "End.");

        // Standalone Line Endings
        CHECK(to_string("|\r\n{{= @ @ =}}\r\n|"_fmt(empty)) == "|\r\n|");

        // Standalone Without Previous Line
        CHECK(to_string("  {{=@ @=}}\n="_fmt(empty)) == "=");

        // Standalone Without Newline
        CHECK(to_string("=\n  {{=@ @=}}"_fmt(empty)) == "=\n");
    }

    // Whitespace Insensitivity
    {
        // Pair with Padding
        CHECK(to_string("|{{= @   @ =}}|"_fmt(empty)) == "||");
    }
}

TEST_CASE("comments")
{
    object const empty;

    // Inline
    CHECK(to_string("12345{{! Comment Block! }}67890"_fmt(empty)) == "1234567890");

    // Multiline
    CHECK(to_string(
        "12345{{!\n"
        "  This is a\n"
        "  multi-line comment...\n"
        "}}67890"_fmt(empty))
        == 
        "1234567890");

    // Standalone
    CHECK(to_string(
        "Begin.\n"
        "{{! Comment Block! }}\n"
        "End."_fmt(empty))
        ==
        "Begin.\n"
        "End.");

    // Indented Standalone
    CHECK(to_string(
        "Begin.\n"
        "  {{! Comment Block! }}\n"
        "End."_fmt(empty))
        ==
        "Begin.\n"
        "End.");

    // Standalone Line Endings
    CHECK(to_string("|\r\n{{! Standalone Comment }}\r\n|"_fmt(empty)) == "|\r\n|");

    // Standalone Without Previous Line
    CHECK(to_string("  {{! I'm Still Standalone }}\n!"_fmt(empty)) == "!");

    // Standalone Without Newline
    CHECK(to_string("!\n  {{! I'm Still Standalone }}"_fmt(empty)) == "!\n");

    // Multiline Standalone
    CHECK(to_string(
        "Begin.\n"
        "{{!\n"
        "Something's going on here...\n"
        "}}\n"
        "End."_fmt(empty))
        ==
        "Begin.\n"
        "End.");

    // Indented Multiline Standalone
    CHECK(to_string(
        "Begin.\n"
        "  {{!\n"
        "    Something's going on here...\n"
        "  }}\n"
        "End."_fmt(empty))
        ==
        "Begin.\n"
        "End.");

    // Indented Inline
    CHECK(to_string("  12 {{! 34 }}\n"_fmt(empty)) == "  12 \n");

    // Surrounding Whitespace
    CHECK(to_string("12345 {{! Comment Block! }} 67890"_fmt(empty)) == "12345  67890");
}

TEST_CASE("partials")
{
    object const empty;

    // Basic Behavior
    CHECK(to_string(R"("{{>text}}")"_fmt(empty, context{{"text", "from partial"_fmt}})) == R"("from partial")");

    // Failed Lookup
    CHECK(to_string(R"("{{>text}}")"_fmt(empty)) == R"("")");

    // Context
    CHECK(to_string(R"("{{>partial}}")"_fmt(object{{"text", "content"}}, context{{"partial", "*{{text}}*"_fmt}})) == R"("*content*")");

    // Recursion
    CHECK(to_string("{{>node}}"_fmt(object{
        {"content", "X"},
        {"nodes", array{object{{"content", "Y"}, {"nodes", array{}}}}}
    }, context{{"node", "{{content}}<{{#nodes}}{{>node}}{{/nodes}}>"_fmt}})) == "X<Y<>>");

    // Whitespace Sensitivity
    {
        // Surrounding Whitespace
        CHECK(to_string("| {{>partial}} |"_fmt(empty, context{{"partial", "\t|\t"_fmt}})) == "| \t|\t |");

        // Inline Indentation
        CHECK(to_string("  {{data}}  {{> partial}}\n"_fmt(object{{"data", "|"}}, context{{"partial", ">\n>"_fmt}})) == "  |  >\n>\n");

        // Standalone Line Endings
        CHECK(to_string("|\r\n{{>partial}}\r\n|"_fmt(empty, context{{"partial", ">"_fmt}})) == "|\r\n>|");

        // Standalone Without Previous Line
        CHECK(to_string("  {{>partial}}\n>"_fmt(empty, context{{"partial", ">\n>"_fmt}})) == "  >\n  >>");

        // Standalone Without Newline
        CHECK(to_string(">\n  {{>partial}}"_fmt(empty, context{{"partial", ">\n>"_fmt}})) == ">\n  >\n  >");

        // Standalone Indentation
        CHECK(to_string(
            "\\\n"
            " {{>partial}}\n"
            "/"_fmt(object{{"content", "<\n->"}},
                context{{"partial",
                "|\n"
                "{{{content}}}\n"
                "|\n"_fmt}}))
            == 
            "\\\n"
            " |\n"
            " <\n"
            "->\n"
            " |\n"
            "/");
    }

    // Whitespace Insensitivity
    {
        // Padding Whitespace
        CHECK(to_string("|{{> partial }}|"_fmt(empty, context{{"partial", "[]"_fmt}})) == "|[]|");
    }
}

TEST_CASE("lambdas")
{
    // Interpolation
    CHECK(to_string("Hello, {{lambda}}!"_fmt(object{{"lambda", [] { return "world"; }}})) == "Hello, world!");

    // Interpolation - Expansion
    CHECK(to_string(
        "Hello, {{lambda}}!"_fmt(object{
            {"lambda", [] { return "{{planet}}"_fmt; }},
            {"planet", "world"}}))
        ==
        "Hello, world!");

    // Interpolation - Alternate Delimiters
    CHECK(to_string(
        "{{= | | =}}\nHello, (|&lambda|)!"_fmt(object{
            {"lambda", [] { return "|planet| => {{planet}}"_fmt; }},
            {"planet", "world"}}))
        ==
        "Hello, (|planet| => world)!");

    // Interpolation - Multiple Calls
    CHECK(to_string(
        "{{lambda}} == {{{lambda}}} == {{lambda}}"_fmt(object{
            {"lambda", [n = 0]() mutable { return ++n; }}}))
        ==
        "1 == 2 == 3");

    // Escaping
    CHECK(to_string("<{{lambda}}{{{lambda}}}"_fmt(object{{"lambda", [] { return ">"; }}}, escape_html)) == "<&gt;>");

    // Section - Expansion
    CHECK(to_string("<{{#lambda}}-{{/lambda}}>"_fmt(object{
        {"lambda", [](ast::content_list const& contents) {
            ast::content_list list;
            list.insert(list.end(), contents.begin(), contents.end());
            list.push_back(ast::variable{"planet"});
            list.insert(list.end(), contents.begin(), contents.end());
            return format(std::move(list), false);
        }},
        {"planet", "Earth"}}))
        ==
        "<-Earth->");

    // Section - Multiple Calls
    CHECK(to_string("{{#lambda}}FILE{{/lambda}} != {{#lambda}}LINE{{/lambda}}"_fmt(object{
        {"lambda", [](ast::content_list const& contents) {
            ast::content_list list;
            list.push_back(ast::text("__"));
            list.insert(list.end(), contents.begin(), contents.end());
            list.push_back(ast::text("__"));
            return format(std::move(list), false);
        }}}))
        ==
        "__FILE__ != __LINE__");

    // Inverted Section
    CHECK(to_string("<{{^lambda}}{{static}}{{/lambda}}>"_fmt(object{
        {"lambda", [](ast::content_list const&) { return false; }},
        {"static", "static"}}))
        ==
        "<>");
}