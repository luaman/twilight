import commands
import os
import sys
import string

class My_Options:
	def __init__(self):
		self.help = {}
		self.values = {}

	def set(self, target, value):
		self.values[target] = value;

	def create(self, target, default, help=None):
		if not self.values.has_key(target):
			self.values[target] = default;
		if help != None:
			self.help[target] = help;

	def __getitem__(self, target):
		return self.values[target]

	def has_key (self, target):
		return self.values.has_key (target)

	def get(self, target):
		return self.values[target]

	def args(self, args):
		for item in args.keys():
			if (self.values.has_key(item)):
				self.values[item] = args[item]


	def keys(self):
		return self.values.keys()

	def save(self, file):
		fh = open (file, "w")
		try:
			for item in self.values.keys():
				fh.write ("%s = %s\n" % (item, repr(self.values[item])))
		finally:
			fh.close ();
	
	def load(self, file):
		try:
			fh = open (file, "r")
			try:
				for line in fh.readlines():
					line = string.strip(line)
					tmp = string.split(line, " = ")
					self.values[tmp[0]] = eval(tmp[1])
			finally:
				fh.close()
		except:
			return 0
		return 1


