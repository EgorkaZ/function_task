#include <cstddef>
#include <type_traits>
#include <utility>

namespace detail {

constexpr std::size_t SizeOfStorage = sizeof(void *);
constexpr std::size_t AlignOfStorage = alignof(void *);

template <class T>
constexpr bool fits_storage = sizeof(T) <= SizeOfStorage &&
        alignof(T) % AlignOfStorage && std::is_nothrow_move_constructible<T>::value;

using storage_t = std::aligned_storage<SizeOfStorage, AlignOfStorage>::type;

template <class Functor, bool fits_small = fits_storage<Functor>>
struct func_storage
{

    func_storage(const Functor & func) noexcept(fits_small && std::is_nothrow_copy_constructible_v<Functor>)
    {
        forward_assign(func);
    }

    func_storage(Functor && func) noexcept(fits_small && std::is_nothrow_move_constructible_v<Functor>)
    {
        forward_assign(std::move(func));
    }

    Functor * get_func_ptr() noexcept
    {
        return functor_from_storage<false>(m_storage);
    }

    const Functor & get_func() const noexcept
    {
        return *(functor_from_storage<true>(m_storage));
    }

    void destruct() noexcept(std::is_nothrow_destructible_v<Functor>)
    {
        if constexpr (fits_small) {
            get_func().~Functor();
        } else {
            delete get_func_ptr();
        }
    }

    ~func_storage() noexcept(std::is_nothrow_destructible_v<Functor>)
    {
        destruct();
    }

private:
    template <bool is_const, class C>
    using cond_const_ptr = std::conditional_t<is_const, const C *, C *>;

    template <bool is_const, class C>
    using cond_const_ref = std::conditional_t<is_const, const C &, C &>;

    template <bool is_const>
    static cond_const_ptr<is_const, Functor> functor_from_storage(cond_const_ref<is_const, storage_t> storage) noexcept
    {
        using ReturnedFunctor = cond_const_ptr<is_const, Functor>;

        if constexpr (fits_small) {
            return reinterpret_cast<cond_const_ptr<is_const, Functor>>(&storage);
        } else {
            return reinterpret_cast<Functor * const &>(storage);
        }
    }

    template <class F>
    void forward_assign(F && f) noexcept(fits_small && std::is_nothrow_constructible_v<Functor, F &&>)
    {
        if constexpr (fits_small) {
            new (&m_storage) Functor(std::forward<F>(f));
        } else {
            reinterpret_cast<Functor *&>(m_storage) = new Functor(std::forward<F>(f));
        }
    }

    storage_t m_storage;
};

template <class Signature>
struct function_caller_base;

template <class Functor, class Signature>
struct function_caller;

template <class Ret, class... Args>
struct function_caller_base<Ret(Args...)>
{
    template <class Functor>
    using Derived = function_caller<Functor, Ret(Args...)>;

    virtual Ret invoke(Args &&...) = 0;

    virtual function_caller_base * make_copy() const = 0;

    template <class Functor>
    Functor * get_func_ptr()
    {
        auto as_derived = dynamic_cast<Derived<Functor> *>(this);
        return as_derived ? as_derived->get_func_ptr() : nullptr;
    }

    template <class Functor>
    const Functor & get_func() const
    {
        return reinterpret_cast<const Derived<Functor> *>(this)->get_func();
    }

    virtual ~function_caller_base() = default;
};

template <class Functor, class Ret, class... Args>
struct function_caller<Functor, Ret(Args...)> final : function_caller_base<Ret(Args...)>
{
    using Base = function_caller_base<Ret(Args...)>;

    function_caller() = default;

    function_caller(const Functor & func)
        : m_storage(func)
    {
    }

    function_caller(Functor && func)
        : m_storage(std::move(func))
    {
    }

    Ret invoke(Args &&... args) final
    {
        return m_storage.get_func()(std::forward<Args>(args)...);
    }

    Base * make_copy() const final
    {
        return new function_caller(get_func());
    }

    Functor * get_func_ptr() noexcept
    {
        return m_storage.get_func_ptr();
    }

    const Functor & get_func() const noexcept
    {
        return m_storage.get_func();
    }

private:
    func_storage<Functor> m_storage;
};

}; // namespace detail
