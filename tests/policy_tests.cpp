#include "tiny_delegate.hpp"
#include "check.hpp"

#if !defined(NDEBUG) && (defined(__unix__) || defined(__APPLE__))
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

template <class F>
static void mustTrap(F action) {
    const auto child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        const rlimit no_core{0, 0};
        (void)setrlimit(RLIMIT_CORE, &no_core);
        action();
        _exit(0);
    }
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFSIGNALED(status));
    CHECK(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}
#endif

int main() {
    int evaluated = 0;
    TINY_DELEGATE_ASSERT(++evaluated == 1, "test policy");
#ifdef NDEBUG
    CHECK(evaluated == 0);
#else
    CHECK(evaluated == 1);
#endif

#if !defined(NDEBUG) && (defined(__unix__) || defined(__APPLE__))
    mustTrap([] { tiny::delegate_ref<void()> d; d(); });
    mustTrap([] { tiny::delegate<void()> d; d(); });
    mustTrap([] { tiny::delegate_sbo<void()> d; d(); });
    mustTrap([] { tiny::delegate_ref<void()> d{static_cast<void (*)()>(nullptr)}; });
    mustTrap([] { tiny::delegate<void()> d{static_cast<int (*)()>(nullptr)}; });
    mustTrap([] { tiny::delegate_sbo<void()> d{static_cast<int (*)()>(nullptr)}; });
    mustTrap([] {
        int (*p)() = nullptr;
        tiny::delegate_ref<int()> d{tiny::borrow(p)};
        (void)d.call_if();
    });
    mustTrap([] { tiny::delegate<void()> d; d.call_or(static_cast<int (*)()>(nullptr)); });
    mustTrap([] {
        struct Object { int method() { return 1; } };
        Object* pointer = nullptr;
        auto d = tiny::delegate_ref<int()>::bind<&Object::method>(pointer);
        (void)d();
    });
#endif
}
