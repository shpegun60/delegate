# Adversarial audit suites. Each executable exercises one build flavor:
#  - core_tests:     default fail-closed policy, zero-heap accounting
#  - cross_tu_tests: object created in one TU, moved/destroyed in another (ODR)
#  - heap_tests:     TINY_DELEGATE_ENABLE_HEAP_FALLBACK=1, exact alloc counting
#  - lenient_tests:  custom TINY_DELEGATE_ASSERT override (counting, no trap)

add_executable(core_tests core_tests.cpp)
add_executable(edge_tests edge_tests.cpp)
add_executable(cross_tu_tests cross_tu_impl.cpp cross_tu_tests.cpp)
add_executable(heap_tests heap_tests.cpp)
add_executable(lenient_tests lenient_tests.cpp)

foreach(t core_tests edge_tests cross_tu_tests heap_tests lenient_tests)
    target_link_libraries(${t} PRIVATE tiny_delegate::tiny_delegate)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic
            -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast)
    elseif(MSVC)
        target_compile_options(${t} PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME ${t} COMMAND ${t})
endforeach()
