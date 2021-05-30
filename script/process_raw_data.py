#!/usr/bin/env python3

import os
#import sys
from pylib import *
from multiprocessing.dummy import Pool as ThreadPool
#from functools import partial

import re
import xml.etree.ElementTree as ET

##global variables
isDataCentric = False
isNuma = False
isGeneric = False
isHeap = False
isAllocTimes = False
isReuseDistance = False
isObjLevel = False

g_thread_context_dict = dict()
g_method_dict = dict()


def get_all_files(directory):
    files = [f for f in os.listdir(directory) if os.path.isfile(
        os.path.join(directory, f))]
    ret_dict = dict()
    for f in files:
        if f.startswith("agent-trace-") and f.find(".run") >= 0:
            start_index = len("agent-trace-")
            end_index = f.find(".run")
            tid = f[start_index:end_index]
            if tid not in ret_dict:
                ret_dict[tid] = []
            ret_dict[tid].append(os.path.join(directory, f))
    return ret_dict

def remove_all_files(directory):
    files = [f for f in os.listdir(directory) if os.path.isfile(
        os.path.join(directory, f))]
    for f in files:
        if f.startswith("agent-trace-") and f.find(".run") >= 0:
            os.remove(f)
        elif f.startswith("agent-statistics") and f.find(".run"):
            os.remove(f)

def thread_parse_input_file(input_data):
    tid = input_data[0]
    file_name = input_data[1]
    xml_root_dict = input_data[2]
    print("parsing ", file_name)
    with open(file_name) as f:
        xml = f.read()
    if xml != "":
        tree = ET.fromstring(re.sub(r"(<\?xml[^>]+\?>)", r"<root>", xml) + "</root>")
        if len(tree) != 0:
            xml_root_dict[tid] = tree
    # print(tree.getchildren().tag)

def parse_input_files(directory):
    ### read all agent trace files
    tid_file_dict = get_all_files(directory)

    work_manager = workers.WorkerManager()
    xml_root_dict = dict()
    for tid in tid_file_dict:
        for file_name in tid_file_dict[tid]:
            work_manager.assignWork(thread_parse_input_file, [tid, file_name, xml_root_dict])
    work_manager.finish()
    return xml_root_dict

def load_method(method_root):
    method_manager = code_cache.MethodManager()
    for m_xml in method_root:
        m = code_cache.Method(m_xml.get("id"),m_xml.get("version"))
        ## set fields
        m.start_line = m_xml.get("start_line")
        m.file = m_xml.get("file")
        m.start_addr = m_xml.get("start_addr")
        m.code_size = m_xml.get("code_size")
        m.method_name = m_xml.get("name")
        m.class_name = m_xml.get("class")

        ## add children; currently addr2line mapping and bci2line mapping
        addr2line_xml = None
        bci2line_xml = None
        for c_xml in m_xml:
            if c_xml.get("type") == "addr2line":
                assert(not addr2line_xml)
                addr2line_xml = c_xml
            elif c_xml.get("type") == "bci2line":
                assert(not bci2line_xml)
                bci2line_xml = c_xml
        if addr2line_xml:
            for range_xml in addr2line_xml:
                assert(range_xml.tag == "range")
                start = range_xml.get("start")
                end = range_xml.get("end")
                lineno = range_xml.get("data")

                m.addAddr2Line(start,end,lineno)

        if bci2line_xml:
            for range_xml in bci2line_xml:
                assert(range_xml.tag == "range")
                start = range_xml.get("start")
                end = range_xml.get("end")
                lineno = range_xml.get("data")

                m.addBCI2Line(start,end,lineno)

        method_manager.addMethod(m)
    return method_manager

def load_context(context_root):
    context_manager = context.ContextManager()
    # print("It has ", len(context_root), " contexts")
    for ctxt_xml in context_root :
        ctxt = context.Context(ctxt_xml.get("id"))
        # set fields
        ctxt.method_version = ctxt_xml.get("method_version")
        ctxt.binary_addr = ctxt_xml.get("binary_addr")
        ctxt.numa_node = ctxt_xml.get("numa_node")
        ctxt.method_id = ctxt_xml.get("method_id")
        ctxt.bci = ctxt_xml.get("bci")
        ctxt.setParentID(ctxt_xml.get("parent_id"))

        metrics_xml = None
        for c_xml in ctxt_xml:
            if c_xml.tag == "metrics":
                assert(not metrics_xml)
                metrics_xml = c_xml
        if metrics_xml:
            for c_xml in metrics_xml:
                id = c_xml.get("id")
                if isDataCentric:
                    if id == "0":
                        ctxt.metrics_dict["value"] = c_xml.get("value1")
                        ctxt.metrics_type = "ALLOCTIMES"
                    if id == "1":
                        ctxt.metrics_dict["value"] = c_xml.get("value1")
                        ctxt.metrics_type = "L1CACHEMISSES"
                elif isObjLevel:
                    if id == "1":
                            ctxt.metrics_dict["equality"] = c_xml.get("value1")
                            ctxt.metrics_type = "ALWAYS_EQUAL"
                    if id == "2":
                            ctxt.metrics_dict["inequality"] = c_xml.get("value1")
                            # if ctxt.metrics_dict["equality"]:
                            #     ctxt.metrics_type = "EQUAL_AND_INEQUAL"
                            # else:
                            #     ctxt.metrics_type = "ALWAYS_INEQUAL"
                elif isNuma:
                    if id == "1":
                        ctxt.metrics_dict["equality"] = c_xml.get("value1")
                        ctxt.metrics_type = "ALWAYS_EQUAL"
                    if id == "2":
                        ctxt.metrics_dict["inequality"] = c_xml.get("value1")
                        if "equality" in ctxt.metrics_dict:
                            ctxt.metrics_type = "EQUAL_AND_INEQUAL"
                        else:
                            ctxt.metrics_type = "ALWAYS_INEQUAL"
                else:
                    if c_xml.get("value2") == "-1":
                        ctxt.metrics_dict["value"] = c_xml.get("value1")
                        ctxt.metrics_type = "INT"
                    if c_xml.get("value1") == "-1":
                        ctxt.metrics_dict["value"] = c_xml.get["value2"]
                        ctxt.metrics_type = "FP"

        ## add it to context manager
        context_manager.addContext(ctxt)
    roots = context_manager.getRoots()
    # print("remaining roots: ", str([r.id for r in roots]))
    assert(len(roots) == 1)
    context_manager.getRoots()
    context_manager.populateMetrics()
    return context_manager

def thread_load_method(input_data):
    manager_dict = input_data[0]
    method_root = input_data[1]
    print("load methods")
    manager_dict["method"] = load_method(method_root)

def thread_load_context(input_data):
    manager_dict = input_data[0]
    tid = input_data[1]
    context_root = input_data[2]
    # print("Reconstructing contexts from TID " + tid)
    print("load context TID " + tid)
    manager_dict[tid] = load_context(context_root)
    # print("Dumping contexts from TID "+tid)

def init_manager_dict(xml_root_dict):
    manager_dict = dict()

    work_manager = workers.WorkerManager()
    for tid in xml_root_dict:
        if tid == "method":
            work_manager.assignWork(thread_load_method, [manager_dict, xml_root_dict[tid]])
        else:
            work_manager.assignWork(thread_load_context, [manager_dict, tid, xml_root_dict[tid]])
    work_manager.finish()
    return manager_dict

def output_to_file(method_manager, context_manager, dump_data, dump_data2):
    intpr = interpreter.Interpreter(method_manager, context_manager)
    if isDataCentric:
        accessed = dict()
        for ctxt_list in context_manager.getAllPaths("0", "root-leaf"):#"root-subnode"):
            i = 0
            while i < len(ctxt_list):
                if ctxt_list[i].metrics_dict:
                    key = "\n".join(intpr.getSrcPosition(c) for c in ctxt_list[:i])
                    if ctxt_list[i].metrics_type == "ALLOCTIMES" and (key in accessed) == False:
                        accessed[key] = True
                        if key in dump_data:
                            dump_data[key] += (ctxt_list[i].metrics_dict["value"])
                        else:
                            dump_data[key] = (ctxt_list[i].metrics_dict["value"])
                    elif ctxt_list[i].metrics_type == "L1CACHEMISSES":
                        if key in dump_data2:
                            dump_data2[key] += (ctxt_list[i].metrics_dict["value"])
                        else:
                            dump_data2[key] = (ctxt_list[i].metrics_dict["value"])
                i += 1
    elif isObjLevel:
        for ctxt_list in context_manager.getAllPaths("0", "root-leaf"):#"root-subnode"):
            if ctxt_list[-1].metrics_dict:
                key = "\n".join(intpr.getSrcPosition(c) for c in ctxt_list[:-1])
                if ctxt_list[-1].metrics_type == "ALWAYS_EQUAL":
                    if key in dump_data:
                        dump_data[key] += (ctxt_list[-1].metrics_dict["equality"])
                    else:
                        dump_data[key] = (ctxt_list[-1].metrics_dict["equality"])
                elif ctxt_list[-1].metrics_type == "ALWAYS_INEQUAL":
                    if key in dump_data:
                        dump_data2[key] += (ctxt_list[-1].metrics_dict["inequality"])
                    else:
                        dump_data2[key] = (ctxt_list[-1].metrics_dict["inequality"])
                else :
                    if key in dump_data:
                        dump_data[key] += (ctxt_list[-1].metrics_dict["equality"])
                    # else:
                    #     dump_data[key] = (ctxt_list[-1].metrics_dict["equality"])
                    if key in dump_data:
                        dump_data2[key] += (ctxt_list[-1].metrics_dict["inequality"])
                    else:
                        dump_data2[key] = (ctxt_list[-1].metrics_dict["inequality"])
    elif isNuma:
        for ctxt_list in context_manager.getAllPaths("0", "root-leaf"):#"root-subnode"):
            if ctxt_list[-1].metrics_dict:
                key = "\n".join(intpr.getSrcPosition(c) for c in ctxt_list[:-1])
                if ctxt_list[-1].metrics_type == "ALWAYS_EQUAL":
                    if key in dump_data:
                        dump_data[key] += (ctxt_list[-1].metrics_dict["equality"])
                    else:
                        dump_data[key] = (ctxt_list[-1].metrics_dict["equality"])
                elif ctxt_list[-1].metrics_type == "ALWAYS_INEQUAL":
                    if key in dump_data2:
                        dump_data2[key] += (ctxt_list[-1].metrics_dict["inequality"])
                    else:
                        dump_data2[key] = (ctxt_list[-1].metrics_dict["inequality"])
                else :
                    if key in dump_data:
                        dump_data[key] += (ctxt_list[-1].metrics_dict["equality"])
                    else:
                        dump_data[key] = (ctxt_list[-1].metrics_dict["equality"])
                    if key in dump_data2:
                        dump_data2[key] += (ctxt_list[-1].metrics_dict["inequality"])
                    else:
                        dump_data2[key] = (ctxt_list[-1].metrics_dict["inequality"])

    else:
        for ctxt_list in context_manager.getAllPaths("0", "root-leaf"):#"root-subnode"):
            if ctxt_list[-1].metrics_dict:
                key = "\n".join(intpr.getSrcPosition(c) for c in ctxt_list[:-1])
                if ctxt_list[-1].metrics_type == "INT":
                    if key in dump_data:
                        dump_data[key] += (ctxt_list[-1].metrics_dict["value"])
                    else:
                        dump_data[key] = (ctxt_list[-1].metrics_dict["value"])
                elif ctxt_list[-1].metrics_type == "FP":
                    if key in dump_data2:
                        dump_data2[key] += (ctxt_list[-1].metrics_dict["value"])
                    else:
                        dump_data2[key] = (ctxt_list[-1].metrics_dict["value"])

def main():
    file = open("agent-statistics.run", "r")
    result = file.read().splitlines()
    file.close()

    global isDataCentric
    global isNuma
    global isGeneric
    global isHeap
    global isAllocTimes
    global isReuseDistance
    global isObjLevel

    if result[0] == 'DATACENTRIC':
        isDataCentric = True
        result = result[1:]
    elif result[0] == 'NUMA':
        isNuma = True
        result = result[1:]
    elif result[0] == 'GENERIC':
        isGeneric = True
        result = result[1:]
    elif result[0] == 'HEAP':
        isHeap = True
    elif result[0] == 'ALLOCTIMES':
        isAllocTimes = True
        result = result[1:]
    elif result[0] == 'REUSEDISTANCE':
        isReuseDistance = True
        result = result[1:]
    elif result[0] == 'OBJLEVEL':
        isObjLevel = True
        result = result[1:]

    ### read all agent trace files
    manager_dict = init_manager_dict(parse_input_files("."))

    dump_data = dict()
    dump_data2 = dict()
    for tid in manager_dict:
        if tid == "method":
            continue
        output_to_file(manager_dict["method"], manager_dict[tid], dump_data, dump_data2)

    file = open("agent-data", "w")

    if result and isDataCentric == False and isNuma == False and isGeneric == False and isHeap == False and isAllocTimes == False and isReuseDistance == False and isObjLevel == False:
        assert(len(result) == 3 or len(result) == 4)
        deadOrRedBytes = int(result[1])

        if len(result) == 4 and float(result[2]) != 0.:
            file.write("-----------------------Precise Redundancy------------------------------\n")

        rows = sorted(list(dump_data.items()), key=lambda x: x[-1], reverse = True)
        for row in rows:
            file.write(row[0] + "\n\nFraction: " + str(round(float(row[-1]) * 100 / deadOrRedBytes, 2)) +"%\n")

        if len(result) == 4 and float(result[3]) != 0.:
            file.write("\n----------------------Approximate Redundancy---------------------------\n")

            rows = sorted(list(dump_data2.items()), key=lambda x: x[-1], reverse = True)
            for row in rows:
                file.write(row[0]  + "\n\nFraction: " +  str(round(float(row[-1]) * 100 / deadOrRedBytes, 2)) +"%\n")

        file.write("\nTotal Bytes: " + result[0])
        file.write("\nTotal Redundant Bytes: " + result[1])
        if len(result) == 4:
            file.write("\nTotal Redundancy Fraction: " + str(round((float(result[2]) + float(result[3])) * 100, 2)) + "%")
        else:
            file.write("\nTotal Redundancy Fraction: " + str(round(float(result[2]) * 100, 2)) + "%")
    elif result and isDataCentric == True:
        assert(len(result) == 2)
        allocTimes = int(result[0])
        l1CacheMisses = int(result[1])
        if allocTimes != 0:
            file.write("-----------------------Allocation Times------------------------------\n")

            rows = sorted(list(dump_data.items()), key=lambda x: x[-1], reverse = True)
            for row in rows:
                file.write(row[0] + "\n\nFraction: " + str(round(float(row[-1]) * 100 / allocTimes, 2)) +"%\n")

        if l1CacheMisses != 0:
            file.write("\n-----------------------L1 Cache Misses------------------------------\n")

            rows = sorted(list(dump_data2.items()), key=lambda x: x[-1], reverse = True)
            for row in rows:
                file.write(row[0]  + "\nFraction: " +  str(round(float(row[-1]) * 100 / l1CacheMisses, 2)) +"%\n")
        file.write("\nTotal Allocation Times: " + result[0])
        file.write("\nTotal L1 Cache Misses: " + result[1])
    elif result and isNuma == True:
        assert(len(result) == 2)
        totalEqualityTimes = int(result[0])
        totalInequalityMismatches = int(result[1])
        if (totalInequalityMismatches != 0):
            rows = sorted(list(dump_data.items()), key=lambda x: x[-1], reverse = True)
            for row in rows:
                inequalityTimes = row[-1]
                equalityTimes = 0
                if row[0] in dump_data2:
                    equalityTimes = dump_data2[row[0]]
                file.write(row[0] + "\n\nFraction of Mismatch: " + str(round(float(inequalityTimes) * 100 / totalInequalityMismatches, 2)) + "%;" + " Match Times: " + str(equalityTimes) + " Mismatch Times: " + str(inequalityTimes) + " Match Percentage: " + str(round(float(equalityTimes) * 100 / (equalityTimes + inequalityTimes), 2)) + "%;" + " Mismatch Percentage: " + str(round(float(inequalityTimes) * 100 / (equalityTimes + inequalityTimes), 2)) + "%\n")
        file.write("\nTotal Match Times: " + result[0])
        file.write("\nTotal Mismatch Times: " + result[1])
    elif isGeneric == True:
        file.write("-----------------------Generic Counter------------------------------\n")

        rows = sorted(list(dump_data.items()), key=lambda x: x[-1], reverse = True)

        for row in rows:
            if row[0] != "":
                file.write(row[0] + "\n\nGeneric Counter: " + str(float(row[-1])) +"\n")
        file.write("\nTotal Generic Counter: " + result[0])
    elif isHeap == True:
        file.write("-----------------------Heap Analysis------------------------------\n")

        rows = sorted(list(dump_data.items()), key=lambda x: x[-1], reverse = True)

        for row in rows:
            if row[0] != "":
                file.write(row[0] + "\n\nObject allocation size: " + str(row[-1]) +"bytes\n")
    elif isAllocTimes == True:
        file.write("-----------------------Allocation Times Analysis------------------------------\n")

        rows = sorted(list(dump_data.items()), key=lambda x: x[-1], reverse = True)

        for row in rows:
            if row[0] != "":
                file.write(row[0] + "\n\nAllocation Times: " + str(row[-1]) +"\n")
        file.write("\nTotal Allocation Times: " + result[0])
    elif isReuseDistance == True:
        file.write("-----------------------Reuse Distance Analysis------------------------------\n")

        rows = sorted(list(dump_data.items()), key=lambda x: x[-1], reverse = True)

        for row in rows:
            if row[0] != "" and row[0][-1] != "*" and row[-1] > 0:
                file.write(row[0] + "\n\nReuse Distance: " + str(row[-1]) +"\n")
        file.write("\nTotal Memory Access Times: " + result[0])
    elif result and isObjLevel == True:
        assert(len(result) == 2)
        totalEqualityTimes = int(result[0])
        totalInequalityTimes = int(result[1])

        rows = sorted(dump_data2.items(), key=lambda x: x[-1], reverse = True)

        for row in rows:
            equalityTimes = row[-1]
            inequalityTimes = 0
            file.write(row[0] + "\n\nReplication Factor: " + str(float(equalityTimes) * 10000 / (totalEqualityTimes + totalInequalityTimes)) + "%\n")
        file.write("\nTotal Equality Times: " + result[0])
        file.write("\nTotal Inequality Times: " + result[1])


        # soot
        # assert(len(result) == 2)
        # totalEqualityTimes = int(result[0])
        # totalInequalityTimes = int(result[1])

        # for key,value in dump_data2.items():
        #     if key.find('PhaseOptions.java:84') != -1:
        #         value = 100000
        #         dump_data2[key] = value
        #         print(key + "\n\n" + str(value))


        # rows = sorted(dump_data2.items(), key=lambda x: x[-1], reverse = True)

        # for row in rows:
        #     equalityTimes = row[-1]
        #     inequalityTimes = 0
        #     if row[0].find('PhaseOptions.java:84') != -1:
        #         file.write(row[0] + "\n\n!!!Replication Factor: " + str(float(equalityTimes) * 1000 / (totalEqualityTimes + totalInequalityTimes)) + "%\n")
        #     else:
        #         file.write(row[0] + "\n\nReplication Factor: " + str(float(equalityTimes) * 100 / (totalEqualityTimes + totalInequalityTimes)) + "%\n")
        # file.write("\nTotal Equality Times: " + result[0])
        # file.write("\nTotal Inequality Times: " + result[1])

    file.close()

    print("Final dumping")

    # remove_all_files(".")

main()
