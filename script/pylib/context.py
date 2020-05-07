

class Context:
	def __init__(self, ctxt_id):
		self.id = ctxt_id
		self.method_version = None
		self.binary_addr = None
		self.numa_node = None
		self.method_id = None
		self.bci = None
		self.metrics_type = None
		self.metrics_dict = dict()
		
		self._parent_id = None
		self._children_id_set = set()
	def addChildID(self, c_id):
		self._children_id_set.add(c_id)
	def getChildrenIDs(self):
		return self._children_id_set
	def hasChildren(self):
		return len(self._children_id_set) > 0
	def setParentID(self, p_id):
		self._parent_id = p_id
	def getParentID(self):
		return self._Parent_id	


class ContextManager:
	def _id2context(self, context):
		if isinstance(context, Context):
			return context
		else: ## by id
			return self._context_dict[context]
	def _context2id(self, context):
		if isinstance(context, Context):
			return context.id
		else:
			return context

	def __init__(self):
		self._context_dict = dict()

		self._root_set = set() ##it is a place to save the context whose parent is not found
		self._parent_root_dict = dict() ## key: parent_id, val: [root_id] (it is a list since a parent may have several children); all the root_id must be identical to self._root_set

		self._metrics_field_set = set()
	def addContext(self, ctxt): 
		## we always follow its parent information instead of child information to locate the right place in the tree.
		## Then we update the child information accordingly for future easy reference
		assert(ctxt and ctxt._parent_id)
		assert(isinstance(ctxt, Context))
		if ctxt.id == "0": ## this is the dummpy root from the profiler
			if ctxt.id in self._context_dict:
				return 
#assert(ctxt.id not in self._context_dict)

		## add the context
		self._context_dict[ctxt.id] = ctxt

		## see whether it is a parent of any context in the root set
		if ctxt.id in self._parent_root_dict:
			for r_id in self._parent_root_dict[ctxt.id]:
				self._root_set.remove(r_id)
				ctxt.addChildID(r_id)
			del self._parent_root_dict[ctxt.id]			


		## find its parent
		if ctxt._parent_id in self._context_dict:
			self._context_dict[ctxt._parent_id].addChildID(ctxt.id)
		else: #parent not found, become a root temporarily
			self._root_set.add(ctxt.id)
			if ctxt._parent_id not in self._parent_root_dict:
				self._parent_root_dict[ctxt._parent_id] = []
			self._parent_root_dict[ctxt._parent_id].append(ctxt.id)

	def getContext(self, context_id):
		if context_id in self._context_dict:
			return self._context_dict[context_id]
		else:
			return None
	def getRoots(self):
		if len(self._context_dict) == 0:
			return None
		assert(len(self._root_set) >= 1)
		return [ self._context_dict[r_id] for r_id in self._root_set]


	def getParent(self, context):
		context = self._id2context(context)
		if context._parent_id in self._root_set:
			return None
		else:
			return self._context_dict[context._parent_id]

	def getChildren(self, context):
		context = self._id2context(context)
		return [ self._context_dict[c_id] for c_id in context.getChildrenIDs()]

	def isRoot(self, context):
		context_id = self._context2id(context)
		return context_id in self._root_set

	def isLeaf(self, context):
		context = self._id2context(context)
		return not context.hasChildren()
		
	def getAllPaths(self, root, description):
		## description: "root-leaf" | "root-subnode"
		root = self._id2context(root)
		assert(description in ["root-leaf", "root-subnode"])

		if root.hasChildren():
			ret_list = []
			for c_id in root.getChildrenIDs():
				all_paths = self.getAllPaths(c_id, description)
				all_paths = [ [root] + one_path for one_path in all_paths]
				ret_list += all_paths
			if description == "root-subnode": #the path contains the root to any node beneath
				ret_list += [[root]]
			return ret_list
		else:
			return [[root]]


	def populateMetrics(self):
		def _getMetrics(subtree_root):
			if len(subtree_root.getChildrenIDs()) == 0:
				for k in subtree_root.metrics_dict:
#					subtree_root.metrics_dict[k] = float(subtree_root.metrics_dict[k])
					subtree_root.metrics_dict[k] = int(subtree_root.metrics_dict[k])
				return subtree_root.metrics_dict
			subtree_root.metrics_dict = dict()
			for c_id in subtree_root.getChildrenIDs():
				c = self._context_dict[c_id]
				c_metrics_dict = _getMetrics(c)
				for k in c_metrics_dict:
					if k not in subtree_root.metrics_dict:
						subtree_root.metrics_dict[k] = 0
					subtree_root.metrics_dict[k] += c_metrics_dict[k]
			return subtree_root.metrics_dict
		root_metrics_dict = _getMetrics(self._id2context("0"))
		self._metrics_field_set = set(root_metrics_dict.keys())

	def getMetricsField(self):
		if len(self._metrics_field_set) == 0:
			return None ## should run populateMetrics() first
		else:
			return list(self._metrics_field_set)



