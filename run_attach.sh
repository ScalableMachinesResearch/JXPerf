#!/bin/bash

ATTACH=$JXPerf_HOME/attach/attach
PID="$1"
DURATION="$2"
pid=100041

"$ATTACH" "$PID" load instrument false $JXPerf_HOME/thirdparty/allocation-instrumenter/target/java-allocation-instrumenter-HEAD-SNAPSHOT.jar
"$ATTACH" "$PID" load $JXPerf_HOME/build/libagent.so true NUMA::MEM_LOAD_UOPS_RETIRED:L1_MISS:precise=2@10000s
while (( DURATION-- > 0 )) 
do
    sleep 1
done
"$ATTACH" "$PID" load $JXPerf_HOME/build/libstop.so true NUMA::MEM_LOAD_UOPS_RETIRED:L1_MISS:precise=2@10000p
