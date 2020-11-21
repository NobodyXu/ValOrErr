#include "ValOrErr.hpp"

#include <type_traits>
#include <stdexcept>
#include <cassert>

using utility::ValOrErr;
using utility::ValOrErr_detector_t;
using utility::glue_ValOrErr_t;
using utility::glue_ValOrErr_from_t;

class A {
public:
    A(ValOrErr<void, std::exception> &e);
};

using ValOrErr_t1 = ValOrErr<void, int, void*>;
using ValOrErr_t2 = ValOrErr<void, void*>;
using ValOrErr_t3 = ValOrErr<void, int>;

class B {
public:
    using ValOrErr_t = ValOrErr_t2;
    B(ValOrErr_t &e);
};

int main(int argc, char* argv[])
{
    static_assert(std::is_constructible_v<A, ValOrErr_detector_t>);

    static_assert(std::is_same_v<glue_ValOrErr_t<ValOrErr_t3, ValOrErr_t2>, ValOrErr_t1>);
    static_assert(std::is_same_v<glue_ValOrErr_t<ValOrErr<int, int>, ValOrErr_t2>, 
                                 ValOrErr<int, int, void*>>);

    static_assert(std::is_same_v<glue_ValOrErr_from_t<A, ValOrErr_t2>, ValOrErr_t2>);
    static_assert(std::is_same_v<glue_ValOrErr_from_t<B, ValOrErr_t3>, ValOrErr_t1>);

    // cp ctor from other ValOrErr
    try {
        ValOrErr_t2 r2{static_cast<void*>(nullptr)};
        ValOrErr_t1 r1{r2};

        bool is_visited = false;
        r1.Catch([&](void *p) noexcept {
            assert(is_visited == false);
            assert(p == nullptr);
            is_visited = true;
        });
        assert(is_visited);
    } catch (...) {
        assert(false);
    }

    // cp ctor of ret from other ValOrErr
    try {
        ValOrErr<int, void*> r2{-1};
        ValOrErr<int, char> r1{r2};

        assert(r2.get_return_value() == -1);
        assert(r1.get_return_value() == -1);
    } catch (...) {
        assert(false);
    }

    // mv ctor from other ValOrErr
    try {
        ValOrErr_t1 r1{ValOrErr_t2{static_cast<void*>(nullptr)}};

        bool is_visited = false;
        r1.Catch([&](void *p) noexcept {
            assert(is_visited == false);
            assert(p == nullptr);
            is_visited = true;
        });
        assert(is_visited);
    } catch (...) {
        assert(false);
    }

    // mv ctor ret from other ValOrErr
    try {
        ValOrErr<std::vector<int>, void*> r2{std::vector{-1}};
        ValOrErr<std::vector<int>, char> r1{std::move(r2)};

        assert(r2.get_return_value().size() == 0);
        assert(r1.get_return_value().size() == 1);
        assert(r1.get_return_value()[0] == -1);
    } catch (...) {
        assert(false);
    }

    return 0;
}
