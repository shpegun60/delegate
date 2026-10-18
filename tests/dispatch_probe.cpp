// Stable out-of-line call sites for comparing generated code across revisions.
#include "tiny_delegate.hpp"

struct DispatchTarget {
    int value;
    int operator()(int n) { return value + n; }
    int method(int n) { return value + n; }
};
using Ref = tiny::delegate_ref<int(int)>;
using Own = tiny::delegate<int(int)>;
using Sbo = tiny::delegate_sbo<int(int)>;
int context_adapter(DispatchTarget& target, int n) { return target.value + n; }

extern "C" {
int dispatch_ref(const Ref& d, int n) { return d(n); }
int dispatch_own(const Own& d, int n) { return d(n); }
int dispatch_sbo(const Sbo& d, int n) { return d(n); }
bool query_heap_own(const Own& d) { return d.uses_heap(); }
bool query_heap_sbo(const Sbo& d) { return d.uses_heap(); }
void assign_ref(Ref& d, int (*f)(int)) { d = f; }
void borrow_ref(Ref& d, DispatchTarget& f) { d = tiny::borrow(f); }
void bind_ref(Ref& d, DispatchTarget& f) { d = Ref::bind<&DispatchTarget::method>(f); }
void bind_context_ref(Ref& d, DispatchTarget& f) { d = Ref::bind_context<&context_adapter>(f); }
void borrow_own(Own& d, DispatchTarget& f) { d = tiny::borrow(f); }
void assign_own(Own& d, DispatchTarget f) { d = f; }
}
