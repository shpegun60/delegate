# Separate executables keep each configuration consistent across its TUs.

add_executable(core_tests core_tests.cpp)
add_executable(edge_tests edge_tests.cpp)
add_executable(cross_tu_tests cross_tu_impl.cpp cross_tu_tests.cpp)
add_executable(heap_tests heap_tests.cpp)
add_executable(lenient_tests lenient_tests.cpp)
add_executable(lenient_heap_tests lenient_tests.cpp)
target_compile_definitions(lenient_heap_tests PRIVATE TINY_DELEGATE_ENABLE_HEAP_FALLBACK=1)
add_executable(hardening_tests hardening_tests.cpp)
add_executable(hardening_heap_tests hardening_tests.cpp)
target_compile_definitions(hardening_heap_tests PRIVATE TINY_DELEGATE_ENABLE_HEAP_FALLBACK=1)
add_executable(exception_tests exception_tests.cpp)
add_executable(exception_heap_tests exception_tests.cpp)
target_compile_definitions(exception_heap_tests PRIVATE TINY_DELEGATE_ENABLE_HEAP_FALLBACK=1)
add_executable(policy_tests policy_tests.cpp)

foreach(t core_tests edge_tests cross_tu_tests heap_tests lenient_tests lenient_heap_tests hardening_tests hardening_heap_tests
          exception_tests exception_heap_tests policy_tests)
    target_link_libraries(${t} PRIVATE tiny_delegate::tiny_delegate)
    set_target_properties(${t} PROPERTIES CXX_EXTENSIONS OFF)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic
            -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast)
    elseif(MSVC)
        target_compile_options(${t} PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME ${t} COMMAND ${t})
endforeach()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
    if(CMAKE_CXX_STANDARD)
        set(test_standard ${CMAKE_CXX_STANDARD})
    else()
        set(test_standard 17)
    endif()
    add_test(NAME negative_compile COMMAND ${CMAKE_COMMAND}
        "-DCXX=${CMAKE_CXX_COMPILER}" "-DSTANDARD=${test_standard}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/negative"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/negative_compile.cmake")
endif()
