#!/usr/bin/env python3

import os
import sys
from pylib import *
from multiprocessing.dummy import Pool as ThreadPool
#from functools import partial

import re
import xml.etree.ElementTree as ET

import json

output_root = sys.argv[1]

g_thread_context_dict = dict()
g_method_dict = dict()
g_file_map = dict()


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
                if id == "1":
                        ctxt.metrics_dict["inequality"] = c_xml.get("value1")
                        ctxt.metrics_dict["equality"] = 0
                        # ctxt.metrics_type = "ALWAYS_INEQUAL"
                if id == "2":
                        ctxt.metrics_dict["inequality"] = 0
                        ctxt.metrics_dict["equality"] = c_xml.get("value1")
                        # ctxt.metrics_dict["equality"] = 10000
                        # ctxt.metrics_type = "ALWAYS_EQUAL"

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

def get_file_path(file_name, class_name):
    package_name = class_name.rsplit(".", 1)[0]
    if package_name + ":" + file_name in g_file_map:
        return g_file_map[package_name + ":" + file_name]
    else:
        return file_name

def get_simple_tree(root, filter_value):
    new_children = []
    for child in root['c']:
        if child['v'] > filter_value:
            new_children.append(child)
    root['c'] = new_children
    for child in root['c']:
        get_simple_tree(child, filter_value)

def output_to_vscode(tid, method_manager, context_manager, ctxt_map, tree_node_map, totalEqualityTimes, totalInequalityTimes):
    thread_tree_root = None
    intpr = interpreter.Interpreter(method_manager, context_manager)
    rtraces = context_manager.getAllRtrace("0")
    for rtrace in rtraces:
        # print("len" + str(len(rtrace)))
        metrics_value = 0
        if len(rtrace) > 0:
            metrics_value = float(rtrace[0].metrics_dict["equality"]) * 10000 / (totalEqualityTimes + totalInequalityTimes)
        else:
            continue
        last_tree_item_id = "-1"
        for trace_node in rtrace:
            if trace_node.id != 0:
                key = intpr.getInterpreter_Context(trace_node)
                if key.ctype < 0 and len(rtrace) == 2 and key.ctype != -2:
                    break
                elif key.ctype < 0 and key.ctype != -2:
                    continue
                if key.ctype == -2:
                    ctxt_hndl_str = tid + "-root"
                else:
                    ctxt_hndl_str = tid + "-" + str(trace_node.id)

                if ctxt_hndl_str in ctxt_map:
                    ctxt_map[ctxt_hndl_str]["value"] += metrics_value
                    tree_node_map[ctxt_hndl_str]["v"] += metrics_value
                    if last_tree_item_id !=  "-1":
                        if tree_node_map[last_tree_item_id] not in tree_node_map[ctxt_hndl_str]["c"]:
                            # print(ctxt_hndl_str + " append "+ last_tree_item_id)
                            tree_node_map[ctxt_hndl_str]["c"].append(tree_node_map[last_tree_item_id])
                else:
                    if key.ctype == -2:
                        ctxt_map[ctxt_hndl_str] = {
                            "pc": " ",
                            "name": "Thread["+ tid + "]ROOT",
                            "file_path": " ",
                            "asm": " ",
                            "line_no": 0,
                            "value": metrics_value
                        }
                        tree_node_map[ctxt_hndl_str] = {
                            "ctxt_hndl": ctxt_hndl_str,
                            "n": "Thread["+ tid + "]ROOT",
                            "v": metrics_value,
                            "c": [],
                            "s": 0
                        }
                        thread_tree_root = tree_node_map[ctxt_hndl_str]
                    elif key.ctype == 1:
                        if key.source_lineno == "??":
                            key.source_lineno = "0"
                        line_no = int(key.source_lineno)
                        file_path = get_file_path(key.source_file, key.class_name)
                        ctxt_map[ctxt_hndl_str] = {
                            "pc": " ",
                            "name": key.class_name + "." + key.method_name + ":" + str(key.source_lineno),
                            "file_path": file_path,
                            "asm": " ",
                            "line_no": line_no,
                            "value": metrics_value
                        }
                        tree_node_map[ctxt_hndl_str] = {
                            "ctxt_hndl": ctxt_hndl_str,
                            "n": key.class_name + "." + key.method_name + ":" + str(key.source_lineno),
                            "v": metrics_value,
                            "c": [],
                            "s": 0
                        }
                    if last_tree_item_id !=  "-1":
                        # print(ctxt_hndl_str + " append "+ last_tree_item_id)
                        tree_node_map[ctxt_hndl_str]["c"].append(tree_node_map[last_tree_item_id])
                if key.ctype == -2:
                    break
                last_tree_item_id = ctxt_hndl_str
    return thread_tree_root

def markTree(root, array):
    s = 0
    if len(root["c"]) == 0 :
        for x in array:
            if root["ctxt_hndl"] == x:
                s = 1
                break
    for c in root["c"]:
        s += markTree(c, array)
    if s > 0:
        root["s"] = 1
    return root["s"]

def newTree(newroot, root):
    for c in root["c"]:
        if c["s"] == 1:
            newC = {
                "ctxt_hndl": c["ctxt_hndl"],
                "n": c["n"],
                "v": c["v"],
                "c": []
            }
            newTree(newC, c)
            newroot["c"].append(newC)

def updateTree(root):
    if len(root["c"]) != 0:
        root["v"] = 0 
    # else:
    #     root["v"] = root["v"] / 10
    for c in root["c"]:
        updateTree(c)
        root["v"] += c["v"]

def loopInEveryLeafNode(root, ctxt_map, array):
    array_size = 10
    if len(root["c"]) == 0:
        if len(array) == 0:
            array.append(root["ctxt_hndl"])
        elif len(array) < array_size:
            if ctxt_map[array[0]]['value'] > ctxt_map[root["ctxt_hndl"]]['value']:
                array.insert(0, root["ctxt_hndl"])
            else:
                array.append(root["ctxt_hndl"])
        elif root["v"] > ctxt_map[array[0]]['value']:
            array[0] = root["ctxt_hndl"]
            index = 0
            for i in range(1,array_size):
                # print(index)
                if ctxt_map[array[i]]['value'] < ctxt_map[array[index]]['value']:
                    index = i
            if index != 0:
                array[0] = array[index]
                array[index] = root["ctxt_hndl"]     
    for child in root["c"]:
        loopInEveryLeafNode(child, ctxt_map, array)


def recursion_merge_list(node_list, ctxt_map, tree_node_map):
    new_list = []
    for node1 in node_list:
        same_node = None
        for node2 in new_list:
            if node2["n"] == node1["n"]:
                same_node = node2
                break
        if same_node == None:
            new_list.append(node1)
        else:
            ctxt_map[same_node["ctxt_hndl"]]["value"] += node1["v"]
            same_node["v"] += node1["v"]
            for child_node in node1["c"]:
                same_node["c"].append(child_node)
            ctxt_map.pop(node1["ctxt_hndl"])
            tree_node_map.pop(node1["ctxt_hndl"])
    node_list.clear()
    for node in new_list:
        recursion_merge_list(node["c"], ctxt_map, tree_node_map)
        node_list.append(node)


def merge_tree_node(node1, node2, ctxt_map, tree_node_map):
    ctxt_map[node1["ctxt_hndl"]]["value"] += node2["v"]
    node1["v"] += node2["v"]
    for child_node in node2["c"]:
        node1["c"].append(child_node)
    ctxt_map.pop(node2["ctxt_hndl"])
    tree_node_map.pop(node2["ctxt_hndl"])

    recursion_merge_list(node1["c"], ctxt_map, tree_node_map)


def cout_tree_node(root):
    if root == None:
        return 0
    num = 1
    for child in root["c"]:
        num += cout_tree_node(child)
    return num

def update_sourcefile_map(file_name, filepath, type):
    # print(file_name)
    with open(filepath,'r', errors='replace') as file:
        content = file.readlines()
        for line in content:
            if line.startswith('package '):
                package_name = ""
                if type == "java":
                    package_name = line[8:-2]
                else :
                    package_name = line[8:-1]
                # print(package_name + ":" + file_name + " " + filepath)
                g_file_map[package_name + ":" + file_name] = filepath.replace(output_root+"/", "")
                break

def init_sourcefile_map(path):
    filelist = os.listdir(path)
    for filename in filelist:
        filepath = os.path.join(path, filename)
        if os.path.isdir(filepath):
            init_sourcefile_map(filepath)
        else:
            if os.path.splitext(filename)[-1][1:] == "java" or os.path.splitext(filename)[-1][1:] == "scala" :
                update_sourcefile_map(filename, filepath, os.path.splitext(filename)[-1][1:])



def main():
    init_sourcefile_map(output_root)
    file = open("agent-statistics.run", "r")
    result = file.read().splitlines()
    result = result[1:]
    assert(len(result) == 2)
    totalEqualityTimes = int(result[0])
    totalInequalityTimes = int(result[1])
    file.close()


    ### read all agent trace files
    manager_dict = init_manager_dict(parse_input_files("."))

    ctxt_map = {
        "process-root": {
            "pc": " ",
            "name": "ROOT",
            "file_path": " ",
            "asm": " ",
            "line_no": 0,
            "value": 0.00
        }
    }
    tree_node_map = {
        "process-root": {
            "ctxt_hndl": "process-root",
            "n": "ROOT",
            "v": 0.00,
            "c": [],
            "s": 1
        }
    }
    tree_root = tree_node_map["process-root"]
    for tid in manager_dict:
        if tid == "method":
            continue
        # print(tid)
        # output_to_buff(manager_dict["method"], manager_dict[tid])
        thread_tree_root = output_to_vscode(tid, manager_dict["method"], manager_dict[tid], ctxt_map, tree_node_map, totalEqualityTimes, totalInequalityTimes)

        if thread_tree_root:
            tree_root['v'] += thread_tree_root['v']
            tree_root['c'].append(thread_tree_root)
            # merge_tree_node(tree_root, thread_tree_root, ctxt_map, tree_node_map)

    showLeafNodes = []
    loopInEveryLeafNode(tree_root, ctxt_map, showLeafNodes)
    markTree(tree_root, showLeafNodes)
    newRoot = {
        "ctxt_hndl": "process-root",
        "n": "ROOT",
        "v": tree_root['v'],
        "c": []
    }
    newTree(newRoot, tree_root)
    updateTree(newRoot)

    # print(cout_tree_node(tree_root))
    # get_simple_tree(tree_root, tree_root['v']/1000)
    # print(cout_tree_node(tree_root))

    drdata_folder = output_root + "/.drcctprof";
    print(drdata_folder)

    if not os.path.exists(drdata_folder):
        os.makedirs(drdata_folder)
    with open(drdata_folder + '/ctxt-map.json', 'w') as fp:
        json.dump(ctxt_map, fp)

    with open(drdata_folder + '/flame-graph.json', 'w') as fp:
        json.dump(newRoot, fp)

    metrics = [
        {
            "Des": "Global CPU Cycles",
            "Type": 1
        }
    ]

    with open(drdata_folder + '/metrics.json', 'w') as fp:
        json.dump(metrics, fp)
    # remove_all_files(".")

main()
