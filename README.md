# ValOrErr

![testing CI](https://github.com/NobodyXu/ValOrErr/workflows/testing%20CI/badge.svg)

Return errors in an unignorable way and have compile-time only Catch, which produces no bloat code with `CXXFLAGS=-O2 -Wl,--strip-all`.

[Document]

## Advantage compared to C++ exception

 - Pay for what you use -- only when you use a specific feature will that code be generated.
 - No rtti is required -- `variant` is used for minimal runtime check per-`ValOrErr`, only generate
 when required and will not add a lot of global information.
 - You can use any `variant` implementation in `ValOrErr`, giving you further control of the 
 generated code.
 - All generated code will undergo optimizer, which can heavily optimize to remove any bloat code with 
 `CXXFLAGS=-O2 -Wl,--strip-all`.
 - More explicit syntax: `ValOrErr` requires all possible exception type to be provided at compile-time.
 - If you don't handle the exception stored in `ValOrErr`, then it will terminate your program by
   - Calling `errx` and print `e.what()` (if `e.what()` is valid) if exception is disabled.
   - Otherwise, throw exception again.

## Downsides compared to C++ exceptions

 - Hard or impossible to use in constructor.
 - Since all possible exception type is explicit, it poses a big problem in template code:
 <br>You would have to use detect return type of function and use `glue_ret_except_from_t` to add more
 exceptions.

For more information, check [Document].

## Usage

```c++
#include "/path/to/ValOrErr.hpp"

#include <limits>
#include <stdexcept>

#include <cstdio>
#include <cstdint>
#include <cinttypes>

#include <err.h>

template <class T>
auto ftoi(long double f) noexcept -> utility::ValOrErr<T, std::invalid_argument, std::out_of_range>
{
    if (f == std::numeric_limits<float>::infinity())
        return {std::invalid_argument{"f should not be INF"}};
    else if (f > std::numeric_limits<T>::max())
        return {std::out_of_range("f is too large")};
    else if (f < std::numeric_limits<T>::min())
        return {std::out_of_range("f is too small")};
    return static_cast<T>(f);
}

int main(int argc, char* argv[])
{
    long double f;
    std::scanf("%Lf", &f);

    auto integer = ftoi<std::uint64_t>(f)
        .Catch([](const std::out_of_range &e) noexcept {
            // Catch out_of_range only
            errx(1, "std::out_of_range: %s", e.what());
        }).Catch([](const auto &e) noexcept {
            // Catch any exception
            errx(1, "In function %s: Catched exception %s", 
                    __PRETTY_FUNCTION__, e.what());
        }).get_return_value(); /* Get return value here when all exceptions are handled */

    std::printf("%" PRIu64 "\n", integer);
    return 0;
}
```

Now, if you compile the code with `clang++ -std=c++17 -O2 -Wl,--strip-all -fno-exceptions -fno-rtti example.cc`,
you will get an executable that is fairly small (on my x86-64 intel machine, 12K only).

Let's do some test on the code:

```
$ ./a.out
1.2333
1
$ ./a.out
123412434324823878712471230712847312844
a.out: std::out_of_range: f is too large
$ ./a.out
INF
a.out: In function auto main(int, char **)::(anonymous class)::operator()(const auto &) const [e:auto = std::invali
d_argument]: Catched exception f should not be INF
```

It's doing exactly what we want it to do.

What is more amazing is that how small the executable can be.
If you run the compiler again with 
`clang++ -std=c++17 -O3 -flto -Wl,--strip-all -fno-exceptions -fno-rtti example.cc`, you would probably find
the executable to be even smaller.

On my machine, I found it to be merely 8.9K, which definitely is not possible with c++ exceptions.

For more information, check [Document].

[Document]: https://nobodyxu.github.io/ValOrErr/
