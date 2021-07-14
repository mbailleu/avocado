#! /usr/bin/env bash

set -x

BUILDDIR="/scratch/maurice/avocado/build"
SRCDIR=$(pwd)

MODULES="$SRCDIR/cmake_modules"

[ ! -e "$BUILDDIR" ] && mkdir -p "$BUILDDIR"
[ ! -e "build" ] && mkdir "build"
mountpoint -q -- "$SRCDIR/build" || sudo mount --bind "$BUILDDIR" "build"
[ ! -e "$BUILDDIR/config" ] && ln -s "$SRCDIR/config-$(< /etc/hostname)" "$BUILDDIR/config"
[ ! -e "$BUILDDIR/benchmark" ] && ln -s "$SRCDIR/config-benchmark" "$BUILDDIR/benchmark"

#DOWNLOAD cmake dependencies
[ ! -e "$MODULES" ] && mkdir -p "$MODULES"
declare -A cmake_deps=( 
["GetGitRevisionDescription.cmake"]="https://raw.githubusercontent.com/rpavlik/cmake-modules/master/GetGitRevisionDescription.cmake"
["GetGitRevisionDescription.cmake.in"]="https://raw.githubusercontent.com/rpavlik/cmake-modules/master/GetGitRevisionDescription.cmake.in"
)

for f in ${!cmake_deps[@]};
do
[ ! -e "$MODULES/$f" ] && curl "${cmake_deps[$f]}" -o "$MODULES/$f"
done
