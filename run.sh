#!/bin/bash

export GPU_MAX_HEAP_SIZE=100
export GPU_MAX_USE_SYNC_OBJECTS=1
export GPU_SINGLE_ALLOC_PERCENT=100
export GPU_MAX_ALLOC_PERCENT=100
export GPU_MAX_SINGLE_ALLOC_PERCENT=100
export GPU_ENABLE_LARGE_ALLOCATION=100
export GPU_MAX_WORKGROUP_SIZE=1024

./xbtgpuarc \
    --platform 0 \
    --device 0 \
    --algo zhash_144_5 \
    --pool solo-btg.2miners.com \
    --port 4040 \
    --wallet Gb4V4a9Jk3p8aH6jkW3Aq3sq8rQCuJQ6S8 \
    --worker A730m \
    --password x \
    --intensity 256
