#include "ValOrErr.hpp"
#include <cassert>
#include <stdexcept>

#include <cstddef>
#include <type_traits>
#include <initializer_list>
#include <algorithm>

namespace test {
template <class T>
struct in_place_type_t {};

struct bad_variant_access: std::runtime_error {
    bad_variant_access():
        std::runtime_error{"bad variant access"}
    {}

    bad_variant_access(const bad_variant_access&) = default;
};

/**
 * Naive variant that only supports at most 4 different types
 *
 * It also improperly impl some member function to simplify the code,
 * as long as Ret_except_t<variant, in_place_type_t> works.
 */
template <class ...>
struct variant {};

template <class T1, class T2, class T3, class T4>
class variant<T1, T2, T3, T4> {
public:
    static inline constexpr char variant_npos = -1;

protected:
    char index_v;

    char storage[std::max(std::initializer_list<std::size_t>{sizeof(T1), sizeof(T2), sizeof(T3), sizeof(T4)})];

    template <class T, class ...Args>
    void construct(in_place_type_t<T>, Args &&...args)
    {
        static_assert(std::is_same_v<T, T1> || std::is_same_v<T, T2> || std::is_same_v<T, T3> || 
                      std::is_same_v<T, T4>);

        try {
            if constexpr(std::is_same_v<T, T1>) {
                index_v = 0;
                new (storage) T1(std::forward<Args>(args)...);
            } else if constexpr(std::is_same_v<T, T2>) {
                index_v = 1;
                new (storage) T2(std::forward<Args>(args)...);
            } else if constexpr(std::is_same_v<T, T3>) {
                index_v = 2;
                new (storage) T3(std::forward<Args>(args)...);
            } else if constexpr(std::is_same_v<T, T4>) {
                index_v = 3;
                new (storage) T4(std::forward<Args>(args)...);
            } else
                assert(false);
        } catch (...) {
            index_v = variant_npos;
            throw;
        }
    }

public:
    template <class F>
    friend constexpr void visit(F &&f, variant &v)
    {
        switch (v.index_v) {
            case 0:
                std::invoke(std::forward<F>(f), *reinterpret_cast<T1*>(v.storage));
                break;

            case 1:
                std::invoke(std::forward<F>(f), *reinterpret_cast<T2*>(v.storage));
                break;

            case 2:
                std::invoke(std::forward<F>(f), *reinterpret_cast<T3*>(v.storage));
                break;

            case 3:
                std::invoke(std::forward<F>(f), *reinterpret_cast<T4*>(v.storage));
        }
    }
    template <class F>
    friend constexpr void visit(F &&f, variant &&v)
    {
        switch (v.index_v) {
            case 0:
                std::invoke(std::forward<F>(f), std::move(*reinterpret_cast<T1*>(v.storage)));
                break;

            case 1:
                std::invoke(std::forward<F>(f), std::move(*reinterpret_cast<T2*>(v.storage)));
                break;

            case 2:
                std::invoke(std::forward<F>(f), std::move(*reinterpret_cast<T3*>(v.storage)));
                break;

            case 3:
                std::invoke(std::forward<F>(f), std::move(*reinterpret_cast<T4*>(v.storage)));
        }
    }

    template <class T>
    constexpr T& get_impl() noexcept
    {
        return *reinterpret_cast<T*>(storage);
    }

protected:
    constexpr void destroy() noexcept
    {
        visit([](auto &&val) noexcept
        {
            using type = std::decay_t<decltype(val)>;
            val.~type();
        }, *this);
    }

public:
    template <class T, class ...Args>
    constexpr explicit variant(in_place_type_t<T> type, Args &&...args)
    {
        construct(type, std::forward<Args>(args)...);
    }

    constexpr variant():
        variant(in_place_type_t<T1>{})
    {}

    constexpr variant(variant &&other):
        index_v{other.index_v}
    {
        visit([this](auto &&val)
        {
            new (storage) T1(std::move(val));
        }, std::move(other));
    }

    template <class T, class ...Args>
    void emplace(Args &&...args)
    {
        destroy();
        construct(in_place_type_t<T>{}, std::forward<Args>(args)...);
    }

    constexpr bool valueless_by_exception() const noexcept
    {
        return index_v == -1;
    }

    constexpr std::size_t index() const noexcept
    {
        return index_v;
    }

    ~variant()
    {
        destroy();
    }
};
} /* namespace test */

namespace utility::ValOrErrImpl {
template <>
struct variant_non_member_functions_t<test::variant> {
    template <std::size_t i>
    using size_constant = std::integral_constant<std::size_t, i>;

    template <std::size_t i, class T, class ...Ts>
    struct index_of_T {};

    template <std::size_t i, class T, class T1, class ...Ts>
    struct index_of_T<i, T, T1, Ts...>:
        std::conditional_t<std::is_same_v<T, T1>, size_constant<i>, index_of_T<i + 1, T, Ts...>>
    {};

    template <class T, class ...Ts>
    static constexpr bool holds_alternative(const test::variant<Ts...> &v) noexcept
    {
        return v.index() == index_of_T<0, T, Ts...>::value;
    }

    template <class T, class Variant>
    static constexpr auto&& get(Variant &&v)
    {
        if (!holds_alternative<T>(v))
            throw test::bad_variant_access{};
        
        T &val = v.template get_impl<T>();
        if constexpr (std::is_rvalue_reference_v<Variant&&>)
            return std::move(val);
        else
            return val;
    }
};
} /* namespace utility::ValOrErrImpl */

template <class Ret, class ...Ts>
using ValOrErr_test = utility::ValOrErr_t<test::variant, test::in_place_type_t, Ret, Ts...>;

template <class T>
static constexpr const auto in_place_type = test::in_place_type_t<T>{};

template <class Integer>
struct Wrapper {
    Integer integer{0};
};

int main(int argc, char* argv[])
{
    // Test ctor inplace exception + catching exception
    try {
        ValOrErr_test<char, int, long> r{in_place_type<int>, -1};

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
        ValOrErr_test<char, int, long> r{in_place_type<int>, -1};

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
        ValOrErr_test<void, int, long, void*> r{in_place_type<int>, -1};

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
        ValOrErr_test<char, int, long> r{};

        r.set_exception<long>(-1L);
    } catch (long l) {
        assert(l == -1);
    }

    // Test ctor exception + dtor
    try {
        ValOrErr_test<char, int, long> r{-1};

        assert(r.has_exception_type<int>());
    } catch (int i) {
        assert(i == -1);
    }

    // Test set_return_value + get_return_value
    try {
        ValOrErr_test<char, int, void*> r;

        r.set_return_value('c');
        assert(r.get_return_value() == 'c');
    } catch (...) {
        assert(false);
    }

    // Test ctor return value + get_return_value
    try {
        ValOrErr_test<char, int, void*> r{'c'};
        assert(r.get_return_value() == 'c');
    } catch (...) {
        assert(false);
    }

    // Test ctor inplace return value in_place + get_return_value
    try {
        ValOrErr_test<char, int, void*> r{in_place_type<char>, 'c'};
        assert(r.get_return_value() == 'c');
    } catch (...) {
        assert(false);
    }

    // Test ctor exception + get_return_value
    try {
        ValOrErr_test<char, int, void*> r{-1};
        assert(r.has_exception_type<int>());
        assert(r.get_return_value() == 'c');
    } catch (int i) {
        assert(i == -1);
    } catch (...) {
        assert(false);
    }

    // Test default ctor + get_return_value
    try {
        ValOrErr_test<Wrapper<char>, int, void*> r{};
        assert(r.get_return_value().integer == '\0');
    } catch (...) {
        assert(false);
    }

    // Test ctor return value + implict convertion to Ret
    try {
        ValOrErr_test<int, char, void*> r{-1};
        assert(int{r} == -1);
    } catch (...) {
        assert(false);
    }

    return 0;
}
