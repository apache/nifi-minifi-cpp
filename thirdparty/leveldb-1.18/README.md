LevelDB qdb branch
==================

Current version: 1.18

quasardb [LevelDB](http://code.google.com/p/leveldb/) branch with full Windows support. This is not an official LevelDB branch, but the branch we use in our product, [quasardb](https://www.quasardb.net/).

* Full Windows support: everything builds, all tests pass;
* [CMake](http://www.cmake.org/) based build
* Explicit (thread unsafe) de-allocation routines for "clean exits". Helps a lot when running your application into a leak detector;
* The Windows build requires [Boost](http://www.boost.org/); 
* Our code is C++11ish and may require a recent compiler;
* Lots of warnings fixed;
* Is not 100% compliant with Google coding style.

Tested on [FreeBSD](http://www.freebsd.org/), Linux and Windows (32-bit & 64-bit).

Might contains trace of nuts.

Comments? Questions? Suggestions? Pull!
