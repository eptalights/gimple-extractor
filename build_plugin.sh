#!/usr/bin/env bash

cd /gimple_extractor
make clean
make
make check
echo "done ..."