#!/bin/bash
set -ex

cd "$MESON_DIST_ROOT"
DIR=$(mktemp -d)

meson setup "$DIR"
meson compile -C "$DIR"
cp -v "$DIR"/doc/zathura* doc/
rm -rf "$DIR"
