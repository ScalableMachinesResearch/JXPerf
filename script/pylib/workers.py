
import threading
import time
import multiprocessing

class ThreadWorker(threading.Thread):
	def __init__(self, finish_cb):
		threading.Thread.__init__(self)
		self._work_routine = None
		self._input_data = None
		self._finish_cb = finish_cb
	def setWork(self,work_routine, input_data):
		self._work_routine = work_routine
		self._input_data = input_data
	def run(self):
		self._work_routine(self._input_data)
		self._finish_cb(self)
		
class WorkerManager:
	def _workerFinish(self, worker):
		self._lock.acquire()
		self._active_worker_list.remove(worker)
		self._lock.release()

	def __init__(self, max_workers = -1): ## the thread can only be started once. So forget the thread pool idea
		if max_workers <= 0:
			self._max_workers = multiprocessing.cpu_count() - 1 ## thinking of the master thread
		else:
			self._max_workers = max_workers
		self._active_worker_list = []
		self._lock = threading.Lock()

	def assignWork(self, work_routine, input_data):
		self._lock.acquire()
		if len(self._active_worker_list) >= self._max_workers:
			self._lock.release()
			## ok, the manager will do the job anyway
			work_routine(input_data)
			return "Master" 
	
		worker = ThreadWorker(self._workerFinish)
		self._active_worker_list.append(worker)
		self._lock.release()
		
		worker.setWork(work_routine, input_data)
		worker.start()
		return worker
		
	def wait(self, worker_list):
		for w in worker_list:
			if w == "Master":
				continue
			w.join()
	def finish(self):
		while len(self._active_worker_list) > 0:
			time.sleep(0.01) #in seconds
