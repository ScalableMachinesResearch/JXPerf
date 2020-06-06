import code_cache
import context


class Interpreter:
	def __init__(self, method_manager, context_manager):
		assert(isinstance(method_manager, code_cache.MethodManager))
		assert(isinstance(context_manager, context.ContextManager))
		self._method_manager = method_manager
		self._context_manager = context_manager

	def getSrcPosition(self, context, isDataCentric=False, isNuma=True):
		## include [method name], [source file] and [source lineno]
		## the source_file and source_lineno should indicate where the method is called.. I mean it is related to the parent
		
		ip = context.binary_addr
		node = context.numa_node
		if self._context_manager.isRoot(context):
			class_name, method_name, source_file, source_lineno, ip = "Root", None, None, None, None

		elif ip != "0": ## leaf node
			method = self._method_manager.getMethod(context.method_id, context.method_version)
			if method:
				class_name = method.class_name
				method_name = method.method_name
				source_file = method.file
				source_lineno = method.addr2line(context.binary_addr)
			else:
				class_name, method_name, source_file, source_lineno = None, None, None, None


		else: ## middle node
			method = self._method_manager.getMethod(context.method_id, context.method_version)
			if method:
				method_name = method.method_name
				class_name = method.class_name
				source_file = method.file
				source_lineno = method.bci2line(context.bci)
				ip = None
			else:
				method_name, class_name, source_file, source_lineno, ip = None, None, None, None, None

		if class_name == None or len(class_name) == 0:
			class_name = "??"

		if method_name == None or len(method_name) == 0:
			method_name = "??"

		if source_file == None or len(source_file) == 0:
			source_file = "??"

		if source_lineno == None or len(source_lineno) == 0:
			source_lineno = "??"
		if ip == None:
			ip = ""
		else:
			ip = hex(int(ip))
		if node == "10":
			node = ""


		if context.bci == "-65536" and isDataCentric:
			return "***********************Access to the object above***********************"
		elif context.bci == "-65536" and isNuma:
			return "***********************Access to the object above***********************"
		elif context.bci == "-65536":
			return "*********************************REDUNDANT WITH*********************************"
		elif class_name == "Root":
			return ""
		# elif ip != "":
			# return ""
		else:
			return class_name + "." + method_name +"(" + source_file +":" + source_lineno + " " + node + ")"
