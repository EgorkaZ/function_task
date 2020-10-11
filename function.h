#pragma once

#include "function_traits.h"

#include <exception>
#include <memory>

struct bad_function_call : public std::exception
{
    const char * what() const noexcept override
    {
        return "bad function call";
    }
};

template <typename F>
struct function;

template <typename R, typename... Args>
struct function<R(Args...)>
{
    function() = default;

    function(function const & other)
        : m_caller(other ? other.m_caller->make_copy() : nullptr)
    {
    }

    function(function && other) noexcept
        : m_caller(std::move(other.m_caller))
    {
    }

    template <typename T>
    function(T val)
        : m_caller(std::make_unique<detail::function_caller<T, R(Args...)>>(std::move(val)))
    {
    }

    function & operator=(function const & rhs)
    {
        if (this != &rhs) {
            if (rhs) {
                m_caller.reset(rhs.m_caller->make_copy());
            } else {
                m_caller.reset();
            }
        }
        return *this;
    }
    function & operator=(function && rhs) noexcept
    {
        m_caller.swap(rhs.m_caller);
        return *this;
    }

    explicit operator bool() const noexcept
    {
        return m_caller.get();
    }

    R operator()(Args... args) const
    {
        if (m_caller.get()) {
            return m_caller->invoke(std::forward<Args>(args)...);
        } else {
            throw bad_function_call();
        }
    }

    template <typename T>
    T * target() noexcept
    {
        return (*this) ? m_caller->template get_func_ptr<T>() : nullptr;
    }

    template <typename T>
    T const * target() const noexcept
    {
        return (*this) ? m_caller->template get_func_ptr<T>() : nullptr;
    }

private:
    std::unique_ptr<detail::function_caller_base<R(Args...)>> m_caller;
};
