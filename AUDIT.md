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

## Version 1.2.0 extension — 2026-09-20

This extension starts from `a95a9d772570932511ff57325d33ab38f411a624`.
`delegate_ref` and the hybrid `delegate` add
`bind_context<&adapter>(context)`: the adapter is a compile-time free-function
target and the stable lvalue context is the only stored payload. The resulting
`delegate_ref` remains two words. The hybrid uses its existing non-owning
manager and does not allocate. `delegate_sbo` remains owning-only.

The binding preserves the exact context type and recovers it before invocation;
it does not convert a function pointer to an object pointer. Const contexts,
derived-to-base argument adjustment, `void` results and safe reference results
are covered. Deleted overloads and static assertions reject temporary contexts,
null or member-function targets, argument mismatches and reference adaptations
that would manufacture a temporary. As with every borrowed binding, the context
must remain alive at the same address for every call.

The tested header SHA-256 is
`4664d6d36ea8f0b2b38286473e1a244fb74ccb992dafa22b61a4376d4171f359`.
Line-ending conversion can change this byte-level hash.

Local validation:

| Toolchain / target | Configurations | Result |
|---|---|---|
| GCC 13.3.0, WSL Ubuntu 24.04 | C++17/20 Debug/Release; C++20 ASan + UBSan; `-Werror` | 5 configurations, each 15/15 CTest entries passed |
| MinGW GCC 13.1, Windows | C++17/20 Debug/Release; `-Werror` | 4 configurations, each 15/15 CTest entries passed |
| MSVC 19.50.35719, x64 | C++17/20 Debug/Release; `/W4 /WX /permissive- /EHsc` | 4 configurations, each 14/14 CTest entries passed |
| Qt 6.10.1, MinGW 13.1 | C++17 Debug/Release offscreen UI autorun | both runs exited successfully |
| CubeIDE ARM GCC 14.3.1 | Cortex-M0/M3/M4/M7/M33, C++17/20, assertions enabled/disabled | 20 compile configurations passed |

Every GCC CTest run includes 42 expected-error compilation cases. The six new
cases cover each rejected `bind_context` contract. MSVC runs the runtime and
static-assert suites; diagnostic text matching remains GCC/Clang-specific. The
sanitizer build covers both owning storage policies and the new borrowed path.
The Windows matrix also exposed a test-only portability assumption: aligned
allocation accounting now uses `_aligned_malloc`/`_aligned_free` for every
Windows compiler and `std::aligned_alloc` elsewhere.

On Cortex-M4 at both `-O2` and `-Os`, invocation for `delegate_ref`, `delegate`
and `delegate_sbo` remains exactly `ldrd; bx`. Creating a context binding is
three instructions at `-O2` and four at `-Os`, with no helper call. Existing
borrow and member binding have the same budgets. The five-entry constexpr table
still occupies 40 bytes of `.rodata` and creates no writable or startup section.

Clang and macOS validation for the published revision belongs to remote CI;
this section records only the local runs above. Exact remote status must be
checked against the final published commit, not inferred from an older run.

## Version 1.2.1 audit — 2026-10-18

Three parallel reviews covered the complete header implementation, test and
documentation contracts, generated ARM code, and the telemetry/STM32 consumers.
The starting library checkout was clean at
`5143bfa6ae2372b60f387feca57a69f7a55169db`. A fresh fetch and `ls-remote`
confirmed that both GitHub `main` and the latest release tag, `v1.2.0`, point
to that commit. Its [CI run](https://github.com/shpegun60/delegate/actions/runs/35478907244)
completed successfully with 19/19 jobs. Those remote results cover the starting
commit. The validation below was collected before publication of this patch
release; remote CI must be checked against the commit containing these changes.

### Confirmed defect and correction

With heap fallback enabled, a callable having only a mutable copy constructor,
`F(F&)`, was accepted by `is_constructible` and `is_assignable`. Construction
worked, but even non-aliased assignment failed to compile: the runtime alias
branch unconditionally instantiated a move from its saved target, which that
callable cannot perform. Both owning types now preserve the original source's
cv/ref category when the `move_if_noexcept` expression cannot construct the
target. This also handles a constructor accepting only `volatile F&&` while the
ordinary move constructor is deleted; merely copying a saved mutable lvalue
would still reject that otherwise-admitted source.

The heap regressions exercise both source categories, both wrappers, normal
assignment and `d = d()`, check preserved state and verify balanced
construction/destruction and allocation/deallocation. This is a compile-time
admission defect, not a newly demonstrated runtime memory error. No additional
lifetime or undefined-behavior defect was confirmed for calls satisfying the
documented contracts.

### Optimization and its limits

With heap fallback disabled, both owning types now return `false` directly from
`uses_heap()`. The query does not need a manager load or an indirect call. The
existing object layout and invocation ABI remain unchanged.

ARM GCC 13.2.1 and 14.3.1 emit `movs; bx` (4 bytes) for each no-heap query
at `-O2` and `-Os`. In the GCC 13.2.1 baseline comparison, the `-Os` query used
12 bytes and manager dispatch; heap-enabled query objects remain byte-identical
to that baseline. With GCC 13.2.1, the original dispatch probe, compiled
separately against the starting and final headers, also produces byte-identical
object files at both optimization levels. GCC 14.3.1 passed the code-generation
gates; whole-object baseline comparisons were not collected for that compiler.
The added permanent query probes are checked by `check_codegen.py`.

A separate move-count probe exposed a limit for non-trivial arguments passed by
value: the tested delegate call performs two argument moves, compared with one
for `std::function` and zero for a direct call from a prvalue. Changing every
trampoline to take references would change scalar and small-structure calling
conventions. Selecting by size/triviality also needs care because the current
wrappers support signatures containing incomplete argument types. No dispatch
ABI redesign was introduced during this audit. Scalar/reference callback probes
and target-specific disassembly remain the relevant performance evidence.
The independent sources, baseline header, exact commands and output are
retained in `build-audit-oct18/header-probes/`, including the incomplete-signature
and before/after assignment diagnostics probes.

README performance statements now distinguish recorded timings and instruction
counts from unmeasured cycle rankings. Inline storage avoids wrapper allocation
only when the target meets the storage policy; callable operations can still
allocate. A known local delegate binding can be inlined by the compiler.

### Local verification

The final header SHA-256 is
`b9bc65327969a766ed3e076f2665add228145b6349508e30ada02222ed4d2e69`.
This is a working-file hash; checkout line-ending conversion can change it.

- GCC 13.3: C++17/20, Debug/Release, plus C++20 ASan/UBSan, `-Werror`, five
  configurations with 15/15 CTest entries passing in each.
- Clang 18.1.3: C++17 Debug and C++20 ASan/UBSan, `-Werror`, two
  configurations with 15/15 CTest entries passing in each.
- Each GCC/Clang configuration includes a successful control compilation and
  rejection of 42 misuse cases with the expected diagnostics.
- MSVC x64: C++17/20, Debug/Release, `/WX /EHsc`, four configurations with
  14/14 CTest entries passing in each.
- Qt 6.10.1 / MinGW 13.1: fresh C++17 Debug and Release builds; both offscreen
  `--autorun-exit` test-suite runs exited 0.
- ARM GCC 13.2.1: Cortex-M0/M3/M4/M7/M33, C++17/20, assertions enabled and
  disabled, 20 compile configurations passed without exceptions or RTTI.
- ARM GCC 13.2.1 and CubeIDE GCC 14.3.1: `-O2`/`-Os` code-generation gates
  passed. Each of the three scalar invocation sites remains `ldrd; bx`.
  The five-entry constexpr table remains 40 bytes in `.rodata`, without
  writable or startup sections.

Windows logs are retained under `build-audit-oct18/`. Linux and ARM evidence is
retained under `build-agent-audit/`. No new board execution or cycle measurement
was performed. The external lifetime, concurrency, aliasing and reentrancy
contracts above still apply.

### Consumer synchronization and verification

At the start, the standalone telemetry checkout was clean at
`f760d69e80200d30c2e2c62c1426700d9a30ad99`, matching live GitHub `main`.
Its [CI run](https://github.com/shpegun60/telemetry/actions/runs/39073259430)
was successful for that exact starting SHA. Its `lib/delegate` header matched
the unmodified v1.2.0 release byte for byte.

The actual H753 firmware uses a separate header under
`power_analyzer_h753/lib/delegate`, inside the power_analyzer repository. That
copy was still based on `3d970b5e92a950fbcd52ac3153b28d23c99092c2` and did not
contain the previous hardening or later releases. The firmware repository's
`night_builds` HEAD, `c4a724aeb6e68e91c2a702237d1735319ea84301`, matches its live
Bitbucket branch. Existing unrelated documentation/tool-setting edits were
left in place; audit source edits were limited to the delegate headers and
their provenance READMEs in the consumers.

Both vendor headers now match this audit's final header byte for byte, with
their READMEs recording the upstream base, both local changes and the actual
SHA-256. The library invocation ABI and configuration macros remain consistent
within each rebuilt program. Current HTTP route bindings target long-lived
service objects; registration precedes dispatch in the web task. No current
application lifetime or ISR/task race defect was confirmed by this source
inspection.

The firmware's maintained DelegateCheck passed its 12 host checks, oversized
callable rejection and CubeIDE GCC 14.3.1 Cortex-M7 compile check. A full clean
firmware build completed with the first corrected header; after the final
cv/ref-category correction, all 18 affected objects were rebuilt and linked
successfully, with no diagnostics in that final rebuild. The clean build had
22 existing warnings outside the delegate code. Final image sections are
text 856088, data 14984 and bss 831899 bytes; these are build sizes, not a
controlled code-size benchmark against the previous header. The final ELF
SHA-256 is
`eca233672e8d80349dfe1f08231e0a3b06ea7c049f985e250d952d25349b7524`.

The pinned COBS/telemetry integration passed 368 checks with Clang 18 ASan/UBSan.
Its `summary.json` records unchanged before/after input hashes, including the
final delegate header. The complete Clang 18.1.3 C++20 ASan/UBSan consumer
runner completed all 352 commands: 125 successful positive checks and 227
expected compile refusals. Its 22 executions comprise 16 normal programs and
six intentional-abort controls, with 1,572,379 runtime checks and no unexpected
failures. It also checked 55 standalone public headers. The telemetry ARM
consumer gate completed all 336 commands: 114 successful positive checks and
222 expected compile refusals.
CubeIDE GCC 14.3.1 used Cortex-M7 hard-float, C++20, `-O2`/`-Os`, no exceptions
and no RTTI; slot dispatch matched the manual comparisons, and both ID codegen
and mutation-control probes passed. Consumer evidence is retained under the
telemetry checkout's `build/delegate-audit-2026-10-18/` directory, specifically
`final-sanitized/summary.json`, `final-arm-cubeide/summary.json` and
`final-cobs-sanitized/summary.json`. `consumer-final.json` records the final
header hashes before and after the gates, repository HEADs, ELF hash and the
clean/intermediate versus final rebuild distinction. These results describe the
tested working-tree snapshot before publication. Version 1.2.1 includes the
audit corrections relative to v1.2.0; the starting-SHA remote CI does not qualify
those corrections.
