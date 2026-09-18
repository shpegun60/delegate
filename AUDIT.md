# Delegate audit — 2026-09-12

Audited the C++17/C++20 header, all three delegate types, configuration and
lifetime contracts, CMake tests, Qt test bench, comments, documentation and CI.
The starting revision was `3d970b5e92a950fbcd52ac3153b28d23c99092c2`.
The final header's SHA-256 in the audited working tree is
`5cdfd130e85709b302bf02e6e769d05136fd4509290cb90ef3ae83098e52f4eb`.
Line-ending conversion during checkout can change this byte-level hash.

All local validation below passed. This records a tested contract and regression
coverage; it is not a proof that arbitrary application callbacks are safe.

## Corrections and compile-time protection

- Assignment from a reference to the currently owned callable previously
  destroyed the source before copying it. A minimal original-header reproduction
  produced an ASan `heap-use-after-free`. Assignment now preserves an aliased
  source before reset. The regression runs for both owning types with heap
  fallback enabled and disabled.
- Reset now clears ownership metadata before destroying the target. A target
  destructor can reset its owner again without recursively destroying itself.
  Inline moves clear the source before its moved-from target is destroyed.
- `bind` rejects null function/member template arguments and temporary objects.
  Explicitly specifying a reference as the object template parameter no longer
  bypasses temporary-object rejection.
- Construction and assignment check invocation, return-reference compatibility,
  source construction, non-throwing destruction, capacity, alignment and move
  policy during overload resolution. Deleted diagnostic overloads name the
  failure and storage dimensions while keeping `std::is_constructible` and
  `std::is_assignable` truthful. `fits_inline` remains a size/alignment query.
- Reference results must already refer to a compatible type; adaptation cannot
  manufacture a temporary. This deliberately rejects implicit user-defined
  reference conversions. GCC 13 already rejected some direct dangling-result
  examples in the original header; those are additional portable constraints
  and trait/diagnostic improvements, not newly demonstrated original UB.
- Function-pointer conversions use the actual source cv/ref category. Valid
  converters no longer instantiate an unnecessary owning-storage path.
  `call_or` handles `void` result adaptation and only inspects/invokes a fallback
  when the delegate is empty. `call_if` explains unsupported reference or
  immovable results.
- Null adapted callables and null borrowed pointers follow the assert policy
  and leave an empty delegate if the hook returns. A class-specific non-throwing
  allocator returning null also leaves the delegate empty. Invocation through
  a borrowed pointer is checked in Debug.
- `delegate_ref` stores object and function pointers in separate union members.
  It no longer relies on conditionally supported conversions between these
  pointer categories. Its two-word layout and constexpr ROM bindings remain.
  The relevant language rules are [pointer conversions](https://eel.is/c++draft/expr.reinterpret.cast)
  and [INVOKE result adaptation](https://eel.is/c++draft/func.require).
- The public header has an include guard alongside `#pragma once`. Redundant
  comments were removed and implementation/test comments were corrected.
  README contracts now cover exceptions, reentrancy, borrowed lifetimes,
  concurrency and configuration consistency.

## Validation

| Toolchain / target | Configurations | Result |
|---|---|---|
| GCC 13.3.0, WSL Ubuntu 24.04 | C++17/20, Debug/Release; C++20 ASan + UBSan | 5 × 12 CTest entries passed |
| Clang 18.1.3, WSL Ubuntu 24.04 | C++17/20, Debug/Release; C++20 ASan + UBSan | 5 × 12 CTest entries passed |
| MSVC 19.50.35719.0 | C++17/20, Debug/Release | 4 × 11 CTest entries passed |
| Qt 6.10.1, MinGW 13.1 | Console suite; Debug/Release offscreen UI autorun | 145 checks passed; both UI runs exited successfully |
| ARM GCC 13.2.1 | Cortex-M0/M3/M4/M7/M33 × C++17/20 × assert enabled/disabled | 20 compile configurations passed |

GCC/Clang builds used `-Werror`; MSVC used `/WX`. ARM builds used
`-fno-exceptions -fno-rtti -ffreestanding`. Sanitizer configurations covered both
heap policies. Each GCC/Clang CTest run included 36 negative compilation cases:
a valid control must compile, then each invalid source must fail with its
expected diagnostic. MSVC ran the runtime suites and their static assertions;
the negative compiler-output harness currently targets GCC/Clang.

Checks remain active in Release while the library uses its actual `NDEBUG`
policy. POSIX Debug tests verify trap signals in subprocesses. Exception tests
check failed copies/allocations, leak accounting and reuse. Cross-TU tests
exercise ownership transfer, destruction and reference-manager identity.
Over-aligned allocation counters cover aligned `new`/`delete`.

The Qt bench previously assigned typed null function pointers as a reset test,
which triggers the documented Debug trap. It now uses `nullptr` for reset;
dedicated policy tests cover invalid typed-null input.

## Generated code and storage

Compared object disassembly from the original and final headers using the same
Cortex-M4 compiler, probe, `-DNDEBUG`, and optimization settings. The instruction
sequences below match. Counts exclude alignment padding and literal data.

| Probe | `-O2`, original → final | `-Os`, original → final |
|---|---|---|
| `int(int)` dispatch, each of the three delegate types | 2 → 2 (`ldrd; bx`) | 2 → 2 (`ldrd; bx`) |
| `delegate_ref` function-pointer assignment | 6 → 6 | 5 → 5 |
| `delegate_ref` borrow assignment | 3 → 3 | 4 → 4 |
| `delegate_ref` method binding | 3 → 3 | 4 → 4 |

ARM assertions pin `delegate_ref<void()>` to 8 bytes and both default owning
types to 32 bytes. The static binding probe has an 8-byte `.rodata` entry with
no dynamic initializer. [check_codegen.py](tests/check_codegen.py) enforces
dispatch and rebinding budgets using [dispatch_probe.cpp](tests/dispatch_probe.cpp).

These results describe the supplied probes, not cycle measurements or every
possible signature/compiler. Owning functor assignment may need a source-address
comparison and, on aliasing, a temporary copy/move to preserve its source.
That work occurs during assignment, outside the invocation path. Callable
bodies and their argument/result operations can still allocate or throw.

## Reproduction and contract limits

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 tests/check_codegen.py --build-dir build-arm-codegen
```

The code-generation command requires `arm-none-eabi-g++` and
`arm-none-eabi-objdump`. Local logs, baseline reproductions, compiler matrices
and disassembly are retained in the ignored `build-audit/` directory.
The updated workflow defines 16 host jobs, two sanitizer jobs and one ARM job;
its YAML and matrix were checked locally. Remote results must be taken from the
CI run for the exact published commit; the matrix above describes local
execution. No macOS or target hardware execution was performed locally.

Borrowed targets, reference captures and returned references require externally
valid lifetimes. Concurrent mutation needs caller synchronization. Rebinding
from target constructors/destructors and assignment from a subobject destroyed
by replacement are outside the contract; see [README.md](README.md).
Empty `operator()` remains invalid, with the default check removed by `NDEBUG`.
Use `call_if` or `call_or` when empty is an expected state.

Rebuild every consumer with the same header revision and configuration:
the reference invoker's internal type changed even though its size did not.

## Version 1.1.0 extension — 2026-09-18

The follow-up starts from `e76affb87f479399eec12e80d9ce03e74b4451d9`.
`delegate_ref` now directly constant-initializes the function member of its
payload union. Ordinary function pointers and bare `[]`/`+[]` captureless lambdas
can initialize constexpr tables in C++17 and C++20. A matching assignment
overload preserves bare-lambda rebinding, and derived-view copying keeps its
previous behavior. All three types now use the implicit function-pointer
conversion admitted by their constraints, including mixed implicit/explicit
ref-qualified conversion operators. Calls still execute at runtime; target
functions need not be constexpr. The owning types retain their placement construction and
non-trivial destructors, and therefore are not constexpr table elements.

The tested header SHA-256 is
`82af1433730d1f7224c07b21c1b0d0b5e0941b84d171faa9f137b85bd58ffa68`.
As above, checkout line-ending conversion can alter a byte-level file hash.

Local validation of all three types:

| Toolchain / target | Configurations | Result |
|---|---|---|
| GCC 13.3 and Clang 18.1, Linux | C++17/20, Debug/Release/ASan+UBSan, warnings as errors | 12 configurations, each 14/14 CTest entries passed |
| MSVC 19.50, x64 | C++17/20, Debug/Release, `/WX` | 4 configurations, each 13/13 CTest entries passed |
| Qt 6.10.1, MinGW 13.1 | C++17 Debug/Release console and offscreen UI | 145/145 console checks in each configuration; both UI runs exited 0 |
| CubeIDE ARM GCC 14.3 | Cortex-M0/M3/M4/M7/M33, C++17/20, assertions enabled/disabled | 20 compile configurations passed |
| ARM GCC 13.2 and 14.3 | Cortex-M4, `-O2`/`-Os` | Dispatch/rebinding comparison and ROM placement checks passed |

The new tests exercise function, address, noexcept-function, inline-lambda,
template-function and method bindings; live target data; constexpr copy/reset;
void/reference results; runtime rebinding; derived-view copying; implicit versus
explicit conversions; and function-pointer compatibility across the three types.
Conversion failures must preserve a previous function, inline or heap binding.
Each Linux CTest run includes the 36 expected-error
compile cases. Sanitizers remain enabled for both owning storage policies.
MSVC runs runtime/static-assert tests; the diagnostic-matching negative harness
remains GCC/Clang-specific. Qt Release does not define `NDEBUG` in the supplied
project; the CMake Release matrices establish the library's Release policy.

Independent probes additionally exercised 100,000 function/object payload
transitions; 10,000 vector relocations per owning type; nested wrappers;
over-aligned heap targets; throwing pointer conversions; callable lifetime
accounting; returning/throwing custom assertion hooks; and reference adaptation.
No undefined behavior was found for calls satisfying the documented contracts.

The complete dispatch-probe object files are byte-identical to the starting
revision with the same compiler and flags, for both ARM compilers at both
optimization levels. All three `int(int)` dispatch sites remain `ldrd; bx`.
GCC 14.3 emits six instructions for pointer assignment at `-Os`, whereas
GCC 13.2 emits five, including in the baseline. The check now accepts the
measured six-instruction budget and rejects added function calls.
The five-entry direct-function/lambda/template-binding table occupies 40 bytes
of `.rodata`; the probe has no writable data or dynamic initializer sections.
This is code-generation evidence, not a hardware cycle measurement.

GCC 13.3 with UBSan rejects a constexpr null comparison for some function-template
addresses even in a minimal program without the library. CMake detects that
compiler limitation. Only that constexpr template test uses the existing
`bind<>` spelling there; direct runtime template construction and ordinary
direct constexpr functions/lambdas remain tested. The direct constexpr template
case is active in the other configurations. Null guards and sanitizers are
not bypassed. A custom assertion hook must also permit constant evaluation
on valid input; an unconditional call to a non-constexpr logger prevents it.

The existing caller obligations remain: borrowed objects must outlive their
callbacks, concurrent access needs synchronization, and user callables must be
valid. An empty `operator()` is still outside the contract (unchecked in Release
with the default policy); use `call_if`/`call_or` for a legal empty state or a
non-returning custom guard when runtime checks are required in Release.

Evidence is retained locally under ignored `build-release-1.1/`. The runtime
tests, compiler capability check and ARM guards are committed and run by CI.
These are local results; remote status must be checked for the published commit.
