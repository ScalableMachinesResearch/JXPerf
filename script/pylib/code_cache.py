import bintrees
import types
import copy
def str2int(s):
	if type(s) is int: ##it is already an int
		return s
	assert(type(s) is types.StringType)
	if s.startswith("0x"): ##hex
		s = s[2:]
		return int(s, 16)
	else: ##decimal
		return int(s)


class Method:
	def addRangeToTree(self, tree, (start,end), data):
		#[start,end] ##inclusive
		start = str2int(start)
		end = str2int(end)

		## overlapping check
		def isOverlap(start, end):
			if tree.is_empty():
				return False
			###check left item
			min_start, (min_end, _) = tree.min_item()
			if end < min_start:
				return False
			if start <= min_start: ## here end >= min_start
				return True
			left_start, (left_end, _) = tree.floor_item(start)
			if left_end >= start:
				return True
			## check right item
			max_start, (max_end, _) = tree.max_item()
			if start > max_end:
				return False
			if end >= max_start:
				return True
			right_start, (right_end, _) = tree.ceiling_item(start)
			if end >= right_start:
				return True
			return False

		assert(not isOverlap(start,end))
		tree[start] = (end, data)

	def queryTree(self, tree, proxy_key):
		if tree.is_empty():
			return None, None, None
		if proxy_key < tree.min_key():
			return None, None, None
		left_start, (left_end, data) = tree.floor_item(proxy_key)
		if left_end >= proxy_key:
			return left_start, left_end, data
		else:
			return None, None, None

	def __init__(self, method_id, version):
		self.method_id = method_id
		self.version = version
		self.file = None
		self.start_addr = None
		self.code_size = None
		self.method_name = None
		self.class_name = None
		self._addr2line_tree = bintrees.FastRBTree()
		self._bci2line_tree = bintrees.FastRBTree()
	def addAddr2Line(self, (start,end), line_no):
		# [start,end] ##inclusive
		self.addRangeToTree(self._addr2line_tree, (start,end), line_no)

	def addr2line(self, addr):
		addr = str2int(addr)
		start, end, lineno = self.queryTree(self._addr2line_tree, addr)
		return lineno

	def addBCI2Line(self, (start,end), line_no):
		self.addRangeToTree(self._bci2line_tree, (start,end), line_no)
	def bci2line(self, bci):
		bci = str2int(bci)
		start, end, lineno = self.queryTree(self._bci2line_tree, bci)
		return lineno


class MethodManager:
	def __init__(self):
		self._method_dict = dict()

	def addMethod(self, method):
		assert(isinstance(method, Method))
		key = method.method_id + "#"+ method.version
		if key in self._method_dict:
			print key, "already shown before"
#			assert(key not in self._method_dict)
			return
		self._method_dict[key] = method
		#print("method_count = "+str(len(self._method_dict)))
	def getMethod(self, method_id, version):
		key = method_id + "#" + version
		if key in self._method_dict:
			return self._method_dict[key]
		if version != "0":
			return None
		key = method_id + "#1"
		if key not in self._method_dict:
			return None

		candidate_method = self._method_dict[key]
		new_method = copy.deepcopy(candidate_method)
		new_method.version = "0"
		new_method.start_addr = "0"
		new_method.code_size = "0"
		new_method._addr2line_tree = bintrees.FastRBTree()
		
		self.addMethod(new_method)
		return new_method


