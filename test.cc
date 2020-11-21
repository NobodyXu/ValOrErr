#include "ValOrErr.hpp"
#include <cassert>

template <class Integer>
struct Wrapper {
    Integer integer{0};
};

using utility::ValOrErr;

int main(int argc, char* argv[])
{
    // Test ctor inplace exception + catching exception
    try {
        ValOrErr<char, int, long, void*> r{std::in_place_type<int>, -1};

        assert(r.has_exception_type<int>());

        bool is_visited = false;
        r.Catch([](void *p) {
            assert(false);
        }).Catch([&](int i) {
            assert(!is_visited);
            assert(i == -1);
            is_visited = true;
        }).Catch([](auto &e) {
            assert(false);
        });
        assert(is_visited);
    } catch (...) {
        assert(false);
    }

    // Test ctor inplace exception + catch-all
    try {
        ValOrErr<char, int, long, void*> r{std::in_place_type<int>, -1};

        assert(r.has_exception_type<int>());

        bool is_visited = false;
        r.Catch([](void *p) {
            assert(false);
        }).Catch([&](auto i) {
            assert(!is_visited);

            // Ensure the function actually get called at runtime takes int
            assert((std::is_same_v<decltype(i), int>));
            if constexpr(std::is_same_v<decltype(i), int>)
                assert(i == -1);

            is_visited = true;
        });
        assert(is_visited);
    } catch (...) {
        assert(false);
    }

    // Test specifing void as return type + ctor inplace exception + catch
    try {
        ValOrErr<void, int, long, void*> r{std::in_place_type<int>, -1};

        assert(r.has_exception_type<int>());

        bool is_visited = false;
        r.Catch([](void *p) {
            assert(false);
        }).Catch([&](int i) {
            assert(!is_visited);
            assert(i == -1);
            is_visited = true;
        }).Catch([](auto &e) {
            assert(false);
        });
        assert(is_visited);
    } catch (...) {
        assert(false);
    }

    // Test set_exception + dtor
    try {
        ValOrErr<char, int, long, void*> r{};

        r.set_exception<long>(-1L);
    } catch (long l) {
        assert(l == -1);
    }

    // Test ctor exception + dtor
    try {
        ValOrErr<char, int, long, void*> r{-1};

        assert(r.has_exception_type<int>());
    } catch (int i) {
        assert(i == -1);
    }

    // Test set_return_value + get_return_value
    try {
        ValOrErr<char, int> r;

        r.set_return_value('c');
        assert(r.get_return_value() == 'c');
    } catch (...) {
        assert(false);
    }

    // Test ctor return value + get_return_value
    try {
        ValOrErr<char, int> r{'c'};
        assert(r.get_return_value() == 'c');
    } catch (...) {
        assert(false);
    }

    // Test ctor inplace return value in_place + get_return_value
    try {
        ValOrErr<char, int> r{std::in_place_type<char>, 'c'};
        assert(r.get_return_value() == 'c');
    } catch (...) {
        assert(false);
    }

    // Test ctor exception + get_return_value
    try {
        ValOrErr<char, int> r{-1};
        assert(r.has_exception_type<int>());
        assert(r.get_return_value() == 'c');
    } catch (int i) {
        assert(i == -1);
    } catch (...) {
        assert(false);
    }

    // Test default ctor + get_return_value
    try {
        ValOrErr<Wrapper<char>, int> r{};
        assert(r.get_return_value().integer == '\0');
    } catch (...) {
        assert(false);
    }

    // Test ctor return value + implict convertion to Ret
    try {
        ValOrErr<int, char> r{-1};
        assert(int{r} == -1);
    } catch (...) {
        assert(false);
    }

    return 0;
}
