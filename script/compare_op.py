#!/usr/bin/env python3

import os
import sys
import json

output_root = sys.argv[1]
ori_root = sys.argv[2]
new_root = sys.argv[3]

def get_json(file_name):
    data = {}
    with open(file_name, 'r') as f:
        data = json.load(f)
    return data

def is_child_name_equal(node1, node2):
    # print(node1["n"] + ".." + str(node2["n"]))
    for child1 in node1["c"]:
        for child2 in node2["c"]:
            if is_name_equal(child1, child2):
                # print("equal")
                return True
    return False


def is_name_equal(node1, node2):
    if node1["n"] == node2["n"]:
        return True
    elif node1["n"] == "??.??:0" or node2["n"] == "??.??:0":
        if is_child_name_equal(node1, node2):
            if node1["n"] == "??.??:0":
                node1["n"] = node2["n"]
            else:
                node2["n"] = node1["n"]
            return True
        return False
    else:
        return False

# def recursion_merge_list(node_list, ctxt_map):
#     new_list = []
#     for node1 in node_list:
#         same_node = None
#         for node2 in new_list:
#             if is_name_equal(node1, node2):
#                 same_node = node2
#                 break
#         if same_node != None:
#             same_node["f"] = 0
#             new_value = ctxt_map[same_node["ctxt_hndl"]]["value"]
#             ori_value = ctxt_map[node1["ctxt_hndl"]]["value"]
#             if new_value < ori_value:
#                 ctxt_map[node1["ctxt_hndl"]]["value"] = ori_value - new_value
#                 node1["v"] = ori_value - new_value
#                 for child_node in node1["c"]:
#                     same_node["c"].append(child_node)
#                 node1["c"] = []
#                 node1["f"] = -2
#                 same_node["cn"] = node1["ctxt_hndl"]
#                 new_list.append(node1)
#             else:
#                 for child_node in node1["c"]:
#                     same_node["c"].append(child_node)
#         else:
#             new_list.append(node1)
#     node_list.clear()
#     for node in new_list:
#         recursion_merge_list(node["c"], ctxt_map)
#         node_list.append(node)

def recursion_merge_list(node_list, ctxt_map):
    new_list = []
    for node1 in node_list:
        same_node = None
        for node2 in new_list:
            if is_name_equal(node1, node2):
                same_node = node2
                break
        if same_node != None:
            for child_node in node1["c"]:
                same_node["c"].append(child_node)
            new_value = ctxt_map[same_node["ctxt_hndl"]]["value"]
            ori_value = ctxt_map[node1["ctxt_hndl"]]["value"]
            if new_value < ori_value:
                ctxt_map[same_node["ctxt_hndl"]]["value"] = ori_value - new_value
                same_node["f"] = -1
            elif new_value > ori_value:
                ctxt_map[same_node["ctxt_hndl"]]["value"] = new_value - ori_value
                same_node["f"] = 1
            else:
                ctxt_map[same_node["ctxt_hndl"]]["value"] = 0
                same_node["f"] = 0
            same_node["cn"] = node1["ctxt_hndl"]
            ctxt_map.pop(node1["ctxt_hndl"])
        else:
            new_list.append(node1)
    node_list.clear()
    for node in new_list:
        recursion_merge_list(node["c"], ctxt_map)
        node_list.append(node)


# def order_tree(root, ctxt_map):
#     root["c"] = sorted(root["c"], key=lambda x: x["n"])
#     for node in root["c"]:
#         order_tree(node, ctxt_map)


# def recursion_update_flame_graph_value1(tree, ctxt_map):
#     if len(tree["c"]) != 0:
#         ctxt_map[tree["ctxt_hndl"]]["flame_graph_value"] = 0
#         for child_node in tree["c"]:
#             recursion_update_flame_graph_value1(child_node, ctxt_map)
#             ctxt_map[tree["ctxt_hndl"]]["flame_graph_value"] += ctxt_map[child_node["ctxt_hndl"]]["flame_graph_value"]
#     else:
#         ctxt_map[tree["ctxt_hndl"]]["flame_graph_value"] = ctxt_map[tree["ctxt_hndl"]]["value"]

def recursion_update_flame_graph_value(tree, ctxt_map):
    if len(tree["c"]) != 0:
        value = ctxt_map[tree["ctxt_hndl"]]["flame_graph_value"]
        global_value = 0
        for child_node in tree["c"]:
            global_value += ctxt_map[child_node["ctxt_hndl"]]["value"]
        for child_node in tree["c"]:
            if global_value > 0:
                ctxt_map[child_node["ctxt_hndl"]]["flame_graph_value"] = ctxt_map[child_node["ctxt_hndl"]]["value"] * value / global_value
            else:
                ctxt_map[child_node["ctxt_hndl"]]["flame_graph_value"] = 0
            recursion_update_flame_graph_value(child_node, ctxt_map)

# def recursion_update_flame_graph_value2(tree, ctxt_map): 
#     if len(tree["c"]) > 0:
#         value = 0
#         for child_node in tree["c"]:
#             recursion_update_flame_graph_value2(child_node, ctxt_map)
#             value += ctxt_map[child_node["ctxt_hndl"]]["flame_graph_value"]
#         ctxt_map[tree["ctxt_hndl"]]["flame_graph_value"] = value
#     if tree["cn"] != tree["ctxt_hndl"]:
#         ctxt_map[tree["cn"]]["flame_graph_value"] = ctxt_map[tree["ctxt_hndl"]]["flame_graph_value"] * ctxt_map[tree["cn"]]["value"] / ctxt_map[tree["ctxt_hndl"]]["value"]
    

def recursion_update_value(tree, ctxt_map):
    tree["v"] = ctxt_map[tree["ctxt_hndl"]]["flame_graph_value"]
    for child_node in tree["c"]:
        recursion_update_value(child_node, ctxt_map)

def recursion_update_name(tree):
    if tree["f"] == -2:
        tree["n"] = "[D] " + tree["n"]
    elif  tree["f"] == -1:
        tree["n"] = "[-] " + tree["n"]
    elif  tree["f"] == 1:
        tree["n"] = "[+] " + tree["n"]
    elif  tree["f"] == 2:
        tree["n"] = "[A] " + tree["n"]
    for child_node in tree["c"]:
        recursion_update_name(child_node)
    

def update_ctxt_map(new_map, ctxt_map, t):
    if t == -1:
        for ctxt in ctxt_map:
            ctxt_hndl_str = "ori-" +  ctxt
            new_map[ctxt_hndl_str] = ctxt_map[ctxt]
            new_map[ctxt_hndl_str]["file_path"] = ori_root + "/" + new_map[ctxt_hndl_str]["file_path"]
            new_map[ctxt_hndl_str]["flame_graph_value"] = new_map[ctxt_hndl_str]["value"]
    elif t == 1:
        for ctxt in ctxt_map:
            ctxt_hndl_str = "new-" +  ctxt
            new_map[ctxt_hndl_str] = ctxt_map[ctxt]
            new_map[ctxt_hndl_str]["file_path"] = new_root + "/" + new_map[ctxt_hndl_str]["file_path"]
            new_map[ctxt_hndl_str]["flame_graph_value"] = new_map[ctxt_hndl_str]["value"]
    

def update_tree(tree, t):
    if t == -2:
        tree["ctxt_hndl"] = "ori-" +  tree["ctxt_hndl"]
        tree["f"] = -2
    elif t == 2:
        tree["ctxt_hndl"] = "new-" +  tree["ctxt_hndl"]
        tree["f"] = 2
    tree["cn"] = tree["ctxt_hndl"]
    for child in tree["c"]:
        update_tree(child, t)
    
def get_big_value_in(ctxt_map, index):
    index = len(ctxt_map) - 1 if len(ctxt_map) <= index else index
    return sorted(list(ctxt_map.values()), key=lambda x: x["value"], reverse=True)[index]["value"]

def recursion_update_tree_value_by_field(root, ctxt_map):
    if len(root["c"]) != 0:
        root["v"] = 0
        for child_node in root["c"]:
            recursion_update_tree_value_by_field(child_node, ctxt_map)
            root["v"] += child_node["v"]
    else:
        if root["f"] < 0:
            root["v"] = - ctxt_map[root["ctxt_hndl"]]["flame_graph_value"]
        elif root["f"] > 0:
            root["v"] = ctxt_map[root["ctxt_hndl"]]["flame_graph_value"]
        else:
            root["v"] = 0

def recursion_order_field(root):
    root["c"] = sorted(root["c"], key = lambda x: x["v"], reverse=True)
    for child in root["c"]:
        recursion_order_field(child)

def order_tree_by_field(root, ctxt_map):
    recursion_update_tree_value_by_field(root, ctxt_map)
    recursion_order_field(root)

def get_simple_tree(root, ctxt_map, filter_value):
    new_children = []
    for child in root['c']:
        if ctxt_map[child["ctxt_hndl"]]["value"] > filter_value:
            new_children.append(child)
    root['c'] = new_children
    for child in root['c']:
        get_simple_tree(child, ctxt_map, filter_value)

def cout_tree_node(root):
    if root == None:
        return 0
    num = 1
    for child in root["c"]:
        num += cout_tree_node(child)
    return num

def main():
    ctxt_map = {}
    ctxt_map_ori = get_json(output_root + "/" + ori_root + '/.drcctprof/ctxt-map.json')
    update_ctxt_map(ctxt_map, ctxt_map_ori, -1)
    ctxt_map_new = get_json(output_root + "/" + new_root + '/.drcctprof/ctxt-map.json')
    update_ctxt_map(ctxt_map, ctxt_map_new, 1)

    ori_tree = get_json(output_root + "/" + ori_root + '/.drcctprof/flame-graph.json')
    update_tree(ori_tree, -2)
    new_tree = get_json(output_root + "/" + new_root + '/.drcctprof/flame-graph.json')
    update_tree(new_tree, 2)

    tree_root = new_tree
    for child in ori_tree["c"]:
        tree_root["c"].append(child)
    new_value = ctxt_map[new_tree["ctxt_hndl"]]["value"]
    ori_value = ctxt_map[ori_tree["ctxt_hndl"]]["value"]
    if new_value < ori_value:
        ctxt_map[new_tree["ctxt_hndl"]]["value"] = ori_value - new_value
        tree_root["f"] = -1
    elif new_value > ori_value:
        ctxt_map[new_tree["ctxt_hndl"]]["value"] = new_value - ori_value
        tree_root["f"] = 1
    else:
        ctxt_map[new_tree["ctxt_hndl"]]["value"] = 0
        tree_root["f"] = 0
    ctxt_map.pop(ori_tree["ctxt_hndl"])


    recursion_merge_list(tree_root["c"], ctxt_map)
    recursion_update_flame_graph_value(tree_root, ctxt_map)
    order_tree_by_field(tree_root, ctxt_map)
    recursion_update_value(tree_root, ctxt_map)
    # order_tree(tree_root, ctxt_map)
    print(cout_tree_node(tree_root))
    get_simple_tree(tree_root, ctxt_map, get_big_value_in(ctxt_map, 3000))
    print(cout_tree_node(tree_root))
    recursion_update_name(tree_root)

    # print(cout_tree_node(tree_root))
    # get_simple_tree(tree_root, get_big_value_in(ctxt_map, 3000))
    # print(cout_tree_node(tree_root))

    drdata_folder = output_root + "/.drcctprof-java";
    print(drdata_folder)

    if not os.path.exists(drdata_folder):
        os.makedirs(drdata_folder)
    with open(drdata_folder + '/ctxt-map.json', 'w') as fp:
        json.dump(ctxt_map, fp)
    
    with open(drdata_folder + '/flame-graph.json', 'w') as fp:
        json.dump(tree_root, fp)
    
    metrics = [
        {
            "Des": "CPU Cycles Different",
            "Type": 1
        }
    ]
    
    with open(drdata_folder + '/metrics.json', 'w') as fp:
        json.dump(metrics, fp)

main()
