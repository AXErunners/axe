#!/bin/sh
# Copyright (c) 2013-2016 The Bitcoin Core developers
# Copyright (c) 2017-2018 The AXE Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
cat << "EOF"

         █████╗    ██╗  ██╗   ███████╗
        ██╔══██╗   ╚██╗██╔╝   ██╔════╝
        ███████║    ╚███╔╝    █████╗
        ██╔══██║    ██╔██╗    ██╔══╝
        ██║  ██║   ██╔╝ ██╗   ███████╗
        ╚═╝  ╚═╝   ╚═╝  ╚═╝   ╚══════╝

   ██████╗    ██████╗    ██████╗    ███████╗
  ██╔════╝   ██╔═══██╗   ██╔══██╗   ██╔════╝
  ██║        ██║   ██║   ██████╔╝   █████╗
  ██║        ██║   ██║   ██╔══██╗   ██╔══╝
  ╚██████╗   ╚██████╔╝   ██║  ██║   ███████╗
   ╚═════╝    ╚═════╝    ╚═╝  ╚═╝   ╚══════╝
   
EOF
printf "\033[1m\033[31mAXE\033[0m\n\033[33;2mWarming up...\033[0m\n"
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
