#!/bin/sh
# Copyright (c) 2013-2016 The Bitcoin Core developers
# Copyright (c) 2017-2018 The AXE Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
cat << "EOF"
     ___           ___           ___                    ___           ___           ___           ___
    /\  \         |\__\         /\  \                  /\  \         /\  \         /\  \         /\  \
   /::\  \        |:|  |       /::\  \                /::\  \       /::\  \       /::\  \       /::\  \
  /:/\:\  \       |:|  |      /:/\:\  \              /:/\:\  \     /:/\:\  \     /:/\:\  \     /:/\:\  \
 /::\~\:\  \      |:|__|__   /::\~\:\  \            /:/  \:\  \   /:/  \:\  \   /::\~\:\  \   /::\~\:\  \
/:/\:\ \:\__\ ____/::::\__\ /:/\:\ \:\__\          /:/__/ \:\__\ /:/__/ \:\__\ /:/\:\ \:\__\ /:/\:\ \:\__\
\/__\:\/:/  / \::::/~~/~    \:\~\:\ \/__/          \:\  \  \/__/ \:\  \ /:/  / \/_|::\/:/  / \:\~\:\ \/__/
     \::/  /   ~~|:|~~|      \:\ \:\__\             \:\  \        \:\  /:/  /     |:|::/  /   \:\ \:\__\
     /:/  /      |:|  |       \:\ \/__/              \:\  \        \:\/:/  /      |:|\/__/     \:\ \/__/
    /:/  /       |:|  |        \:\__\                 \:\__\        \::/  /       |:|  |        \:\__\
    \/__/         \|__|         \/__/                  \/__/         \/__/         \|__|         \/__/

EOF
set -e
srcdir="$(dirname $0)"
cd "$srcdir"
if [ -z ${LIBTOOLIZE} ] && GLIBTOOLIZE="`which glibtoolize 2>/dev/null`"; then
  LIBTOOLIZE="${GLIBTOOLIZE}"
  export LIBTOOLIZE
fi
which autoreconf >/dev/null || \
  (echo "configuration failed, please install autoconf first" && exit 1)
autoreconf --install --force --warnings=all
