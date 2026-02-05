#!/bin/sh
../build/fuzzers/fuzz_iter -workers=100 -jobs=100 -timeout=120 corpus
