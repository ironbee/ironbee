#!/bin/bash

# This script executes GTest tests in a controlled manner.

test_prg="$1"
test_name=`basename "$test_prg"`

"$test_prg" \
    --gtest_output=xml:"${test_name}_details.xml" \
    2> "${test_name}_stderr.log"

