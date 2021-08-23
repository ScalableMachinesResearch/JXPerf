LD_PRELOAD=$OJXPerf_HOME/build/libpreload.so java -javaagent:$JAVA_AGENT -agentpath:$OJXPerf_HOME/build/libagent.so=OBJLEVEL::MEM_UOPS_RETIRED:ALL_LOADS:precise=2@10000 -jar ./dacapo.jar avrora
# export LD_PRELOAD=$OJXPerf_HOME/build/libpreload.so
# java -javaagent:$JAVA_AGENT -jar ./dacapo.jar -s large avrora
