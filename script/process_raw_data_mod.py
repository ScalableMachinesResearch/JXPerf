#!/usr/bin/env python2

import os, sys
from pylib import *
from multiprocessing.dummy import Pool as ThreadPool
from functools import partial


##global variables

g_thread_context_dict = dict()
g_method_dict = dict()


def get_all_files(directory):
	files = [ f for f in os.listdir(directory) if os.path.isfile(os.path.join(directory,f))]
	ret_dict = dict()
	for f in files:
		if f.startswith("agent-trace-") and f.find(".run") >= 0:
			start_index = len("agent-trace-")
			end_index = f.find(".run")
			tid = f[start_index:end_index]
			if tid not in ret_dict:
				ret_dict[tid] = []
			ret_dict[tid].append(os.path.join(directory,f))
	return ret_dict	

def parse_input_file(file_path, level_one_node_tag):
	print "parsing", file_path
	with open(file_path) as f:
		contents = f.read()
		#print contents
	parser = special_xml.HomoXMLParser(level_one_node_tag, contents)
	return parser.getVirtualRoot()

def load_method(method_root):
	method_manager = code_cache.MethodManager()
	for m_xml in method_root.getChildren():
		m = code_cache.Method(m_xml.getAttr("id"),m_xml.getAttr("version"))
		## set fields
		m.file = m_xml.getAttr("file")
		m.start_addr = m_xml.getAttr("start_addr")
		m.code_size = m_xml.getAttr("code_size")
		m.method_name = m_xml.getAttr("name")
		m.class_name = m_xml.getAttr("class")

		## add children; currently addr2line mapping and bci2line mapping
		addr2line_xml = None
		bci2line_xml = None
		for c_xml in m_xml.getChildren():
			if c_xml.name() == "addr2line":
				assert(not addr2line_xml)
				addr2line_xml = c_xml
			elif c_xml.name() == "bci2line":
				assert(not bci2line_xml)
				bci2line_xml = c_xml
		if addr2line_xml:
			for range_xml in addr2line_xml.getChildren():
				assert(range_xml.name() == "range")
				start = range_xml.getAttr("start")
				end = range_xml.getAttr("end")
				lineno = range_xml.getAttr("data")	

				m.addAddr2Line((start,end),lineno)

		if bci2line_xml:
			for range_xml in bci2line_xml.getChildren():
				assert(range_xml.name() == "range")
				start = range_xml.getAttr("start")
				end = range_xml.getAttr("end")
				lineno = range_xml.getAttr("data")	

				m.addBCI2Line((start,end),lineno)
			
		method_manager.addMethod(m)
	return method_manager
def load_context(context_root):
	context_manager = context.ContextManager()
	print "It has ", len(context_root.getChildren()), " contexts"
	i = 0
	for ctxt_xml in context_root.getChildren():
		
		ctxt = context.Context(ctxt_xml.getAttr("id"))
		# set fields
		ctxt.method_version = ctxt_xml.getAttr("method_version")
		ctxt.binary_addr = ctxt_xml.getAttr("binary_addr")
		ctxt.method_id = ctxt_xml.getAttr("method_id")
		ctxt.bci = ctxt_xml.getAttr("bci")
		ctxt.setParentID(ctxt_xml.getAttr("parent_id"))
	    	
		metrics_xml = None
		for c_xml in ctxt_xml.getChildren():
			if c_xml.name() == "metrics":
				assert(not metrics_xml)
				metrics_xml = c_xml
		if metrics_xml:
			for c_xml in metrics_xml.getChildren():
				attr_dict = c_xml.getAttrDict()
#				id, value = attr_dict["id"], attr_dict["value"]
#				id, ctxt.metrics_type, value = attr_dict["id"], attr_dict["type"], attr_dict["value"]
				id = attr_dict["id"]
				if attr_dict.has_key("value1"):
				    ctxt.metrics_dict["non-trigger"] = attr_dict["value1"]
				else:
				    ctxt.metrics_dict["non-trigger"] = 0
				if attr_dict.has_key("value2"):
				    ctxt.metrics_dict["trigger"] = attr_dict["value2"]
				else:
				    ctxt.metrics_dict["trigger"] = 0
			#ctxt.metrics_dict = metrics_xml.getAttrDict()			
		
		## add it to context manager
		context_manager.addContext(ctxt)
	roots = context_manager.getRoots()
	print "remaining roots: ", str([r.id for r in roots])
	assert(len(roots) == 1)
	#len(context_manager.getRoots()) == 1)
	context_manager.populateMetrics()
	return context_manager

def output_to_file(tid, method_manager, context_manager, output):
	intpr = interpreter.Interpreter(method_manager, context_manager)
#	sorted_context_tree = sorted(context_manager.getAllPaths("0", "root-subnode"), key = lambda x:x[-1].metrics_dict)
	for ctxt_list in context_manager.getAllPaths("0", "root-leaf"):#"root-subnode"):
		if ctxt_list[-1].metrics_dict:
		    row_data = dict()
		    row_data["tid"] = tid
		    row_data["call-stack"] = "\n".join(intpr.getSrcPosition(c, True) for c in ctxt_list)
		
#		    row_data["metrics-type"] = ctxt_list[-1].metrics_type
		    metrics_dict = ctxt_list[-1].metrics_dict
		    row_data.update(metrics_dict)

		    output.writeRow(row_data)

#		    row_data["tid"] = tid
#		    row_data["call-stack.method_id"] = "/".join([c.method_id for c in ctxt_list[0:-1]])
#		    row_data["call-stack.src_pos"] = ":".join(intpr.getSrcPosition(c) for c in ctxt_list)
#		    row_data["leaf-node"] = str(context_manager.isLeaf(ctxt_list[-1]))
#		    row_data["IP"] = hex(int(ctxt_list[-1].binary_addr))
#		    row_data["IP.src_pos"] = intpr.getSrcPosition(ctxt_list[-1])
#	        row_data["metrics-type"] = ctxt_list[-1].metrics_type
#	        metrics_dict = ctxt_list[-1].metrics_dict	
#	        row_data.update(metrics_dict)	
#	        output.writeRow(row_data)	

def parallel_parse(tid_file_dict, xml_root_dict, tid):
	root = xml.XMLObj("root")
	if tid == "method":
		level_one_node_tag = "method"
	else:
		level_one_node_tag = "context"

	for f in tid_file_dict[tid]:
		new_root = parse_input_file(f, level_one_node_tag)
		root.addChildren(new_root.getChildren())
	if len(root.getChildren()) > 0:
		xml_root_dict[tid] = root


def main():
	### read all agent trace files
	tid_file_dict = get_all_files(".")

	### each file may have two kinds of information
	# 1. context; 2. code
	# the code information should be shared global while the context information is on a per-thread basis.
	xml_root_dict = dict()
#	pool = ThreadPool()
#	func = partial(parallel_parse, tid_file_dict, xml_root_dict)
#	pool.map(func, tid_file_dict)
#	pool.close()
#	pool.join()
	for tid in tid_file_dict:
		root = xml.XMLObj("root")
		if tid == "method":
			level_one_node_tag = "method"
		else:
			level_one_node_tag = "context"

		for f in tid_file_dict[tid]:
			new_root = parse_input_file(f, level_one_node_tag)
			root.addChildren(new_root.getChildren())
		if len(root.getChildren()) > 0:
			xml_root_dict[tid] = root
		
	### reconstruct method
	print("start to load methods")
	method_root = xml_root_dict["method"]
	method_manager = load_method(method_root)
	print("Finished loading methods")

	### reconstruct context trees
	print("Start to reconstruct contexts")
	context_manager_dict = dict()
	for tid in xml_root_dict:
		if tid == "method":
			continue
		print("Reconstructing contexts from TID " + tid)
		xml_root = xml_root_dict[tid]
		context_manager_dict[tid]  = load_context(xml_root)
	print("Finished reconstructing contexts")

	print("Start to output")
	### output to proper format
	## Let's output to csv file
	field = ["tid", "call-stack"]

	metrics_field = []
	for tid in context_manager_dict:
		tmp_metrics_field = context_manager_dict[tid].getMetricsField()
		if tmp_metrics_field:
			metrics_field += tmp_metrics_field

	metrics_field = list(set(metrics_field))

	output = writer.CSVWriter("agent-data",field + metrics_field) 
	for tid in context_manager_dict:
		print("Dumping information from TID "+tid)
		output_to_file(tid, method_manager, context_manager_dict[tid], output)
	print("Final dumping")
	output.finish()
main()
