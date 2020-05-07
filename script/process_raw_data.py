#!/usr/bin/env python2

import os, sys
from pylib import *
from multiprocessing.dummy import Pool as ThreadPool
from functools import partial

##global variables
isDataCentric = False
isNuma = False

g_thread_context_dict = dict()
g_method_dict = dict()


def get_all_files(directory):
	files = [f for f in os.listdir(directory) if os.path.isfile(os.path.join(directory,f))]
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

def remove_all_files(directory):
	files = [f for f in os.listdir(directory) if os.path.isfile(os.path.join(directory,f))]
	for f in files:
		if f.startswith("agent-trace-") and f.find(".run") >= 0:
			os.remove(f)
		elif f.startswith("agent-statistics") and f.find(".run"):
			os.remove(f)

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
	for ctxt_xml in context_root.getChildren():

		ctxt = context.Context(ctxt_xml.getAttr("id"))
		# set fields
		ctxt.method_version = ctxt_xml.getAttr("method_version")
		ctxt.binary_addr = ctxt_xml.getAttr("binary_addr")
		ctxt.numa_node = ctxt_xml.getAttr("numa_node")
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
				id = attr_dict["id"]
				if isDataCentric:
					if id == "0" and attr_dict.has_key("value1"):
				    		ctxt.metrics_dict["value"] = attr_dict["value1"]
				    		ctxt.metrics_type = "ALLOCTIMES"
					if id == "1" and attr_dict.has_key("value1"):
				    		ctxt.metrics_dict["value"] = attr_dict["value1"]
				    		ctxt.metrics_type = "L1CACHEMISSES"
				elif isNuma:
					if id == "1" and attr_dict.has_key("value1"):
				    		ctxt.metrics_dict["equality"] = attr_dict["value1"]
				    		ctxt.metrics_type = "ALWAYS_EQUAL"
					if id == "2" and attr_dict.has_key("value1"):
				    		ctxt.metrics_dict["inequality"] = attr_dict["value1"]
						if ctxt.metrics_dict.has_key("equality"):
				    			ctxt.metrics_type = "EQUAL_AND_INEQUAL"
						else:
							ctxt.metrics_type = "ALWAYS_INEQUAL"
				else:
					if attr_dict.has_key("value1"):
				    		assert(not(attr_dict.has_key("value2")))
				    		ctxt.metrics_dict["value"] = attr_dict["value1"]
				    		ctxt.metrics_type = "INT"
					if attr_dict.has_key("value2"):
				    		assert(not(attr_dict.has_key("value1")))
				    		ctxt.metrics_dict["value"] = attr_dict["value2"]
				    		ctxt.metrics_type = "FP"

		## add it to context manager
		context_manager.addContext(ctxt)
	roots = context_manager.getRoots()
	print "remaining roots: ", str([r.id for r in roots])
	assert(len(roots) == 1)
	context_manager.getRoots()
	context_manager.populateMetrics()
	return context_manager

def output_to_file(method_manager, context_manager, dump_data, dump_data2):
	intpr = interpreter.Interpreter(method_manager, context_manager)
	if isDataCentric:
		accessed = dict()
		for ctxt_list in context_manager.getAllPaths("0", "root-leaf"):#"root-subnode"):
	 		i = 0
			while i < len(ctxt_list):
				if ctxt_list[i].metrics_dict:
					key = "\n".join(intpr.getSrcPosition(c, isDataCentric) for c in ctxt_list[:i])
					if ctxt_list[i].metrics_type == "ALLOCTIMES" and accessed.has_key(key) == False:
						accessed[key] = True
						if dump_data.has_key(key):
							dump_data[key] += (ctxt_list[i].metrics_dict["value"])
						else:
							dump_data[key] = (ctxt_list[i].metrics_dict["value"])
					elif ctxt_list[i].metrics_type == "L1CACHEMISSES":
						if dump_data2.has_key(key):
							dump_data2[key] += (ctxt_list[i].metrics_dict["value"])
						else:
							dump_data2[key] = (ctxt_list[i].metrics_dict["value"])
				i += 1
	elif isNuma:
		for ctxt_list in context_manager.getAllPaths("0", "root-leaf"):#"root-subnode"):
			if ctxt_list[-1].metrics_dict:
				key = "\n".join(intpr.getSrcPosition(c) for c in ctxt_list[:-1])
				if ctxt_list[-1].metrics_type == "ALWAYS_EQUAL":
					if dump_data.has_key(key):
						dump_data[key] += (ctxt_list[-1].metrics_dict["equality"])
					else:
						dump_data[key] = (ctxt_list[-1].metrics_dict["equality"])
				elif ctxt_list[-1].metrics_type == "ALWAYS_INEQUAL":
					if dump_data2.has_key(key):
						dump_data2[key] += (ctxt_list[-1].metrics_dict["inequality"])
					else:
						dump_data2[key] = (ctxt_list[-1].metrics_dict["inequality"])
				else :
					if dump_data.has_key(key):
						dump_data[key] += (ctxt_list[-1].metrics_dict["equality"])
					else:
						dump_data[key] = (ctxt_list[-1].metrics_dict["equality"])
					if dump_data2.has_key(key):
						dump_data2[key] += (ctxt_list[-1].metrics_dict["inequality"])
					else:
						dump_data2[key] = (ctxt_list[-1].metrics_dict["inequality"])

	else:
		for ctxt_list in context_manager.getAllPaths("0", "root-leaf"):#"root-subnode"):
			if ctxt_list[-1].metrics_dict:
				key = "\n".join(intpr.getSrcPosition(c) for c in ctxt_list[:-1])
				if ctxt_list[-1].metrics_type == "INT":
					if dump_data.has_key(key):
						dump_data[key] += (ctxt_list[-1].metrics_dict["value"])
					else:
						dump_data[key] = (ctxt_list[-1].metrics_dict["value"])
				elif ctxt_list[-1].metrics_type == "FP":
					if dump_data2.has_key(key):
						dump_data2[key] += (ctxt_list[-1].metrics_dict["value"])
					else:
						dump_data2[key] = (ctxt_list[-1].metrics_dict["value"])

def main():
	file = open("agent-statistics.run", "r")
	result = file.read().splitlines()
	file.close()

	global isDataCentric
	global isNuma
	if result[0] == 'DATACENTRIC':
		isDataCentric = True
		result = result[1:]
	elif result[0] == 'NUMA':
		isNuma = True
		result = result[1:]

	### read all agent trace files
	tid_file_dict = get_all_files(".")

	### each file may have two kinds of information
	# 1. context; 2. code
	# the code information should be shared global while the context information is on a per-thread basis.
	xml_root_dict = dict()
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

	print("Start to output")

	dump_data = dict()
	dump_data2 = dict()

	for tid in xml_root_dict:
		if tid == "method":
			continue
		print("Reconstructing contexts from TID " + tid)
		xml_root = xml_root_dict[tid]
		print("Dumping contexts from TID "+tid)
	 	output_to_file(method_manager, load_context(xml_root), dump_data, dump_data2)

	file = open("agent-data", "w")

	if result and isDataCentric == False and isNuma == False:
		assert(len(result) == 3 or len(result) == 4)
		deadOrRedBytes = long(result[1])

		if len(result) == 4 and float(result[2]) != 0.:
			file.write("-----------------------Precise Redundancy------------------------------\n")

		rows = sorted(dump_data.items(), key=lambda x: x[-1], reverse = True)
		for row in rows:
			file.write(row[0] + "\n\nFraction: " + str(round(float(row[-1]) * 100 / deadOrRedBytes, 2)) +"%\n")

		if len(result) == 4 and float(result[3]) != 0.:
			file.write("\n----------------------Approximate Redundancy---------------------------\n")

			rows = sorted(dump_data2.items(), key=lambda x: x[-1], reverse = True)
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
		allocTimes = long(result[0])
		l1CacheMisses = long(result[1])
		if allocTimes != 0:
			file.write("-----------------------Allocation Times------------------------------\n")

			rows = sorted(dump_data.items(), key=lambda x: x[-1], reverse = True)
			for row in rows:
				file.write(row[0] + "\n\nFraction: " + str(round(float(row[-1]) * 100 / allocTimes, 2)) +"%\n")

		if l1CacheMisses != 0:
			file.write("\n-----------------------L1 Cache Misses------------------------------\n")

			rows = sorted(dump_data2.items(), key=lambda x: x[-1], reverse = True)
			for row in rows:
				file.write(row[0]  + "\nFraction: " +  str(round(float(row[-1]) * 100 / l1CacheMisses, 2)) +"%\n")
		file.write("\nTotal Allocation Times: " + result[0])
		file.write("\nTotal L1 Cache Misses: " + result[1])
	
	elif result and isNuma == True:
		assert(len(result) == 2)
		totalEqualityTimes = long(result[0])
		totalInequalityMismatches = long(result[1])

		rows = sorted(dump_data.items(), key=lambda x: x[-1], reverse = True)

		for row in rows:
			inequalityTimes = row[-1]
			equalityTimes = 0
			if dump_data2.has_key(row[0]):
				equalityTimes = dump_data2[row[0]]	
			file.write(row[0] + "\n\nFraction of Mismatch: " + str(round(float(inequalityTimes) * 100 / totalInequalityMismatches, 2)) + "%;" + " Match Times: " + str(equalityTimes) + " Mismatch Times: " + str(inequalityTimes) + " Match Percentage: " + str(round(float(equalityTimes) * 100 / (equalityTimes + inequalityTimes), 2)) + "%;" + " Mismatch Percentage: " + str(round(float(inequalityTimes) * 100 / (equalityTimes + inequalityTimes), 2)) + "%\n")
		file.write("\nTotal Match Times: " + result[0])
		file.write("\nTotal Mismatch Times: " + result[1])

	file.close()

	print("Final dumping")

	remove_all_files(".")

main()
