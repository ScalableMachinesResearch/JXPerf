#!/bin/bash

ATTACH=$JXPerf_HOME/attach/attach
DURATION="$1"
pid=100041

"$ATTACH" "$pid" load instrument false $JXPerf_HOME/thirdparty/allocation-instrumenter/target/java-allocation-instrumenter-HEAD-SNAPSHOT.jar
"$ATTACH" "$pid" load $JXPerf_HOME/build/libagent.so true NUMA::MEM_LOAD_UOPS_RETIRED:L1_MISS:precise=2@10000s
while (( DURATION-- > 0 )) 
do
    sleep 1
done
"$ATTACH" "$pid" load $JXPerf_HOME/build/libstop.so true NUMA::MEM_LOAD_UOPS_RETIRED:L1_MISS:precise=2@10000p
