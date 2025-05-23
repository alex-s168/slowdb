
#include <stddef.h>

// AUTOGENERATED BY https://github.com/alex-s168/build.c/blob/main/gen_kvfn.py
//
// SOURCE:   slowdb__runtime_assert__impl = __attribute__((noreturn)) void runtime_assert__backing(char const* file, size_t line, char const* cond, kv char const* message, kv int exit_code);


#ifndef __runtime_assert__backing__
#define __runtime_assert__backing__

#define __runtime_assert__backing__PASTE2(A,B) A ## B
#define __runtime_assert__backing__CAT2(A,B) __runtime_assert__backing__PASTE2(A,B)
#define __runtime_assert__backing__EMPTY()   /**/
#define __runtime_assert__backing__DELAY(F)  F __runtime_assert__backing__EMPTY()

#define __runtime_assert__backing___message message.have=1, .message.value.have
#define __runtime_assert__backing___exit_code exit_code.have=1, .exit_code.value.have

struct __runtime_assert__backing__argt
{
  char const* file;
  size_t line;
  char const* cond;
  struct { int have; union { char const* have; char _none; } value; } message;
  struct { int have; union { int have; char _none; } value; } exit_code;
};

__attribute__((noreturn)) void slowdb__runtime_assert__impl (struct __runtime_assert__backing__argt a);

#define runtime_assert__backing(_in_file, _in_line, _in_cond, ...) slowdb__runtime_assert__impl((struct __runtime_assert__backing__argt){ .file=(_in_file), .line=(_in_line), .cond=(_in_cond) __runtime_assert__backing__CAT2(__runtime_assert__backing__ATTRS, __VA_OPT__(1))(__VA_ARGS__) })

#define __runtime_assert__backing__ATTRS() /**/
#define __runtime_assert__backing__ATTRS1(attr, ...)  ,. __runtime_assert__backing__DELAY(__runtime_assert__backing__CAT2(__runtime_assert__backing___, attr)) __runtime_assert__backing__CAT2(__runtime_assert__backing__ATTRS, __VA_OPT__(2))(__VA_ARGS__)
#define __runtime_assert__backing__ATTRS2(attr, ...)  ,. __runtime_assert__backing__DELAY(__runtime_assert__backing__CAT2(__runtime_assert__backing___, attr)) __runtime_assert__backing__CAT2(__runtime_assert__backing__ATTRS, __VA_OPT__(3))(__VA_ARGS__)

#endif
