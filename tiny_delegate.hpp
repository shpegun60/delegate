// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#pragma once

// English comments.
//
// tiny_delegate.hpp
// - C++17/C++20 compatible
// - No exceptions required; stored callables must be nothrow-movable and
//   nothrow-destructible, so every move/reset/destroy path is noexcept
// - Optional global default bytes/alignment and heap fallback toggle
//
// Main types:
//   tiny::delegate_ref<Sig>                 // non-owning
//   tiny::delegate_sbo<Sig, InlineBytes, InlineAlign>
//                                          // owning, SBO (+ optional heap fallback), move-only
//   tiny::delegate<Sig, InlineBytes, InlineAlign>
//                                          // auto (own by default; borrow/bind => ref mode), move-only
//
// Helpers:
//   tiny::borrow(x)                         // force ref binding for lvalues
//   tiny::bind<&T::method>(obj)             // bind method with signature deduction
//
// Compile-time paranoia:
//   fits_inline<T>(), required_inline_bytes<T>(), static_assert_fits_inline<T>()
//   ABI sanity checks in tiny::ct
//
// Runtime paranoia policy (assert()-style, keyed on NDEBUG):
//   TINY_DELEGATE_ASSERT(expr, msg) guards every unsafe entry point (calling
//   an empty delegate, assigning a null function pointer). Without NDEBUG
//   the default traps deterministically (__builtin_trap / std::abort)
//   instead of jumping through a null pointer. With NDEBUG (release) the
//   checks compile out entirely and every delegate call site is two
//   instructions on Cortex-M — calling an empty delegate is then UB, same
//   as calling a method through a null interface pointer. Define
//   TINY_DELEGATE_ASSERT yourself to force either behavior in any build,
//   or to log/count instead. Like NDEBUG itself, the definition must be
//   identical across every TU that includes this header (ODR).

#include <cstddef>
#include <cstdlib>    // std::abort (non-GNU trap fallback)
#include <optional>   // call_if() result for non-void signatures
#include <type_traits>
#include <utility>
#include <new>
#include <functional> // std::invoke
#include <memory>     // std::addressof

// Default inline capacity scales with the platform word instead of a fixed
// host-sized 64 bytes: four pointers' worth, but never below the layout
// minimum of 16. On a 32-bit MCU that is 16 bytes; on a 64-bit host, 32.
// Use delegate64/delegate<Sig, N> when a bigger buffer is intended.
#ifndef TINY_DELEGATE_DEFAULT_BYTES
#define TINY_DELEGATE_DEFAULT_BYTES \
    ((4u * sizeof(void*)) < 16u ? 16u : (4u * sizeof(void*)))
#endif
#ifndef TINY_DELEGATE_DEFAULT_ALIGN
#define TINY_DELEGATE_DEFAULT_ALIGN alignof(std::max_align_t)
#endif
#ifndef TINY_DELEGATE_ENABLE_HEAP_FALLBACK
#define TINY_DELEGATE_ENABLE_HEAP_FALLBACK 0
#endif

// Branch-layout hint so the fail-closed guard stays off the hot path.
#ifndef TINY_DELEGATE_UNLIKELY
#if defined(__GNUC__) || defined(__clang__)
#define TINY_DELEGATE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define TINY_DELEGATE_UNLIKELY(x) (x)
#endif
#endif

// assert()-style default. Debug builds (no NDEBUG) fail closed: a violated
// guard traps deterministically instead of continuing into undefined
// behavior (e.g. an indirect call through null). Release builds (NDEBUG)
// compile the checks out, exactly like <cassert>, which makes the call
// site two instructions on Cortex-M. Override TINY_DELEGATE_ASSERT to pin
// either behavior regardless of NDEBUG, or to log/count instead; keep the
// definition identical across all TUs.
#ifndef TINY_DELEGATE_ASSERT
#ifdef NDEBUG
#define TINY_DELEGATE_ASSERT(expr, msg) ((void)0)
#else
#define TINY_DELEGATE_ASSERT(expr, msg) \
    do { if (TINY_DELEGATE_UNLIKELY(!(expr))) { ::tiny::detail::trap(); } } while (false)
#endif
#endif

namespace tiny {

template <class T>
struct sig_of;

template <class R, class... Args>
struct sig_of<R(*)(Args...)> { using type = R(Args...); };
template <class R, class... Args>
struct sig_of<R(*)(Args...) noexcept> { using type = R(Args...); };

template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...)> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) volatile> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const volatile> { using type = R(Args...); };

template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) &> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const &> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) volatile &> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const volatile &> { using type = R(Args...); };

template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) &&> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const &&> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) volatile &&> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const volatile &&> { using type = R(Args...); };

template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) noexcept> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const noexcept> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) volatile noexcept> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const volatile noexcept> { using type = R(Args...); };

template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) & noexcept> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const & noexcept> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) volatile & noexcept> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const volatile & noexcept> { using type = R(Args...); };

template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) && noexcept> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const && noexcept> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) volatile && noexcept> { using type = R(Args...); };
template <class C, class R, class... Args>
struct sig_of<R(C::*)(Args...) const volatile && noexcept> { using type = R(Args...); };

template <class T>
using sig_of_t = typename sig_of<T>::type;

namespace detail {

// Deterministic stop for a violated runtime guard. On GCC/Clang this is a
// single trapping instruction (udf on ARM) and needs no library support;
// elsewhere it degrades to std::abort. Never returns.
[[noreturn]] inline void trap() noexcept {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_trap();
#else
    std::abort();
#endif
}

template <class Fn, std::size_t NeedSize, std::size_t HaveSize, std::size_t NeedAlign, std::size_t HaveAlign>
struct diag_delegate_does_not_fit;

template <class...>
struct dependent_false : std::false_type {};
template <class... Ts>
inline constexpr bool dependent_false_v = dependent_false<Ts...>::value;

template <class Fn, std::size_t NeedSize, std::size_t HaveSize, std::size_t NeedAlign, std::size_t HaveAlign>
constexpr void fail_delegate_does_not_fit() {
    static_assert(dependent_false_v<diag_delegate_does_not_fit<Fn, NeedSize, HaveSize, NeedAlign, HaveAlign>>,
                  "tiny::delegate family: callable does not fit inline storage. Increase InlineBytes/InlineAlign or enable heap fallback.");
}
constexpr bool is_pow2(std::size_t x) noexcept { return x && ((x & (x - 1u)) == 0u); }

constexpr std::size_t round_up_to(std::size_t value, std::size_t align) noexcept {
    return ((value + align - 1u) / align) * align;
}

constexpr std::size_t max_size(std::size_t a, std::size_t b) noexcept {
    return (a < b) ? b : a;
}

constexpr std::size_t max_size(std::size_t a, std::size_t b, std::size_t c) noexcept {
    return max_size(max_size(a, b), c);
}

constexpr std::size_t max_size(std::size_t a, std::size_t b, std::size_t c, std::size_t d) noexcept {
    return max_size(max_size(a, b), max_size(c, d));
}

template <class T>
constexpr void* erase_ptr(T* p) noexcept {
    return const_cast<void*>(static_cast<const volatile void*>(p));
}
} // namespace detail

template <class F>
struct borrow_t { F* p; };

template <class F>
constexpr auto borrow(F&& f) noexcept -> borrow_t<std::remove_reference_t<F>> {
    static_assert(std::is_lvalue_reference_v<F&&>,
                  "tiny::borrow: cannot borrow a temporary. Pass an lvalue with a lifetime that outlives the delegate.");
    return borrow_t<std::remove_reference_t<F>>{std::addressof(f)};
}

template <class Sig>
class delegate_ref;

template <class R, class... Args>
class delegate_ref<R(Args...)> {
public:
    using signature = R(Args...);
    using fnptr_t   = R (*)(Args...);
    // The invoker receives the payload pointer directly (single indirection,
    // like the owning delegates), not a reference back to the delegate.
    using invoke_t  = R (*)(void*, Args...);

    constexpr delegate_ref() noexcept = default;
    constexpr delegate_ref(std::nullptr_t) noexcept {}
    delegate_ref& operator=(std::nullptr_t) noexcept { reset(); return *this; }

    delegate_ref(fnptr_t fp) { *this = fp; }
    delegate_ref& operator=(fnptr_t fp) {
        // Storing a function pointer in void* is conditionally-supported but
        // universal on every mainstream ABI; the size check keeps it honest.
        static_assert(sizeof(fnptr_t) == sizeof(void*),
                      "tiny::delegate_ref: function pointer must fit in void*");
        TINY_DELEGATE_ASSERT(fp, "tiny::delegate_ref: null function pointer");
        if (!fp) {
            reset();
            return *this;
        }
        payload_ = reinterpret_cast<void*>(fp);
        invoke_ = &invoke_fnptr_;
        return *this;
    }

    template <class F>
    delegate_ref(borrow_t<F> br) { *this = br; }

    template <class F>
    delegate_ref& operator=(borrow_t<F> br) {
        static_assert(std::is_invocable_r_v<R, F&, Args...>, "tiny::delegate_ref::borrow: signature mismatch.");
        TINY_DELEGATE_ASSERT(br.p, "tiny::delegate_ref::borrow: null pointer");
        payload_ = detail::erase_ptr(br.p);
        invoke_ = &invoke_functor_ref_<F>;
        return *this;
    }

    template <auto Method, class T>
    static constexpr delegate_ref bind(T& obj) noexcept {
        static_assert(std::is_member_function_pointer_v<decltype(Method)>,
                      "tiny::delegate_ref::bind: Method must be a member function pointer.");
        static_assert(std::is_invocable_r_v<R, decltype(Method), T&, Args...>,
                      "tiny::delegate_ref::bind: signature mismatch (args/return).");

        delegate_ref d;
        d.payload_ = detail::erase_ptr(std::addressof(obj));
        d.invoke_ = &invoke_method_ref_<Method, T>;
        return d;
    }

    template <auto Method, class T>
    static constexpr delegate_ref bind(const T& obj) noexcept {
        static_assert(std::is_member_function_pointer_v<decltype(Method)>,
                      "tiny::delegate_ref::bind: Method must be a member function pointer.");
        static_assert(std::is_invocable_r_v<R, decltype(Method), const T&, Args...>,
                      "tiny::delegate_ref::bind: signature mismatch (const args/return).");

        delegate_ref d;
        d.payload_ = detail::erase_ptr(std::addressof(obj));
        d.invoke_ = &invoke_method_ref_const_<Method, T>;
        return d;
    }

    template <auto Method, class T, std::enable_if_t<!std::is_lvalue_reference_v<T&&>, int> = 0>
    static constexpr delegate_ref bind(T&&) = delete;

    /**
     * Compile-time free-function binding: the function is baked into the
     * generated invoker, so the delegate is fully constexpr-constructible
     * (ROM tables) and its payload stays unused.
     */
    template <auto Function,
              std::enable_if_t<std::is_pointer_v<decltype(Function)>
                               && std::is_function_v<std::remove_pointer_t<decltype(Function)>>,
                               int> = 0>
    static constexpr delegate_ref bind() noexcept {
        static_assert(Function != nullptr,
                      "tiny::delegate_ref::bind: function cannot be null.");
        static_assert(std::is_invocable_r_v<R, decltype(Function), Args...>,
                      "tiny::delegate_ref::bind: signature mismatch (args/return).");
        delegate_ref d;
        d.invoke_ = &invoke_function_static_<Function>;
        return d;
    }

    /**
     * Compile-time instance binding: both the method and the object (which
     * must have static storage and linkage) are template arguments; nothing
     * is loaded from the payload at call time.
     */
    template <auto Method, auto& Instance>
    static constexpr delegate_ref bind() noexcept {
        static_assert(std::is_member_function_pointer_v<decltype(Method)>,
                      "tiny::delegate_ref::bind: Method must be a member function pointer.");
        static_assert(std::is_invocable_r_v<R, decltype(Method),
                                            decltype(Instance), Args...>,
                      "tiny::delegate_ref::bind: signature mismatch (args/return).");
        delegate_ref d;
        d.invoke_ = &invoke_method_static_<Method, Instance>;
        return d;
    }

    constexpr void reset() noexcept {
        payload_ = nullptr;
        invoke_ = nullptr;
    }

    constexpr explicit operator bool() const noexcept { return invoke_ != nullptr; }

    R operator()(Args... args) const {
        TINY_DELEGATE_ASSERT(invoke_, "tiny::delegate_ref: call on empty");
        return invoke_(payload_, std::forward<Args>(args)...);
    }

    // Safe-call: empty is a legal state here, not a violation. Returns bool
    // (called or not) for void signatures, std::optional<R> otherwise.
    auto call_if(Args... args) const {
        static_assert(!std::is_reference_v<R>,
                      "tiny::delegate_ref::call_if: std::optional cannot hold a "
                      "reference result; use operator() or call_or instead.");
        if constexpr (std::is_void_v<R>) {
            if (!invoke_) return false;
            invoke_(payload_, std::forward<Args>(args)...);
            return true;
        } else {
            if (!invoke_) return std::optional<R>{};
            return std::optional<R>(invoke_(payload_, std::forward<Args>(args)...));
        }
    }

    // Safe-call with a fallback: invokes the delegate when engaged,
    // otherwise the alternative callable, with the same arguments.
    template <typename Alternative>
    R call_or(Alternative&& alternative, Args... args) const {
        static_assert(std::is_invocable_r_v<R, Alternative, Args...>,
                      "tiny::delegate_ref::call_or: alternative signature mismatch.");
        if constexpr (std::is_pointer_v<std::decay_t<Alternative>>
                      || std::is_member_pointer_v<std::decay_t<Alternative>>) {
            TINY_DELEGATE_ASSERT(alternative != nullptr,
                                 "tiny::delegate_ref::call_or: null fallback");
        }
        if (invoke_) {
            return invoke_(payload_, std::forward<Args>(args)...);
        }
        return std::invoke(std::forward<Alternative>(alternative),
                           std::forward<Args>(args)...);
    }

private:
    void*     payload_ = nullptr;
    invoke_t  invoke_  = nullptr;

    static R invoke_fnptr_(void* p, Args... a) {
        auto fp = reinterpret_cast<fnptr_t>(p);
        if constexpr (std::is_void_v<R>) { fp(std::forward<Args>(a)...); return; }
        else { return fp(std::forward<Args>(a)...); }
    }

    template <class F>
    static R invoke_functor_ref_(void* p, Args... a) {
        F& fn = *static_cast<F*>(p);
        if constexpr (std::is_void_v<R>) { std::invoke(fn, std::forward<Args>(a)...); return; }
        else { return std::invoke(fn, std::forward<Args>(a)...); }
    }

    template <auto Method, class T>
    static R invoke_method_ref_(void* p, Args... a) {
        T& o = *static_cast<T*>(p);
        if constexpr (std::is_void_v<R>) { std::invoke(Method, o, std::forward<Args>(a)...); return; }
        else { return std::invoke(Method, o, std::forward<Args>(a)...); }
    }

    template <auto Method, class T>
    static R invoke_method_ref_const_(void* p, Args... a) {
        const T& o = *static_cast<const T*>(p);
        if constexpr (std::is_void_v<R>) { std::invoke(Method, o, std::forward<Args>(a)...); return; }
        else { return std::invoke(Method, o, std::forward<Args>(a)...); }
    }

    template <auto Function>
    static R invoke_function_static_(void*, Args... a) {
        if constexpr (std::is_void_v<R>) { Function(std::forward<Args>(a)...); return; }
        else { return Function(std::forward<Args>(a)...); }
    }

    template <auto Method, auto& Instance>
    static R invoke_method_static_(void*, Args... a) {
        if constexpr (std::is_void_v<R>) { std::invoke(Method, Instance, std::forward<Args>(a)...); return; }
        else { return std::invoke(Method, Instance, std::forward<Args>(a)...); }
    }
};

template <class Sig, std::size_t InlineBytes = TINY_DELEGATE_DEFAULT_BYTES,
          std::size_t InlineAlign = TINY_DELEGATE_DEFAULT_ALIGN>
class delegate_sbo;

template <class R, class... Args, std::size_t InlineBytes, std::size_t InlineAlign>
class delegate_sbo<R(Args...), InlineBytes, InlineAlign> {
public:
    using signature = R(Args...);
    using fnptr_t   = R (*)(Args...);
    using invoke_t  = R (*)(void*, Args...);

    template <class T>
    static constexpr std::size_t required_inline_bytes() noexcept { return sizeof(std::decay_t<T>); }

    template <class T>
    static constexpr std::size_t required_inline_align() noexcept { return alignof(std::decay_t<T>); }

    template <class T>
    static constexpr bool fits_inline() noexcept {
        using DT = std::decay_t<T>;
        return (sizeof(DT) <= InlineBytes) && (alignof(DT) <= InlineAlign);
    }

    template <class T>
    static constexpr void static_assert_fits_inline() {
        using DT = std::decay_t<T>;
        if constexpr (!fits_inline<DT>()) {
            detail::fail_delegate_does_not_fit<DT, sizeof(DT), InlineBytes, alignof(DT), InlineAlign>();
        }
    }

    static_assert(InlineBytes >= 16, "tiny::delegate_sbo: InlineBytes too small.");
    static_assert(detail::is_pow2(InlineAlign), "tiny::delegate_sbo: InlineAlign must be power-of-two.");
    static_assert(InlineBytes >= sizeof(fnptr_t), "tiny::delegate_sbo: InlineBytes must fit function pointer.");
    static_assert(InlineAlign >= alignof(fnptr_t), "tiny::delegate_sbo: InlineAlign must fit function pointer alignment.");

    constexpr delegate_sbo() noexcept = default;
    constexpr delegate_sbo(std::nullptr_t) noexcept {}

    delegate_sbo(const delegate_sbo&) = delete;
    delegate_sbo& operator=(const delegate_sbo&) = delete;

    // Stored callables are statically required to be nothrow-movable and
    // nothrow-destructible, so transfer and teardown are unconditionally
    // noexcept: a type-erased move can never fail halfway.
    delegate_sbo(delegate_sbo&& other) noexcept { move_from_(other); }
    delegate_sbo& operator=(delegate_sbo&& other) noexcept {
        if (this != &other) { reset(); move_from_(other); }
        return *this;
    }

    ~delegate_sbo() { reset(); }

    constexpr void reset() noexcept {
        if (mgr_) mgr_->destroy(*this);
        clear_();
    }

    constexpr explicit operator bool() const noexcept { return invoke_ != nullptr; }

    constexpr bool uses_heap() const noexcept { return mgr_ ? mgr_->uses_heap(*this) : false; }
    constexpr bool uses_inline() const noexcept { return (invoke_ != nullptr) && !uses_heap(); }

    static constexpr std::size_t inline_capacity_bytes() noexcept { return InlineBytes; }
    static constexpr std::size_t inline_capacity_align() noexcept { return InlineAlign; }

    R operator()(Args... args) const {
        TINY_DELEGATE_ASSERT(invoke_, "tiny::delegate_sbo: call on empty");
        return invoke_(ctx_, std::forward<Args>(args)...);
    }

    // Safe-call: empty is a legal state here, not a violation. Returns bool
    // (called or not) for void signatures, std::optional<R> otherwise.
    auto call_if(Args... args) const {
        static_assert(!std::is_reference_v<R>,
                      "tiny::delegate_sbo::call_if: std::optional cannot hold a "
                      "reference result; use operator() or call_or instead.");
        if constexpr (std::is_void_v<R>) {
            if (!invoke_) return false;
            invoke_(ctx_, std::forward<Args>(args)...);
            return true;
        } else {
            if (!invoke_) return std::optional<R>{};
            return std::optional<R>(invoke_(ctx_, std::forward<Args>(args)...));
        }
    }

    // Safe-call with a fallback: invokes the delegate when engaged,
    // otherwise the alternative callable, with the same arguments.
    template <typename Alternative>
    R call_or(Alternative&& alternative, Args... args) const {
        static_assert(std::is_invocable_r_v<R, Alternative, Args...>,
                      "tiny::delegate_sbo::call_or: alternative signature mismatch.");
        if constexpr (std::is_pointer_v<std::decay_t<Alternative>>
                      || std::is_member_pointer_v<std::decay_t<Alternative>>) {
            TINY_DELEGATE_ASSERT(alternative != nullptr,
                                 "tiny::delegate_sbo::call_or: null fallback");
        }
        if (invoke_) {
            return invoke_(ctx_, std::forward<Args>(args)...);
        }
        return std::invoke(std::forward<Alternative>(alternative),
                           std::forward<Args>(args)...);
    }

    delegate_sbo& operator=(std::nullptr_t) noexcept { reset(); return *this; }

    delegate_sbo(fnptr_t fp) { *this = fp; }
    delegate_sbo& operator=(fnptr_t fp) {
        reset();
        TINY_DELEGATE_ASSERT(fp, "tiny::delegate_sbo: null function pointer");
        if (!fp) return *this;
        ::new (inline_ptr_()) fnptr_t(fp);
        ctx_ = inline_ptr_();
        invoke_ = &invoke_obj_<fnptr_t>;
        mgr_ = &mgr_inline_<fnptr_t>();
        return *this;
    }

    // Constrained so overload resolution (and std::is_constructible /
    // std::is_assignable) reject a non-matching callable instead of failing
    // inside the body.
    template <class F, class DF = std::decay_t<F>,
              std::enable_if_t<!std::is_same_v<DF, delegate_sbo>
                               && (std::is_invocable_r_v<R, DF&, Args...>
                                   || std::is_convertible_v<DF, fnptr_t>),
                               int> = 0>
    delegate_sbo(F&& f) { *this = std::forward<F>(f); }

    template <class F, class DF = std::decay_t<F>,
              std::enable_if_t<!std::is_same_v<DF, delegate_sbo>
                               && (std::is_invocable_r_v<R, DF&, Args...>
                                   || std::is_convertible_v<DF, fnptr_t>),
                               int> = 0>
    delegate_sbo& operator=(F&& f) {
        reset();
        assign_callable_(std::forward<F>(f));
        return *this;
    }

private:
    struct manager {
        void (*destroy)(delegate_sbo&) noexcept;
        void (*move)(delegate_sbo& src, delegate_sbo& dst) noexcept;
        bool (*uses_heap)(const delegate_sbo&) noexcept;
    };

    alignas(InlineAlign) mutable std::byte storage_[InlineBytes]{};
    void*    ctx_    = nullptr;
    invoke_t invoke_ = nullptr;
    const manager* mgr_ = nullptr;

    constexpr void* inline_ptr_() const noexcept {
        return static_cast<void*>(const_cast<std::byte*>(storage_));
    }

    constexpr void clear_() noexcept {
        ctx_ = nullptr;
        invoke_ = nullptr;
        mgr_ = nullptr;
    }

    void move_from_(delegate_sbo& other) noexcept {
        if (other.mgr_) {
            other.mgr_->move(other, *this);
            other.clear_();
        } else {
            ctx_ = other.ctx_;
            invoke_ = other.invoke_;
            mgr_ = other.mgr_;
            other.clear_();
        }
    }

    template <class Obj>
    static R invoke_obj_(void* c, Args... a) {
        Obj& obj = *std::launder(reinterpret_cast<Obj*>(c));
        if constexpr (std::is_void_v<R>) { std::invoke(obj, std::forward<Args>(a)...); return; }
        else { return std::invoke(obj, std::forward<Args>(a)...); }
    }

    template <class Obj>
    static const manager& mgr_inline_() noexcept {
        static const manager m{
            +[](delegate_sbo& self) noexcept {
                Obj& o = *std::launder(reinterpret_cast<Obj*>(self.inline_ptr_()));
                o.~Obj();
            },
            +[](delegate_sbo& src, delegate_sbo& dst) noexcept {
                Obj& s = *std::launder(reinterpret_cast<Obj*>(src.inline_ptr_()));
                ::new (dst.inline_ptr_()) Obj(std::move(s));
                s.~Obj();
                dst.ctx_ = dst.inline_ptr_();
                dst.invoke_ = &invoke_obj_<Obj>;
                dst.mgr_ = &mgr_inline_<Obj>();
                src.clear_();
            },
            +[](const delegate_sbo&) noexcept { return false; }
        };
        return m;
    }

    template <class Obj>
    static const manager& mgr_heap_() noexcept {
        static const manager m{
            +[](delegate_sbo& self) noexcept { delete static_cast<Obj*>(self.ctx_); },
            +[](delegate_sbo& src, delegate_sbo& dst) noexcept {
                dst.ctx_ = src.ctx_;
                dst.invoke_ = &invoke_obj_<Obj>;
                dst.mgr_ = &mgr_heap_<Obj>();
                src.clear_();
            },
            +[](const delegate_sbo&) noexcept { return true; }
        };
        return m;
    }

    template <class F>
    void assign_callable_(F&& f) {
        using DF = std::decay_t<F>;
        if constexpr (std::is_convertible_v<DF, fnptr_t>) {
            fnptr_t fp = static_cast<fnptr_t>(std::forward<F>(f));
            *this = fp;
            return;
        }

        static_assert(std::is_invocable_r_v<R, DF&, Args...>, "tiny::delegate_sbo: signature mismatch.");
        static_assert(std::is_nothrow_destructible_v<DF>,
                      "tiny::delegate_sbo: stored callable must be nothrow-destructible "
                      "(reset()/~delegate_sbo() are noexcept).");

        constexpr std::size_t need_size  = sizeof(DF);
        constexpr std::size_t need_align = alignof(DF);
        // Inline placement needs BOTH the fit and a nothrow move (delegate
        // moves relocate the payload). Heap placement needs neither: its
        // move is a pointer handoff. So a callable that fits but has a
        // potentially-throwing move goes to the heap when the fallback is
        // enabled, and is rejected with a precise message otherwise.
        constexpr bool fits_storage =
            need_size <= InlineBytes && need_align <= InlineAlign;
        constexpr bool inline_ok =
            fits_storage && std::is_nothrow_move_constructible_v<DF>;

        if constexpr (inline_ok) {
            ::new (inline_ptr_()) DF(std::forward<F>(f));
            ctx_ = inline_ptr_();
            invoke_ = &invoke_obj_<DF>;
            mgr_ = &mgr_inline_<DF>();
        } else {
#if TINY_DELEGATE_ENABLE_HEAP_FALLBACK
            DF* p = new DF(std::forward<F>(f));
            ctx_ = static_cast<void*>(p);
            invoke_ = &invoke_obj_<DF>;
            mgr_ = &mgr_heap_<DF>();
#else
            if constexpr (!fits_storage) {
                detail::fail_delegate_does_not_fit<DF, need_size, InlineBytes, need_align, InlineAlign>();
            } else {
                static_assert(std::is_nothrow_move_constructible_v<DF>,
                              "tiny::delegate_sbo: an inline-stored callable must be "
                              "nothrow-move-constructible so delegate moves are noexcept. "
                              "Mark its move/copy constructor noexcept, or enable "
                              "TINY_DELEGATE_ENABLE_HEAP_FALLBACK to store it on the heap.");
            }
#endif
        }
    }
};

template <class Sig,
          std::size_t InlineBytes = TINY_DELEGATE_DEFAULT_BYTES,
          std::size_t InlineAlign = TINY_DELEGATE_DEFAULT_ALIGN>
class delegate;

template <class R, class... Args, std::size_t InlineBytes, std::size_t InlineAlign>
class delegate<R(Args...), InlineBytes, InlineAlign> {
public:
    using signature = R(Args...);
    using fnptr_t   = R (*)(Args...);
    using invoke_t  = R (*)(void*, Args...);

    template <class T>
    static constexpr std::size_t required_inline_bytes() noexcept { return sizeof(std::decay_t<T>); }

    template <class T>
    static constexpr std::size_t required_inline_align() noexcept { return alignof(std::decay_t<T>); }

    template <class T>
    static constexpr bool fits_inline() noexcept {
        using DT = std::decay_t<T>;
        return (sizeof(DT) <= InlineBytes) && (alignof(DT) <= InlineAlign);
    }

    template <class T>
    static constexpr void static_assert_fits_inline() {
        using DT = std::decay_t<T>;
        if constexpr (!fits_inline<DT>()) {
            detail::fail_delegate_does_not_fit<DT, sizeof(DT), InlineBytes, alignof(DT), InlineAlign>();
        }
    }

    static constexpr std::size_t inline_capacity_bytes() noexcept { return InlineBytes; }
    static constexpr std::size_t inline_capacity_align() noexcept { return InlineAlign; }

    static_assert(InlineBytes >= 16, "tiny::delegate: InlineBytes too small.");
    static_assert(detail::is_pow2(InlineAlign), "tiny::delegate: InlineAlign must be power-of-two.");
    static_assert(InlineBytes >= sizeof(fnptr_t), "tiny::delegate: InlineBytes must fit function pointer.");
    static_assert(InlineAlign >= alignof(fnptr_t), "tiny::delegate: InlineAlign must fit function pointer alignment.");

    constexpr delegate() noexcept = default;
    constexpr delegate(std::nullptr_t) noexcept {}

    delegate(const delegate&) = delete;
    delegate& operator=(const delegate&) = delete;

    // Stored callables are statically required to be nothrow-movable and
    // nothrow-destructible, so transfer and teardown are unconditionally
    // noexcept: a type-erased move can never fail halfway.
    delegate(delegate&& other) noexcept { move_from_(other); }
    delegate& operator=(delegate&& other) noexcept {
        if (this != &other) { reset(); move_from_(other); }
        return *this;
    }

    ~delegate() { reset(); }

    constexpr void reset() noexcept {
        if (mgr_) mgr_->destroy(*this);
        clear_();
    }

    constexpr explicit operator bool() const noexcept { return invoke_ != nullptr; }

    constexpr bool non_owning() const noexcept { return mgr_ == &mgr_ref_(); }
    constexpr bool owning() const noexcept { return (invoke_ != nullptr) && !non_owning(); }

    constexpr bool uses_heap() const noexcept { return mgr_ ? mgr_->uses_heap(*this) : false; }
    constexpr bool uses_inline() const noexcept { return owning() && !uses_heap(); }

    R operator()(Args... args) const {
        TINY_DELEGATE_ASSERT(invoke_, "tiny::delegate: call on empty");
        return invoke_(ctx_, std::forward<Args>(args)...);
    }

    // Safe-call: empty is a legal state here, not a violation. Returns bool
    // (called or not) for void signatures, std::optional<R> otherwise.
    auto call_if(Args... args) const {
        static_assert(!std::is_reference_v<R>,
                      "tiny::delegate::call_if: std::optional cannot hold a "
                      "reference result; use operator() or call_or instead.");
        if constexpr (std::is_void_v<R>) {
            if (!invoke_) return false;
            invoke_(ctx_, std::forward<Args>(args)...);
            return true;
        } else {
            if (!invoke_) return std::optional<R>{};
            return std::optional<R>(invoke_(ctx_, std::forward<Args>(args)...));
        }
    }

    // Safe-call with a fallback: invokes the delegate when engaged,
    // otherwise the alternative callable, with the same arguments.
    template <typename Alternative>
    R call_or(Alternative&& alternative, Args... args) const {
        static_assert(std::is_invocable_r_v<R, Alternative, Args...>,
                      "tiny::delegate::call_or: alternative signature mismatch.");
        if constexpr (std::is_pointer_v<std::decay_t<Alternative>>
                      || std::is_member_pointer_v<std::decay_t<Alternative>>) {
            TINY_DELEGATE_ASSERT(alternative != nullptr,
                                 "tiny::delegate::call_or: null fallback");
        }
        if (invoke_) {
            return invoke_(ctx_, std::forward<Args>(args)...);
        }
        return std::invoke(std::forward<Alternative>(alternative),
                           std::forward<Args>(args)...);
    }

    delegate& operator=(std::nullptr_t) noexcept { reset(); return *this; }

    delegate(fnptr_t fp) { *this = fp; }
    delegate& operator=(fnptr_t fp) {
        reset();
        TINY_DELEGATE_ASSERT(fp, "tiny::delegate: null function pointer");
        if (!fp) return *this;
        ::new (inline_ptr_()) fnptr_t(fp);
        ctx_ = inline_ptr_();
        invoke_ = &invoke_obj_<fnptr_t>;
        mgr_ = &mgr_inline_<fnptr_t>();
        return *this;
    }

    // Constrained so overload resolution (and std::is_constructible /
    // std::is_assignable) reject a non-matching callable instead of failing
    // inside the body.
    template <class F, class DF = std::decay_t<F>,
              std::enable_if_t<!std::is_same_v<DF, delegate>
                               && (std::is_invocable_r_v<R, DF&, Args...>
                                   || std::is_convertible_v<DF, fnptr_t>),
                               int> = 0>
    delegate(F&& f) { *this = std::forward<F>(f); }

    template <class F, class DF = std::decay_t<F>,
              std::enable_if_t<!std::is_same_v<DF, delegate>
                               && (std::is_invocable_r_v<R, DF&, Args...>
                                   || std::is_convertible_v<DF, fnptr_t>),
                               int> = 0>
    delegate& operator=(F&& f) {
        reset();
        assign_callable_(std::forward<F>(f));
        return *this;
    }

    template <class F>
    delegate(borrow_t<F> br) { *this = br; }

    template <class F>
    delegate& operator=(borrow_t<F> br) {
        reset();
        assign_borrow_(br);
        return *this;
    }

    template <auto Method, class T>
    static constexpr delegate bind(T& obj) noexcept {
        delegate d;
        d.template assign_method_<Method>(obj);
        return d;
    }

    template <auto Method, class T>
    static constexpr delegate bind(const T& obj) noexcept {
        delegate d;
        d.template assign_method_<Method>(obj);
        return d;
    }

    template <auto Method, class T, std::enable_if_t<!std::is_lvalue_reference_v<T&&>, int> = 0>
    static constexpr delegate bind(T&&) = delete;

    /** Compile-time free-function binding; the payload stays unused. */
    template <auto Function,
              std::enable_if_t<std::is_pointer_v<decltype(Function)>
                               && std::is_function_v<std::remove_pointer_t<decltype(Function)>>,
                               int> = 0>
    static delegate bind() noexcept {
        static_assert(Function != nullptr,
                      "tiny::delegate::bind: function cannot be null.");
        static_assert(std::is_invocable_r_v<R, decltype(Function), Args...>,
                      "tiny::delegate::bind: signature mismatch (args/return).");
        delegate d;
        d.invoke_ = &invoke_function_static_<Function>;
        d.mgr_ = &mgr_ref_();
        return d;
    }

    /** Compile-time instance binding (object with static storage/linkage). */
    template <auto Method, auto& Instance>
    static delegate bind() noexcept {
        static_assert(std::is_member_function_pointer_v<decltype(Method)>,
                      "tiny::delegate::bind: Method must be a member function pointer.");
        static_assert(std::is_invocable_r_v<R, decltype(Method),
                                            decltype(Instance), Args...>,
                      "tiny::delegate::bind: signature mismatch (args/return).");
        delegate d;
        d.invoke_ = &invoke_method_static_<Method, Instance>;
        d.mgr_ = &mgr_ref_();
        return d;
    }

private:
    struct manager {
        void (*destroy)(delegate&) noexcept;
        void (*move)(delegate& src, delegate& dst) noexcept;
        bool (*uses_heap)(const delegate&) noexcept;
    };

    alignas(InlineAlign) mutable std::byte storage_[InlineBytes]{};
    void*    ctx_    = nullptr;
    invoke_t invoke_ = nullptr;
    const manager* mgr_ = nullptr;

    constexpr void* inline_ptr_() const noexcept {
        return static_cast<void*>(const_cast<std::byte*>(storage_));
    }

    constexpr void clear_() noexcept {
        ctx_ = nullptr;
        invoke_ = nullptr;
        mgr_ = nullptr;
    }

    void move_from_(delegate& other) noexcept {
        if (other.mgr_) {
            other.mgr_->move(other, *this);
            other.clear_();
        } else {
            ctx_ = other.ctx_;
            invoke_ = other.invoke_;
            mgr_ = other.mgr_;
            other.clear_();
        }
    }

    template <class Obj>
    static R invoke_obj_(void* c, Args... a) {
        Obj& obj = *std::launder(reinterpret_cast<Obj*>(c));
        if constexpr (std::is_void_v<R>) { std::invoke(obj, std::forward<Args>(a)...); return; }
        else { return std::invoke(obj, std::forward<Args>(a)...); }
    }

    template <class F>
    static R invoke_functor_ref_(void* c, Args... a) {
        F& fn = *static_cast<F*>(c);
        if constexpr (std::is_void_v<R>) { std::invoke(fn, std::forward<Args>(a)...); return; }
        else { return std::invoke(fn, std::forward<Args>(a)...); }
    }

    template <auto Method, class T>
    static R invoke_method_ref_(void* c, Args... a) {
        T& self = *static_cast<T*>(c);
        if constexpr (std::is_void_v<R>) { std::invoke(Method, self, std::forward<Args>(a)...); return; }
        else { return std::invoke(Method, self, std::forward<Args>(a)...); }
    }

    template <auto Method, class T>
    static R invoke_method_ref_const_(void* c, Args... a) {
        const T& self = *static_cast<const T*>(c);
        if constexpr (std::is_void_v<R>) { std::invoke(Method, self, std::forward<Args>(a)...); return; }
        else { return std::invoke(Method, self, std::forward<Args>(a)...); }
    }

    template <auto Function>
    static R invoke_function_static_(void*, Args... a) {
        if constexpr (std::is_void_v<R>) { Function(std::forward<Args>(a)...); return; }
        else { return Function(std::forward<Args>(a)...); }
    }

    template <auto Method, auto& Instance>
    static R invoke_method_static_(void*, Args... a) {
        if constexpr (std::is_void_v<R>) { std::invoke(Method, Instance, std::forward<Args>(a)...); return; }
        else { return std::invoke(Method, Instance, std::forward<Args>(a)...); }
    }

    static const manager& mgr_ref_() noexcept {
        static const manager m{
            +[](delegate&) noexcept {},
            +[](delegate& src, delegate& dst) noexcept {
                dst.ctx_ = src.ctx_;
                dst.invoke_ = src.invoke_;
                dst.mgr_ = &mgr_ref_();
                src.clear_();
            },
            +[](const delegate&) noexcept { return false; }
        };
        return m;
    }

    template <class Obj>
    static const manager& mgr_inline_() noexcept {
        static const manager m{
            +[](delegate& self) noexcept {
                Obj& o = *std::launder(reinterpret_cast<Obj*>(self.inline_ptr_()));
                o.~Obj();
            },
            +[](delegate& src, delegate& dst) noexcept {
                Obj& s = *std::launder(reinterpret_cast<Obj*>(src.inline_ptr_()));
                ::new (dst.inline_ptr_()) Obj(std::move(s));
                s.~Obj();
                dst.ctx_ = dst.inline_ptr_();
                dst.invoke_ = &invoke_obj_<Obj>;
                dst.mgr_ = &mgr_inline_<Obj>();
                src.clear_();
            },
            +[](const delegate&) noexcept { return false; }
        };
        return m;
    }

    template <class Obj>
    static const manager& mgr_heap_() noexcept {
        static const manager m{
            +[](delegate& self) noexcept { delete static_cast<Obj*>(self.ctx_); },
            +[](delegate& src, delegate& dst) noexcept {
                dst.ctx_ = src.ctx_;
                dst.invoke_ = &invoke_obj_<Obj>;
                dst.mgr_ = &mgr_heap_<Obj>();
                src.clear_();
            },
            +[](const delegate&) noexcept { return true; }
        };
        return m;
    }

    template <class F>
    void assign_borrow_(borrow_t<F> br) {
        static_assert(std::is_invocable_r_v<R, F&, Args...>, "tiny::delegate::borrow: signature mismatch.");
        TINY_DELEGATE_ASSERT(br.p, "tiny::delegate::borrow: null pointer");
        ctx_ = detail::erase_ptr(br.p);
        invoke_ = &invoke_functor_ref_<F>;
        mgr_ = &mgr_ref_();
    }

    template <auto Method, class T>
    void assign_method_(T& obj) noexcept {
        static_assert(std::is_member_function_pointer_v<decltype(Method)>,
                      "tiny::delegate::bind: Method must be a member function pointer.");
        static_assert(std::is_invocable_r_v<R, decltype(Method), T&, Args...>,
                      "tiny::delegate::bind: signature mismatch (args/return).");

        ctx_ = detail::erase_ptr(std::addressof(obj));
        invoke_ = &invoke_method_ref_<Method, T>;
        mgr_ = &mgr_ref_();
    }

    template <auto Method, class T>
    void assign_method_(const T& obj) noexcept {
        static_assert(std::is_member_function_pointer_v<decltype(Method)>,
                      "tiny::delegate::bind: Method must be a member function pointer.");
        static_assert(std::is_invocable_r_v<R, decltype(Method), const T&, Args...>,
                      "tiny::delegate::bind: signature mismatch (const args/return).");

        ctx_ = detail::erase_ptr(std::addressof(obj));
        invoke_ = &invoke_method_ref_const_<Method, T>;
        mgr_ = &mgr_ref_();
    }

    template <class F>
    void assign_callable_(F&& f) {
        using DF = std::decay_t<F>;
        if constexpr (std::is_convertible_v<DF, fnptr_t>) {
            fnptr_t fp = static_cast<fnptr_t>(std::forward<F>(f));
            *this = fp;
            return;
        }

        static_assert(std::is_invocable_r_v<R, DF&, Args...>, "tiny::delegate: signature mismatch.");
        static_assert(std::is_nothrow_destructible_v<DF>,
                      "tiny::delegate: stored callable must be nothrow-destructible "
                      "(reset()/~delegate() are noexcept).");

        constexpr std::size_t need_size  = sizeof(DF);
        constexpr std::size_t need_align = alignof(DF);
        // Inline placement needs BOTH the fit and a nothrow move; heap
        // placement needs neither (its move is a pointer handoff). See the
        // identical logic in delegate_sbo::assign_callable_.
        constexpr bool fits_storage =
            need_size <= InlineBytes && need_align <= InlineAlign;
        constexpr bool inline_ok =
            fits_storage && std::is_nothrow_move_constructible_v<DF>;

        if constexpr (inline_ok) {
            ::new (inline_ptr_()) DF(std::forward<F>(f));
            ctx_ = inline_ptr_();
            invoke_ = &invoke_obj_<DF>;
            mgr_ = &mgr_inline_<DF>();
        } else {
#if TINY_DELEGATE_ENABLE_HEAP_FALLBACK
            DF* p = new DF(std::forward<F>(f));
            ctx_ = static_cast<void*>(p);
            invoke_ = &invoke_obj_<DF>;
            mgr_ = &mgr_heap_<DF>();
#else
            if constexpr (!fits_storage) {
                detail::fail_delegate_does_not_fit<DF, need_size, InlineBytes, need_align, InlineAlign>();
            } else {
                static_assert(std::is_nothrow_move_constructible_v<DF>,
                              "tiny::delegate: an inline-stored callable must be "
                              "nothrow-move-constructible so delegate moves are noexcept. "
                              "Mark its move/copy constructor noexcept, or enable "
                              "TINY_DELEGATE_ENABLE_HEAP_FALLBACK to store it on the heap.");
            }
#endif
        }
    }
};

template <auto Method, class T>
constexpr auto bind(T& obj) noexcept {
    using sig = sig_of_t<decltype(Method)>;
    return delegate<sig>::template bind<Method>(obj);
}

template <auto Method, class T>
constexpr auto bind(const T& obj) noexcept {
    using sig = sig_of_t<decltype(Method)>;
    return delegate<sig>::template bind<Method>(obj);
}

template <auto Method, class T, std::enable_if_t<!std::is_lvalue_reference_v<T&&>, int> = 0>
constexpr auto bind(T&&) = delete;

template <class Sig> using delegate64 = delegate<Sig, 64>;
template <class Sig> using delegate32 = delegate<Sig, 32>;

template <class Sig> using delegate_sbo64 = delegate_sbo<Sig, 64>;
template <class Sig> using delegate_sbo32 = delegate_sbo<Sig, 32>;

namespace ct {
template <class D>
constexpr std::size_t abi_budget_bytes() noexcept {
    using invoke_t = typename D::invoke_t;

    constexpr std::size_t ctx_align    = alignof(void*);
    constexpr std::size_t invoke_align = alignof(invoke_t);
    constexpr std::size_t mgr_align    = alignof(const void*);
    constexpr std::size_t struct_align =
        detail::max_size(D::inline_capacity_align(), ctx_align, invoke_align, mgr_align);

    std::size_t size = D::inline_capacity_bytes();
    size = detail::round_up_to(size, ctx_align) + sizeof(void*);
    size = detail::round_up_to(size, invoke_align) + sizeof(invoke_t);
    size = detail::round_up_to(size, mgr_align) + sizeof(void*);
    return detail::round_up_to(size, struct_align);
}

template <class D>
constexpr std::size_t ref_budget_bytes() noexcept {
    using fnptr_t  = typename D::fnptr_t;
    using invoke_t = typename D::invoke_t;

    constexpr std::size_t fn_align     = alignof(fnptr_t);
    constexpr std::size_t obj_align    = alignof(void*);
    constexpr std::size_t invoke_align = alignof(invoke_t);
    constexpr std::size_t struct_align = detail::max_size(fn_align, obj_align, invoke_align);

    std::size_t size = 0;
    size = detail::round_up_to(size, fn_align) + sizeof(fnptr_t);
    size = detail::round_up_to(size, obj_align) + sizeof(void*);
    size = detail::round_up_to(size, invoke_align) + sizeof(invoke_t);
    return detail::round_up_to(size, struct_align);
}

template <class Sig>
inline constexpr void delegate_sanity() {
    using D = tiny::delegate<Sig>;

    static_assert(std::is_move_constructible_v<D>, "delegate must be move-constructible.");
    static_assert(std::is_move_assignable_v<D>, "delegate must be move-assignable.");
    static_assert(std::is_nothrow_move_constructible_v<D>, "delegate moves must be noexcept.");
    static_assert(std::is_nothrow_move_assignable_v<D>, "delegate move-assign must be noexcept.");
    static_assert(std::is_nothrow_destructible_v<D>, "delegate destruction must be noexcept.");
    static_assert(!std::is_copy_constructible_v<D>, "delegate must be move-only.");
    static_assert(!std::is_copy_assignable_v<D>, "delegate must be move-only.");
    static_assert(!std::is_constructible_v<D, int>, "delegate must reject non-callables (SFINAE).");
    static_assert(!std::is_assignable_v<D&, double>, "delegate must reject non-callables (SFINAE).");

    static_assert(alignof(D) >= alignof(void*), "delegate alignment too small.");
    static_assert(sizeof(D) >= 3 * sizeof(void*), "delegate too small (layout bug?).");
    static_assert(sizeof(D) <= abi_budget_bytes<D>(), "delegate ABI budget exceeded (unexpected bloat).");
}

template <class Sig>
inline constexpr void delegate_sbo_sanity() {
    using D = tiny::delegate_sbo<Sig>;

    static_assert(std::is_move_constructible_v<D>, "delegate_sbo must be move-constructible.");
    static_assert(std::is_move_assignable_v<D>, "delegate_sbo must be move-assignable.");
    static_assert(std::is_nothrow_move_constructible_v<D>, "delegate_sbo moves must be noexcept.");
    static_assert(std::is_nothrow_move_assignable_v<D>, "delegate_sbo move-assign must be noexcept.");
    static_assert(std::is_nothrow_destructible_v<D>, "delegate_sbo destruction must be noexcept.");
    static_assert(!std::is_copy_constructible_v<D>, "delegate_sbo must be move-only.");
    static_assert(!std::is_copy_assignable_v<D>, "delegate_sbo must be move-only.");
    static_assert(!std::is_constructible_v<D, int>, "delegate_sbo must reject non-callables (SFINAE).");
    static_assert(!std::is_assignable_v<D&, double>, "delegate_sbo must reject non-callables (SFINAE).");

    static_assert(alignof(D) >= alignof(void*), "delegate_sbo alignment too small.");
    static_assert(sizeof(D) >= 3 * sizeof(void*), "delegate_sbo too small (layout bug?).");
    static_assert(sizeof(D) <= abi_budget_bytes<D>(), "delegate_sbo ABI budget exceeded (unexpected bloat).");
}

template <class Sig>
inline constexpr void delegate_ref_sanity() {
    using D = tiny::delegate_ref<Sig>;

    static_assert(std::is_copy_constructible_v<D>, "delegate_ref must be copy-constructible.");
    static_assert(std::is_copy_assignable_v<D>, "delegate_ref must be copy-assignable.");
    static_assert(std::is_move_constructible_v<D>, "delegate_ref must be move-constructible.");
    static_assert(std::is_move_assignable_v<D>, "delegate_ref must be move-assignable.");

    static_assert(alignof(D) >= alignof(void*), "delegate_ref alignment too small.");
    // Pinned exact layout: one payload pointer plus one invoker pointer.
    // A regression that grows delegate_ref past two words must fail here.
    static_assert(sizeof(D) == sizeof(void*) + sizeof(typename D::invoke_t),
                  "delegate_ref must stay exactly two pointers.");
    static_assert(sizeof(D) <= ref_budget_bytes<D>(), "delegate_ref ABI budget exceeded (unexpected bloat).");
}

inline constexpr int run_all = []{
    delegate_ref_sanity<void()>();
    delegate_ref_sanity<void(int)>();
    delegate_ref_sanity<int(int,int)>();
    delegate_sanity<void()>();
    delegate_sanity<void(int)>();
    delegate_sanity<int(int,int)>();
    delegate_sbo_sanity<void()>();
    delegate_sbo_sanity<void(int)>();
    delegate_sbo_sanity<int(int,int)>();
    return 0;
}();

} // namespace ct

} // namespace tiny
