#!/bin/bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Check that all logs are terminated with '\n'
#
# Some logs are continued over multiple lines. They should be explicitly
# commented with \* Continued *\
#
# There are some instances of LogPrintf() in comments. Those can be
# ignored


UNTERMINATED_LOGS=$(git grep "LogPrintf(" -- "*.cpp" | \
    grep -v '\\n"' | \
    grep -v "/\* Continued \*/" | \
    grep -v "LogPrintf()")
if [[ ${UNTERMINATED_LOGS} != "" ]]; then
    echo "All calls to LogPrintf() should be terminated with \\n"
    echo
    echo "${UNTERMINATED_LOGS}"
    exit 1
fi
