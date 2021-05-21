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
            xml_root_dict[tid] = tree;
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

def output_to_buff(method_manager, context_manager):
        intpr = interpreter.Interpreter(method_manager, context_manager)
        rtraces = context_manager.getAllRtrace("0")
        print(len(rtraces))

        profile = profile_pb2.Profile()

        sample_type = profile.sample_type.add()
        profile.string_table.append("")
        profile.string_table.append("type1")
        sample_type.type = len(profile.string_table) - 1
        profile.string_table.append("unit1")
        sample_type.unit = len(profile.string_table) - 1

        sample_type = profile.sample_type.add()
        profile.string_table.append("")
        profile.string_table.append("type2")
        sample_type.type = len(profile.string_table) - 1
        profile.string_table.append("unit2")
        sample_type.unit = len(profile.string_table) - 1

        location_id = 1
        function_id = 1
        for rtrace in rtraces:
            location = profile.location.add()
            location.id = location_id

            sample = profile.sample.add()
            sample.location_id.append(location_id)
            sample.value.append(1)
            sample.value.append(1)
            location_id += 1
            
            print(len(rtrace))
            for trace_node in rtrace:
                if trace_node.id != 0:
                    key = intpr.getInterpreter_Context(trace_node)
                    print(key.ctype)
                    if key.ctype == 0:
                        print("root")
                    elif key.ctype == 1:
                        if key.source_lineno == "??":
                            key.source_lineno = "-1"
                        if key.method_start_line == "??":
                            key.method_start_line = "-1"
                        function = profile.function.add()
                        function.id = function_id
                        # profile.string_table.append(key.method_name)
                        profile.string_table.append(key.class_name + "." + key.method_name + ":" + str(key.source_lineno))
                        function.name = len(profile.string_table) - 1
                        sample.value[0] = 10
                        sample.value[1] = 1000

                        # profile.string_table.append("/Users/dolan/Desktop/test/gui/ObjectLayout/ObjectLayout/src/main/java/"+ key.source_file)
                        profile.string_table.append(key.class_name)
                        function.filename = len(profile.string_table) - 1
                        function.start_line = int(key.method_start_line)

                        line = location.line.add()
                        line.function_id = function_id
                        line.line = int(key.source_lineno)

                        function_id += 1
                        print("class_name:",key.class_name)
                        print("method_name:",key.method_name)
                        print("source_file:",key.source_file)
                        print("source_lineno:",key.source_lineno)
                    else:
                        print("break")
                    
                    print("-----------------")
            
        f = open("jxperf.pprof", "wb")
        f.write(profile.SerializeToString())
        f.close()
        # for ctxt_list in context_manager.getAllPaths("0", "root-leaf"):#"root-subnode"):
        #     if ctxt_list[-1].metrics_dict:
        #         key = "\n".join(intpr.getSrcPosition(c) for c in ctxt_list[:-1])
        #         print(key)
                # if ctxt_list[-1].metrics_type == "INT":
                #     if key in dump_data:
                #         dump_data[key] += (ctxt_list[-1].metrics_dict["value"])
                #     else:
                #         dump_data[key] = (ctxt_list[-1].metrics_dict["value"])
                # elif ctxt_list[-1].metrics_type == "FP":
                #     if key in dump_data2:
                #         dump_data2[key] += (ctxt_list[-1].metrics_dict["value"])
                #     else:
                #         dump_data2[key] = (ctxt_list[-1].metrics_dict["value"])

def main():
    file = open("agent-statistics.run", "r")
    result = file.read().splitlines()
    file.close()

    global isDataCentric
    global isNuma
    global isGeneric
    global isHeap
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

    ### read all agent trace files
    manager_dict = init_manager_dict(parse_input_files("."))

    for tid in manager_dict:
        if tid == "method":
            continue
        output_to_buff(manager_dict["method"], manager_dict[tid])

    # remove_all_files(".")

main()
