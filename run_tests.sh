#!/bin/env sh

case "$OSTYPE" in
  msys*)    f=tests.exe ;;
  *)        f=tests.out ;;
esac
case "$OSTYPE" in
  msys*)    asan="" ;;
  *)        asan="-fsanitize=address" ;;
  #*)        asan="-fsanitize=undefined -fno-sanitize=function" ;;
esac

clang --std=gnu99 src/tests.c -Wall -Wextra -pedantic -O3 -g -ggdb $asan -Wno-unused-function -o $f || exit 1
./$f
