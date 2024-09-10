#!/bin/env sh

case "$OSTYPE" in
  msys*)    f=bbae.exe ;;
  *)        f=bbae.out ;;
esac
case "$OSTYPE" in
  msys*)    asan="" ;;
  *)        asan="-fsanitize=address" ;;
esac

clang --std=c99 src/main.c -Wall -Wextra -pedantic -O0 -g -ggdb $asan -Wno-unused-function -o $f || exit 1
./$f "$@"
