#pragma once

#define DROP_4_ARG(a1, a2, a3, a4, ...) (__VA_ARGS__)
#define DROP_3_ARG(a1, a2, a3, ...) (__VA_ARGS__)
#define DROP_2_ARG(a1, a2, ...) (__VA_ARGS__)
#define DROP_1_ARG(a1, ...) (__VA_ARGS__)
