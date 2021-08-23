#!/bin/bash
export LD_PRELOAD=$OJXPerf_HOME/build/libpreload.so
ATTACH=$OJXPerf_HOME/bin/jattach/attach
# Pick one of following modes
# MODE=Generic::CYCLES:precise=0@1000000
# MODE=ReuseDistance::MEM_UOPS_RETIRED:ALL_LOADS:precise=2@10000,ReuseDistance::MEM_UOPS_RETIRED:ALL_STORES:precise=2@10000
# MODE=DeadStore::MEM_UOPS_RETIRED:ALL_STORES:precise=2@10000
# MODE=SilentStore::MEM_UOPS_RETIRED:ALL_STORES:precise=2@10000
# MODE=SilentLoad::MEM_UOPS_RETIRED:ALL_LOADS:precise=2@10000
# MODE=DataCentric::MEM_LOAD_UOPS_RETIRED:L1_MISS:precise=2@10000
MODE=OBJLEVEL::MEM_UOPS_RETIRED:ALL_LOADS:precise=2@10000

DURATION="$1"
PID="$2"
LOAD=load
INSTRUMENT=instrument
TRUE_FLAG=true
FALSE_FLAG=false
JVMTI_AGENT_START=$OJXPerf_HOME/build/libagent.so

#"$ATTACH" "$PID" "$LOAD" "$INSTRUMENT" "$FALSE_FLAG" $JAVA_AGENT
"$ATTACH" "$PID" "$LOAD" "$JVMTI_AGENT_START" "$TRUE_FLAG" "$MODE"s
while (( DURATION-- > 0 ))
do
    sleep 1
done
"$ATTACH" "$PID" "$LOAD" "$JVMTI_AGENT_START" "$TRUE_FLAG" "$MODE"p
