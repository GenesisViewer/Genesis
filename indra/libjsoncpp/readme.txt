I decided to integrate jsoncpp to the viewer sources, because conflicts and
consequently crash bugs arise (such as a crash in std::deque::clear() !) when
trying to link a jsoncpp library that was built with gcc (seen with gcc v4.8.5
which was the Cool VL Viewer build system compiler) and a viewer binary built
with clang (seen with at least clang v8.0.1 and v9.0.0) on a system with a
different libstdc++ (seen with a newer one, associated with gcc v9.2.0).

By building jsoncpp together with the viewer binary, these bugs vanish...

Beside, the official jsoncpp library causes issues because of a badly named
header (config.h is standard system headers under Linux and such a name should
therefore never be used by library headers !). I changed the name and edited
the jsoncpp sources accordingly to cope.
I also disabled locale support (not needed), and the deprecation warnings (we
are still using the old API for the reader and writer, which is easier and
faster to use).
Oh, there was also an issue with snprintf, which is *NOT* in std namespace for
many C++11/14 compilers...

Henri Beauchamp.

Note: for the jsoncpp license, please see the doc/licenses-linux.txt file.
Original sources: https://github.com/open-source-parsers/jsoncpp