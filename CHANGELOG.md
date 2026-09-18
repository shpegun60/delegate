# Changelog

## 1.1.0 — 2026-09-18

- `tiny::delegate_ref` supports constant initialization from ordinary function
  pointers and inline `[]`/`+[]` captureless lambdas in C++17 and C++20. Targets may
  be ordinary or `noexcept` functions and may read changing runtime data.
- Existing `bind` forms, runtime function assignment, borrowing, empty-call
  policies and ownership APIs retain their contracts. Invocation itself is
  not a constant-expression operation.
- Bare-lambda construction and assignment store the converted function pointer
  without retaining the closure. Capturing lambdas still require an explicit
  borrow or an owning delegate. Base-view copying from derived `delegate_ref`
  objects retains its existing behavior.
- All three types consistently use the implicit function-pointer conversion
  admitted by their constraints. This corrects an owning-type edge case where
  a competing explicit ref-qualified conversion could be selected instead.
  A throwing conversion during assignment preserves the previous binding.
- `delegate_ref` retains its two-word layout (8 bytes on ARM32). ARM checks
  cover direct-function/lambda table placement in `.rodata`, absence of dynamic
  initialization, and the dispatch/rebinding budgets for all three types.
- Dedicated regression tests cover the new construction forms and their
  compatibility, conversion selection and runtime function-pointer APIs of `delegate_ref`,
  `delegate` and `delegate_sbo`.

`delegate` and `delegate_sbo` continue to manage owned objects at runtime.
Their placement construction and non-trivial destructors do not support
constexpr tables; use `delegate_ref` for a static catalog or dispatch table.

For constant initialization, custom assertion hooks must permit constant
evaluation on valid input. GCC 13.3 with UBSan has a function-template pointer
comparison limitation; the existing `bind<&function<Args...>>()` form avoids
it. Tests detect the compiler limitation without disabling sanitizers or
changing the library's null-pointer policy. See [README.md](README.md).

Validation details and contract limits are recorded in [AUDIT.md](AUDIT.md).
