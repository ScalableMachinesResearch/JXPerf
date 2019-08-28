import csv

class CSVWriter:
	def __init__(self, file_name, field_list):
		assert(isinstance(field_list, list))
		if file_name.endswith(".csv"):
			self._file_name = file_name
		else:
			self._file_name = file_name + ".csv"
		self._field_list = field_list
		self._row_list = [] 
	def writeRow(self, row_dict):
		assert(isinstance(row_dict, dict))
		self._row_list.append(row_dict)
	def finish(self):
		with open(self._file_name, "w") as f:
			fwriter = csv.DictWriter(f, fieldnames=self._field_list)
			fwriter.writeheader()
			rows = sorted(self._row_list, key = lambda x:int(x[self._field_list[-1]]), reverse = True)
#			for row in self._row_list:
			for row in rows:
				fwriter.writerow(row)
