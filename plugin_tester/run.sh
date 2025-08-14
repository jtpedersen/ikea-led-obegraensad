#!/bin/bash
set -e
set -x

ROOT=`pwd`/..

g++ -DHOST_DDP -I ${ROOT}/include -I . host_ddp.cpp ${ROOT}/src/plugins/Blop.cpp  -o ddp


./ddp
