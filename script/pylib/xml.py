
import re
import workers

##The XML file does not support content.. All the data must be included in attributes. Of course, you still can have children
##NOTES:
## double qoutes: must be used to protect values, it should not show in any content
## single quote: I don't care. it behaves as the normal character

class XMLObj:
	def __init__(self, name):
		self._name = name
		self._attr = dict()
		self._children = []
	def name(self):
		return self._name
	def getAttrDict(self):
		return self._attr
	def getAttr(self, key):
		return self._attr[key]
	def setAttr(self, key, val):
		self._attr[key] = val
	def addChild(self, c):
		assert(isinstance(c, XMLObj))
		self._children.append(c)
	def addChildren(self, c_list):
		assert(isinstance(c_list, list))
		self._children += c_list	
	def getChildren(self):
		return self._children

class XMLParser:

	def _getNextSegment(self, input_str):
		## it can be None, open tag or close tag
		input_str = input_str.strip().rstrip()
		if len(input_str) == 0:
			return None, None, None
		assert(input_str[0] == "<")
		if input_str[1] == "/":
			seg_type = "close"
		else:
			seg_type = "open"

		match_obj = self._reg_pattern["tag_end"].search(input_str)
		assert(match_obj)
		close_bracket_index, _ = match_obj.span()
		return seg_type, input_str[0:close_bracket_index+1], input_str[close_bracket_index+1:]

	def _parseOpenTag(self, open_tag_str):
		open_tag_str = open_tag_str[1:-1] #strip < and >
		attrs = self._reg_pattern["split_attr"].split(open_tag_str)[1::2]
		assert(len(attrs) > 0) # there must be a name
		tag_name = attrs[0]
		attr_dict = dict()
		for i in range(1, len(attrs)):
			key, val = attrs[i].split("=")
			attr_dict[key] = val.strip().rstrip()[1:-1] ##strip the quotes
		return (tag_name, attr_dict)



	def __init__(self, file_str):
		self._reg_pattern = dict()
		self._reg_pattern["split_attr"] = re.compile(r'''((?:[^ "']|"[^"]*"|'[^']*')+)''')  ## it is used to split on any space outside quotation
		self._reg_pattern["tag_end"] = re.compile(r'>(?=(?:[^\"]*\"[^\"]*\")*[^\"]*$)') ## it can search for ">" outside double quotation

		#strip the header if any
		i1 = file_str.find("<?")
		i2 = file_str.find("?>", i1+1)
		if i1 >= 0 and i2 > i1:
			file_str = file_str[i2+2:]
		
		self._content = file_str
		
		remains = file_str
		seg_type, tag_seg  ,remains =  self._getNextSegment(remains)
		
		self._root = XMLObj("root")
		node_stack = [self._root]

		while seg_type:
			if seg_type == "open":
				tag_name, attr_dict = self._parseOpenTag(tag_seg)
				new_obj = XMLObj(tag_name)
				for k in attr_dict:
					new_obj.setAttr(k, attr_dict[k])
				node_stack[-1].addChild(new_obj)
				node_stack.append(new_obj)
			elif seg_type == "close":
				assert(node_stack[-1].name() == tag_seg[2:-1])
				##the open tag should match the close tag
				node_stack.pop()
			else:
				assert(False)
			
			seg_type, tag_seg  ,remains =  self._getNextSegment(remains)
	def getVirtualRoot(self):
		return self._root
