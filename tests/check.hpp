#pragma once
#include <cstdio>
#include <cstdlib>

// Unlike assert(), test observations must execute in Release too. Do not
// change NDEBUG: it also selects the library policy in every translation unit.
#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #__VA_ARGS__); \
    std::abort(); \
} } while (false)
