
import re
import workers
import Queue
import xml
import threading
import time

class HomoXMLParser: 
	''' This XML file only contains only one type of Level-1 XML nodes.
	'''

	def _generateLevelOneNodeStr(self, input_str): ## procuder  ## can only be one thread
		input_str = input_str.strip().rstrip()		

		while len(input_str) > 0:
			match_obj = self._open_tag_pattern.search(input_str)
			assert(match_obj.span()[0] == 0) ## make sure we really start with the expected open tag
			match_obj = self._close_tag_pattern.search(input_str)
			_, ending_index = match_obj.span()
			assert(ending_index > 0)
			current_node_str = input_str[0:ending_index]
			#print "producer ", current_node_str	
			#print "producer"
			###put it into the queue
			self._level_one_str_queue.put(current_node_str)

			input_str = input_str[ending_index:].strip().rstrip()
		#print "producer ends bye"
		self._producer_work_done = True

	def _parseLevelOneNodeStr(self, dumpy_data): ##consumer ## can be multiple threads
		while not self._producer_work_done or not self._level_one_str_queue.empty(): 
			try:
				node_str = self._level_one_str_queue.get_nowait()
				parser = xml.XMLParser(node_str)
				children = parser.getVirtualRoot().getChildren()
				assert(len(children) == 1)
				#print "consumer"
				level_node_xml_obj = children[0]
				self._root_lock.acquire()
				self._root.addChild(level_node_xml_obj)
				self._root_lock.release()
			except Queue.Empty:
				## may sleep a little bit?
				time.sleep(0.001)			

		#print "consumer ends bye"

	def __init__(self, level_one_node, file_str):
		self._open_tag_pattern = re.compile(r'<'+level_one_node)  
		self._close_tag_pattern = re.compile(r'</'+level_one_node+'>') 

		self._level_one_str_queue = Queue.Queue()	
		self._producer_work_done = False

		#strip the header if any
		i1 = file_str.find("<?")
		i2 = file_str.find("?>", i1+1)
		if i1 >= 0 and i2 > i1:
			file_str = file_str[i2+2:]
		self._content = file_str
		
		
		self._root = xml.XMLObj("root")
		self._root_lock = threading.Lock()


		work_manager = workers.WorkerManager()
		work_manager.assignWork(self._generateLevelOneNodeStr, self._content)
		for i in range(0,10):
			work_manager.assignWork(self._parseLevelOneNodeStr, None)

		work_manager.finish()

	def getVirtualRoot(self):
		return self._root
