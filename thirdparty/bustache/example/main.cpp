#include <iostream>
#include <bustache/model.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

int main()
{
    using bustache::object;
    using bustache::array;
    using namespace bustache::literals;

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

    try
    {
        boost::iostreams::mapped_file_source file("in.mustache");
        bustache::format format(file);
        std::cout << "-----------------------\n";
        std::cout << format(data, context, bustache::escape_html) << "\n";
        std::cout << "-----------------------\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what();
    }
    return 0;
}