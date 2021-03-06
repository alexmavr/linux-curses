from ctypes import *

import os
from sys import argv
lib=CDLL("libcurse.so")


LIST_ALL, CURSE_CTRL,		\
ACTIVATE, DEACTIVATE,		\
CHECK_CURSE_ACTIVITY,		\
CHECK_TAINTED_PROCESS,		\
CAST, LIFT, GET_CURSE_NO,	\
SHOW_RULES,					\
ADD_RULE, REM_RULE,			\
ILLEGAL_COMMAND = range(13)

INH_ON,						\
INH_OFF,					\
USR_ACTIVE_PERM_ON,			\
USR_ACTIVE_PERM_OFF,		\
USR_PASSIVE_PERM_ON,		\
USR_PASSIVE_PERM_OFF,		\
SU_ACTIVE_PERM_ON,			\
SU_ACTIVE_PERM_OFF,			\
SU_PASSIVE_PERM_ON,			\
SU_PASSIVE_PERM_OFF   = range(10)


def showhelp():
	magic()
	doc = """
	This tool, enables usage of the curse system
Help:
	-a activate	
	-d deactivate
	-c cast		
	-l lift
	-t check tainted process
	-s check curse status (global)
	-N curse name
	-L List
	-P Pid 
	-i + / -
	-p u/s+/-a/p


Example:
	cursepie -a -N no_fs_cache
	"""
	more_permissions = """
    |   0    |      1       |      2      |   3  |
----+--------+--------------+-------------+------+
1st | no SU  | SU_PASSIVE   | SU_ACTIVE   | BOTH |
----+--------+--------------+-------------+------+
2nd | no USR | USR_PASSIVE  | USR_ACTIVE  | BOTH |
----+--------+--------------+-------------+------+
	"""

	print doc
	return 1

def parse_cbuf(sth, ln):
	a = []
	for j in range(ln):
		i = j*32
		a.append(curse_list_entry(sth[i:i+32]))
	return a

def curse(command, name, target, ctrl, userbuf):
	lib.curse.argtypes = [c_int, c_char_p, c_int, c_int, c_char_p]
	return lib.curse(command, name, target, ctrl, userbuf)

class curse_list_entry(Structure):
	_buf_size_ = 24
	_pack_ = 1
	_fields_ = [('curse_name',c_char*24), ('curse_id', c_ulonglong)]
	def __init__(self,st):
		self.curse_name = st[:self._buf_size_]
		self.curse_id = self.parseInt(st[self._buf_size_:])
	def __repr__(self):
		return "{0}: {1:#08x}".format(self.curse_name,self.curse_id)
	def parseInt(self,sth):
		sth = map(ord, sth)
		s = 0
		for i,j in enumerate(sth):
			s += 256**i*j
		return s


def activate(switches):
	try:
		c_name = switches['-N']
	except KeyError:
		c_name = 'system'
	r =  curse(ACTIVATE, c_name, 0, 0, None)
	if r == 1:
		print "Activated {0}".format(c_name)
	else:
		print "Error {0}".format(r)
	return r

def deactivate(switches):
	try:
		c_name = switches['-N']
	except KeyError:
		c_name = 'system'
	r = curse(DEACTIVATE, c_name, 0, 0, None)
	if r == 1:
		print "Deactivated {0}".format(c_name)
	else:
		print "Error {0}".format(r)
	return r

def cast(switches):
	try:
		c_name = switches['-N']
		c_pid = int(switches['-P'])
	except KeyError:
		showhelp()
		return 1
	r = curse(CAST, c_name, c_pid, 0, None)
	if r == 1:
		print "Cast {0} on {1}".format(c_name, c_pid)
	else:
		print "Error {0}".format(r)
	return r

def lift(switches):
	try:
		c_name = switches['-N']
		c_pid = int(switches['-P'])
	except KeyError:
		showhelp()
		return 1
	r = curse(LIFT, c_name, c_pid, 0, None)
	if r == 1:
		print "Lifted {0} on {1}".format(c_name, c_pid)
	else:
		print "Error {0}".format(r)
	return r


def listC(switches):
	c_no = curse(GET_CURSE_NO, None, 0, 0, None)
	size = c_no*sizeof(curse_list_entry)
	c_buf = create_string_buffer(size)
	dome = curse_list_entry*c_no
	r = curse(LIST_ALL, None, 0, 0, c_buf)
	if r == 1:
		for el in parse_cbuf(c_buf, c_no):
			print el
	else:
		print "Error {0}".format(r)
	return r

def check_tainted_proc(switches):
	try:
		c_name = switches['-N']
		c_pid = int(switches['-P'])
	except KeyError:
		showhelp()
		return 1
	r = curse(CHECK_TAINTED_PROCESS, c_name, c_pid, 0, None)
	if r == 1:
		stat = "has been cast"
	else:
		stat = "has not been cast"
	print 'Curse "{0}" {1} on pid: {2}'.format(c_name, stat, c_pid)
	return r

def check_curse_status(switches):
	try:
		c_name = switches['-N']
	except KeyError:
		showhelp()
		return 1
	r = curse(CHECK_CURSE_ACTIVITY, c_name, 0, 0, None)
	if r == 1:
		stat = "Has been cast"
	else:
		stat = "Has not been cast"
	print 'Curse "{0}" status: {1}'.format(c_name,stat)
	return r

def permissions(switches):
	perms = {'u'	: 'USR', 
			'+'		: 'ON',
			'-'		: 'OFF',
			's'		: 'SU',
			'a'		: 'ACTIVE',
			'p'		: 'PASSIVE'}
	try:
		par = switches['-p']
		control_string = "{0}_{1}_PERM_{2}".format(perms[par[0]], perms[par[2]], perms[par[1]])
		c_control = eval(control_string)
		c_pid = int(switches['-P'])
	except KeyError, IndexError:
		showhelp()
		return 1
	r = curse(CURSE_CTRL, 'system', c_pid, c_control, None)
	if r == 1:
		stat = "Perms have been set"
	else:
		stat = "Perms have not been set"
	print 'Status: {0}'.format(stat)
	return r


def inheritance(switches):
	inhs = {'+'	: INH_ON,
			'-' : INH_OFF}
	try:
		inh = switches['-i']
		c_inherit = inhs[inh]
		c_name = switches['-N']
	except KeyError:
		showhelp()
		return 1
	r = curse(CURSE_CTRL, c_name, 0, c_inherit, None)
	if r == 1:
		stat = "Inheritance has been set to {0}".format(inh)
	else:
		stat = "Inheritance has not been set"
	print 'Status: {0}'.format(stat)
	return r








def magic():
	alm = """
............................................________
....................................,.-'"...................``~.,
.............................,.-"..................................."-.,
.........................,/...............................................":,
.....................,?......................................................,
.................../...........................................................,}
................./......................................................,:`^`..}
.............../...................................................,:"........./
..............?.....__.........................................:`.........../
............./__.(....."~-,_..............................,:`........../
.........../(_...."~,_........"~,_....................,:`........_/
..........{.._$;_......"=,_......."-,_.......,.-~-,},.~";/....}
...........((.....*~_......."=-._......";,,./`..../"............../
...,,,___.`~,......"~.,....................`.....}............../
............(....`=-,,.......`........................(......;_,,-"
............/.`~,......`-...................................../
.............`~.*-,.....................................|,./.....,__
,,_..........}.>-._...................................|..............`=~-,
.....`=~-,__......`,.................................
...................`=~-,,.,...............................
................................`:,,...........................`..............__
.....................................`=-,...................,%`>--==``
........................................_..........._,-%.......`
..................................., 
"""
	print alm
