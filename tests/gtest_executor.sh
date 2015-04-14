#!/bin/sh

# This script executes GTest tests in a controlled manner.
# Setting VERBOSE_TESTS lets output go to stderr instead of a log.

test_prg="$1"
test_name=`basename "$test_prg"`
rc=0

if [ "0${VERBOSE_TESTS}" -eq 0 ]; then
    "$test_prg" \
        --gtest_output=xml:"${test_name}_details.xml" \
        2> "${test_name}_stderr.log"
    rc=$?

    if [ $rc != 0 ]; then
        echo "Test $test_prg exited non-zero. See ${test_name}_stderr.log for details."
    fi
else
    echo "Running $test_prg..."
    "$test_prg" \
        --gtest_output=xml:"${test_name}_details.xml"
    rc=$?

    if [ $rc != 0 ]; then
        echo "Test $test_prg exited non-zero."
    fi
fi

exit $rc
