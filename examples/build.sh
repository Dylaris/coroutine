#!/bin/bash

set -xe

gcc -o counter counter.c -I ..
gcc -o sum sum.c -I ..
gcc -o producer_consumer producer_consumer.c -I ..
