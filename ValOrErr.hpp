#ifndef  __ValOrErr_HPP__
# define __ValOrErr_HPP__

# include <functional>
# include <utility>
# include <type_traits>

# if (__cplusplus >= 201703L)
#  include <variant>
# endif

# if !defined(__EXCEPTIONS) || !defined(__cpp_exceptions)
#  include <err.h>
#  include <cstdio>
# endif

template <template <typename...> class variant_t, template <class> class in_place_type_t, 
          class Ret, class ...Ts>
class ValOrErr_t;

namespace ValOrErrImpl {
struct monostate {};

template <class T>
constexpr auto type_name() -> const char*
{
    return __PRETTY_FUNCTION__ + 49;
}

template <template <typename...> class variant>
struct variant_non_member_functions_t {};

# if (__cplusplus >= 201703L)
template <>
struct variant_non_member_functions_t<std::variant> {
    template <class T, class ...Types>
    static constexpr bool holds_alternative(const std::variant<Types...> &v) noexcept
    {
        return std::holds_alternative<T>(v);
    }
    template <class T, class Variant>
    static constexpr auto&& get(Variant &&v)
    {
        return std::get<T>(std::forward<Variant>(v));
    }
};
# endif

template <class decay_T, class T>
static constexpr bool is_constructible() noexcept
{
    if constexpr(std::is_pointer<decay_T>::value)
        return true;
    else
        return std::is_constructible<decay_T, T>::value;
}

template <class decay_T, class T>
static constexpr bool is_nothrow_constructible() noexcept
{
    if constexpr(std::is_pointer<decay_T>::value)
        return true;
    else
        return std::is_nothrow_constructible<decay_T, T>::value;
}

template <class ValOrErr_t1, class ValOrErr_t2>
class glue_ValOrErr;

template <template <typename...> class variant1, template <class> class in_place_type_t1, 
          class Ret1, class ...Ts,
          template <typename...> class variant2, template <class> class in_place_type_t2, 
          class Ret2, class ...Tp>
class glue_ValOrErr<ValOrErr_t<variant1, in_place_type_t1, Ret1, Ts...>, 
                    ValOrErr_t<variant2, in_place_type_t2, Ret2, Tp...>> {
public:
    using type = ValOrErr_t<variant1, in_place_type_t1, Ret1, Ts..., Tp...>;
};

template <class ...>
using void_t = void;

// primary template handles types that have no nested ::ValOrErr_t member:
template <class T, class ValOrErr_t, class = void_t<>>
struct glue_ValOrErr_from {
    using type = ValOrErr_t;
};
 
// specialization recognizes types that do have a nested ::ValOrErr_t member:
template <class T, class ValOrErr_t>
struct glue_ValOrErr_from<T, ValOrErr_t, void_t<typename T::ValOrErr_t>>:
    public glue_ValOrErr<ValOrErr_t, typename T::ValOrErr_t>
{};
} /* namespace ValOrErrImpl */

/**
 * If ValOrErr_t1 and ValOrErr_t2 uses different variant impl, the result type
 * will use the variant impl of ValOrErr_t1.
 */
template <class ValOrErr_t1, class ValOrErr_t2>
using glue_ValOrErr_t = typename ValOrErrImpl::glue_ValOrErr<ValOrErr_t1, ValOrErr_t2>::type;

/**
 * If ValOrErr_t and T::ValOrErr_t uses different variant impl, the result type
 * will use the variant impl of ValOrErr_t.
 */
template <class T, class ValOrErr_t>
using glue_ValOrErr_from_t = typename ValOrErrImpl::glue_ValOrErr_from<T, ValOrErr_t>::type;

/**
 * ValOrErr forces the exception returned to be handled, otherwise it would be
 * thrown in destructor.
 * 
 * @tparam variant must has 
 *  - API identical to std::variant, including ADL-lookupable visit,
 *  - override ret_exception::impl::holds_alternative_t for variant and
 *    provides variant_non_member_t<variant>::holds_alternative, which should
 *    has the same API as std::holds_alternative.
 *  - override std::get for you type
 * @tparam Ts... must not be void or the same type as Ret or has duplicated types.
 */
template <template <typename...> class variant, template <class> class in_place_type_t, 
          class Ret, class ...Ts>
class ValOrErr_t {
    using variant_nonmem_f_t = ValOrErrImpl::variant_non_member_functions_t<variant>;

    bool is_exception_handled = 0;
    bool has_exception = 0;

    using monostate = ValOrErrImpl::monostate;

    using variant_t = typename std::conditional<std::is_void<Ret>::value, 
                                                variant<monostate, Ts...>,
                                                variant<Ret, monostate, Ts...>>::type;
    variant_t v;

    template <class T>
    static constexpr bool holds_exp() noexcept
    {
        return (std::is_same<T, Ts>::value || ...);
    }

    template <class T>
    static constexpr bool holds_type() noexcept
    {
        return std::is_same<T, Ret>::value || holds_exp<T>();
    }

    struct Matcher {
        ValOrErr_t &r;
    
        template <class T, class decay_T = typename std::decay<T>::type,
                  class = typename std::enable_if<holds_exp<decay_T>() && 
                                                  ValOrErrImpl::is_constructible<decay_T, T>()>::type>
        void operator () (T &&obj)
            noexcept(ValOrErrImpl::is_nothrow_constructible<decay_T, T>())
        {
            r.set_exception<decay_T>(std::forward<T>(obj));
        }
    };

    void throw_if_hold_exp()
    {
        if (has_exception && !is_exception_handled && !v.valueless_by_exception())
            visit([this](auto &&e) {
                using Exception_t = typename std::decay<decltype(e)>::type;
                if constexpr(!std::is_same<Exception_t, Ret>::value && 
                             !std::is_same<Exception_t, monostate>::value) {
                    is_exception_handled = 1;

# if defined(__EXCEPTIONS) || defined(__cpp_exceptions)
                    throw std::move(e);
# else
                    std::fprintf(stderr, "[Exception%s ", ret_exception::impl::type_name<Exception_t>());

                    if constexpr(std::is_base_of<std::exception, Exception_t>::value)
                        errx(1, "%s", e.what());
                    else if constexpr(std::is_pointer<Exception_t>::value)
                        errx(1, "%p", e);
                    else if constexpr(std::is_integral<Exception_t>::value) {
                        if constexpr(std::is_unsigned<Exception_t>::value)
                            errx(1, "%llu", static_cast<unsigned long long>(e));
                        else
                            errx(1, "%lld", static_cast<long long>(e));
                    } else
                        errx(1, "");
# endif
                }
            }, v);
    }

    template <template <typename...> class variant2, template <class> class in_place_type_t2,
              class Ret_t2, class ...Tps>
    void from_other(ValOrErr_t<variant2, in_place_type_t2, Ret_t2, Tps...> &r)
    {
        if (r.has_exception_set())
            r.Catch(Matcher{*this});
        else {
            if constexpr(!std::is_void<Ret_t2>::value && !std::is_void<Ret>::value && 
                         std::is_constructible<Ret, Ret_t2>::value)
                set_return_value(r.get_return_value());
        }
    }

    template <template <typename...> class variant2, template <class> class in_place_type_t2,
              class Ret_t2, class ...Tps>
    void from_other(ValOrErr_t<variant2, in_place_type_t2, Ret_t2, Tps...> &&r)
    {
        if (r.has_exception_set())
            std::move(r).Catch(Matcher{*this});
        else {
            if constexpr(!std::is_void<Ret_t2>::value && !std::is_void<Ret>::value && 
                         std::is_constructible<Ret, Ret_t2>::value)
                set_return_value(std::move(r).get_return_value());
        }
    }

public:
    /**
     * If Ret != void, default initializae Ret;
     * Else, construct empty ValOrErr that contain no exception.
     */
    ValOrErr_t() = default;

    /**
     * @tparam T must be in Ts...
     * Copy construct or move construct type T.
     */
    template <class T, class decay_T = typename std::decay<T>::type,
              class = typename std::enable_if<holds_type<decay_T>() && 
                                              ValOrErrImpl::is_constructible<decay_T, T>()>::type>
    ValOrErr_t(T &&obj)
        noexcept(ValOrErrImpl::is_nothrow_constructible<decay_T, T>()):
            has_exception{!std::is_same<decay_T, Ret>::value},
            v{in_place_type_t<decay_T>{}, std::forward<T>(obj)}
    {}

    /**
     * @tparam T must be in Ts...
     * In place construct type T
     */
    template <class T, class ...Args, 
              class = typename std::enable_if<holds_type<T>() && 
                                              std::is_constructible<T, Args...>::value>::type>
    ValOrErr_t(in_place_type_t<T> type, Args &&...args)
        noexcept(std::is_nothrow_constructible<T, Args...>::value):
            has_exception{!std::is_same<T, Ret>::value}, 
            v{type, std::forward<Args>(args)...}
    {}

    /**
     * This ctor would only cp/mv the exceptions of type Type held by r only if
     * Type is also in Ts...
     *
     * Return value is a bit special here: If return value from r is convertible
     * to Ret, then it will be copied if r contains return value.
     */
    template <template <typename...> class variant2, template <class> class in_place_type_t2,
              class Ret_t2, class ...Tps>
    ValOrErr_t(ValOrErr_t<variant2, in_place_type_t2, Ret_t2, Tps...> &r):
            v{in_place_type_t<monostate>{}}
    {
        from_other(r);
    }

    /**
     * This ctor would only cp/mv the exceptions of type Type held by r only if
     * Type is also in Ts...
     *
     * Return value is a bit special here: If return value from r is convertible
     * to Ret, then it will be moved if r contains return value.
     */
    template <template <typename...> class variant2, template <class> class in_place_type_t2,
              class Ret_t2, class ...Tps,
              class = typename std::enable_if<!std::is_same<
                                          ValOrErr_t, 
                                          ValOrErr_t<variant2, in_place_type_t2, Ret_t2, Tps...>
                                      >::value>::type
             >
    ValOrErr_t(ValOrErr_t<variant2, in_place_type_t2, Ret_t2, Tps...> &&r):
            v{in_place_type_t<monostate>{}}
    {
        from_other(std::move(r));
    }

    ValOrErr_t(const ValOrErr_t&) = delete;

    /**
     * move constructor is required as NRVO isn't guaranteed to happen.
     */
    ValOrErr_t(ValOrErr_t &&other) 
        noexcept(std::is_nothrow_move_constructible<variant_t>::value):
            is_exception_handled{other.is_exception_handled},
            has_exception{other.has_exception},
            v{std::move(other.v)}
    {
        other.has_exception = 0;
        other.v.template emplace<monostate>();
    }

    /**
     * Would only cp/mv the exceptions of type Type held by r only if
     * Type is also in Ts...
     *
     * Return value is a bit special here: If return value from r is convertible
     * to Ret, then it will be copied if r contains return value.
     */
    template <template <typename...> class variant2, template <class> class in_place_type_t2,
              class Ret_t2, class ...Tps>
    ValOrErr_t& operator = (ValOrErr_t<variant2, in_place_type_t2, Ret_t2, Tps...> &r)
    {
        from_other(std::move(r));
        return *this;
    }
    /**
     * Would only cp/mv the exceptions of type Type held by r only if
     * Type is also in Ts...
     *
     * Return value is a bit special here: If return value from r is convertible
     * to Ret, then it will be moved if r contains return value.
     */
    template <template <typename...> class variant2, template <class> class in_place_type_t2,
              class Ret_t2, class ...Tps>
    ValOrErr_t& operator = (ValOrErr_t<variant2, in_place_type_t2, Ret_t2, Tps...> &&r)
    {
        from_other(std::move(r));
        return *this;
    }

    /**
     * Replace the previous value with exception.
     *
     * If an exception is contained in this object and it is not handled when this function
     * is called, this would cause the program to terminate.
     */
    template <class T, class ...Args, 
              class = typename std::enable_if<holds_exp<T>() && std::is_constructible<T, Args...>::value>::type>
    void set_exception(Args &&...args)
    {
        throw_if_hold_exp();

        is_exception_handled = 0;
        has_exception = 1;
        v.template emplace<T>(std::forward<Args>(args)...);
    }

    /**
     * Replace the previous value with return value.
     *
     * If an exception is contained in this object and it is not handled when this function
     * is called, this would cause the program to terminate.
     */
    template <class ...Args, 
              class = typename std::enable_if<std::is_constructible<Ret, Args...>::value>::type>
    void set_return_value(Args &&...args)
    {
        throw_if_hold_exp();

        has_exception = 0;
        v.template emplace<Ret>(std::forward<Args>(args)...);
    }

    bool has_exception_set() const noexcept
    {
        return has_exception;
    }
    bool has_exception_handled() const noexcept
    {
        return is_exception_handled;
    }

    /**
     * Test whether this object contain exception of type T.
     */
    template <class T>
    bool has_exception_type() const noexcept
    {
        return variant_nonmem_f_t::template holds_alternative<T>(v);
    }

    /**
     * Catch and handle the exception.
     *
     * @post has_exception_set() == true, has_exception_handled() == true
     *
     * Example:
     *     auto g() -> ValOrErr<void, PageNotFound, std::runtime_error, std::invalid_argument>;
     *     void f()
     *     {
     *         g().Catch([](const std::runtime_error &e) {
     *             return;
     *         }).Catch([](const auto &e) {
     *             throw e;
     *         });
     *     }
     */
    template <class F>
    auto Catch(F &&f) -> ValOrErr_t&
    {
       if (has_exception && !is_exception_handled && !v.valueless_by_exception())
            visit([&, this](auto &&e) {
                using Exception_t = typename std::decay<decltype(e)>::type;

                if constexpr(!std::is_same<Exception_t, monostate>::value && 
                             !std::is_same<Exception_t, Ret>::value)
                    if constexpr(std::is_invocable<typename std::decay<F>::type, Exception_t>::value) {
                        is_exception_handled = 1;
                        std::invoke(std::forward<F>(f), std::forward<decltype(e)>(e));
                    }
            }, v);

        return *this;
    }

    /**
     * Get the return value.
     *
     * If an exception is contained in this object and it is not handled when this function
     * is called, this would cause the program to terminate.
     *
     * Example:
     *     auto g() -> ValOrErr<std::string, PageNotFound, std::runtime_error, std::invalid_argument>;
     *     void f()
     *     {
     *         auto ret = g();
     *         ret.Catch([](const std::runtime_error &e) {
     *             return;
     *         }).Catch([](const auto &e) {
     *             throw e;
     *         });
     *
     *         auto &s = ret.get_return_value();
     *         std::cout << s << std::endl;
     *     }
     */
    template <class T = Ret, class = typename std::enable_if<!std::is_void<T>::value>::type>
    auto& get_return_value() &
    {
        throw_if_hold_exp();
        return variant_nonmem_f_t::template get<Ret>(v);
    }

    /**
     * Get the return value.
     *
     * If an exception is contained in this object and it is not handled when this function
     * is called, this would cause the program to terminate.
     */
    template <class T = Ret, class = typename std::enable_if<!std::is_void<T>::value>::type>
    auto& get_return_value() const &
    {
        throw_if_hold_exp();
        return variant_nonmem_f_t::template get<Ret>(v);
    }

    template <class T = Ret, class = typename std::enable_if<!std::is_void<T>::value>::type>
    auto&& get_return_value() &&
    {
        throw_if_hold_exp();
        return variant_nonmem_f_t::template get<Ret>(std::move(v));
    }

    template <class T = Ret, class = typename std::enable_if<!std::is_void<T>::value>::type>
    auto&& get_return_value() const &&
    {
        throw_if_hold_exp();
        return variant_nonmem_f_t::template get<Ret>(std::move(v));
    }

    /**
     * Get the return value.
     *
     * If an exception is contained in this object and it is not handled when this function
     * is called, this would cause the program to terminate.
     */
    template <class T = Ret, class = typename std::enable_if<!std::is_void<T>::value>::type>
    operator T&() &
    {
        throw_if_hold_exp();
        return variant_nonmem_f_t::template get<Ret>(v);
    }

    template <class T = Ret, class = typename std::enable_if<!std::is_void<T>::value>::type>
    operator const T&() const &
    {
        throw_if_hold_exp();
        return variant_nonmem_f_t::template get<Ret>(v);
    }

    template <class T = Ret, class = typename std::enable_if<!std::is_void<T>::value>::type>
    operator T&&() &&
    {
        throw_if_hold_exp();
        return variant_nonmem_f_t::template get<Ret>(std::move(v));
    }

    template <class T = Ret, class = typename std::enable_if<!std::is_void<T>::value>::type>
    operator const T&&() const &&
    {
        throw_if_hold_exp();
        return variant_nonmem_f_t::template get<Ret>(std::move(v));
    }

    /**
     * If an exception is contained in this object and it is not handled when dtor
     * is called, this would cause the program to terminate.
     */
    ~ValOrErr_t() noexcept(false)
    {
        throw_if_hold_exp();
    }
};

# if (__cplusplus >= 201703L)
/**
 * ValOrErr is only present when -std=c++17.
 */
template <class Ret, class ...Ts>
using ValOrErr = ValOrErr_t<std::variant, std::in_place_type_t, Ret, Ts...>;
# endif

/**
 * Example usage:
 *
 * Given that:
 *
 *     class A {
 *     public:
 *         A(ValOrErr<void, std::exception> &e);
 *     };
 *     class B {
 *     public:
 *     };
 *
 * To tell the different between A (requires ValOrErr) and B,
 * use std::is_constructible_v<type, ValOrErr_detector_t>.
 */
struct ValOrErr_detector_t {
    template <template <typename...> class variant, template <class> class in_place_type_t, 
              class Ret, class ...Ts>
    operator ValOrErr_t<variant, in_place_type_t, Ret, Ts...>& () const noexcept;
};

#endif
