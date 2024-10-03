#!/bin/env sh

case "$OSTYPE" in
  msys*)    f=tests.exe ;;
  *)        f=tests.out ;;
esac
case "$OSTYPE" in
  msys*)    std=c99 ;;
  *)        std=gnu99 ;; # functions like fdopen aren't exported by the standard library unless compiling in GNU mode
esac
case "$OSTYPE" in
  msys*)    asan="" ;;
  #*)        asan="-fsanitize=address" ;;
  *)        asan="-fsanitize=undefined -fno-sanitize=function" ;;
esac

clang --std=$std src/tests.c -Wall -Wextra -pedantic -O0 -g -ggdb $asan -Wno-unused-function -o $f || exit 1
./$f
