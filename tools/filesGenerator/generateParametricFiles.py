#!/usr/bin/python
# -*- coding: utf-8 -*-

import os, math, os.path
import re
import string
import shutil
import sys, getopt
import tarfile
import datetime
import time
import tempfile
import copy

import listUtils
import fileUtils

def main():

  paramsDict=ParDict()
  paramsDict.initializeArgs(sys.argv[1:])
  dct=paramsDict.add()

#  print 'dct list:', dct
  for i in dct:
    print ''
    print 'Dictionary of parameters analyzed:', i
    print ''
    Files=Generator(i)
#    print 'Files:', Files.Dict, Files.templateLines
    Files.preGen() # creo la cartella che conterrà i file della generazione
    Files.add()

    cDict=CommandDict(i) # creo il dizionario che conterrà le righe del file di configurazione
    cDict.add()
    c=cDict.parseCommands() # eseguo le operazioni specifiche per i diversi comandi e metto in c le variabili ottenute dai comandi %TAR% o %ZIP%

    cDict.substitute() # utilizzo i valori delle variabili ottenute dai comandi %TAR% o %ZIP%
#    cDict.printDict()
    cDict.delKeys(c) # e quindi cancello dal dizionario gli elementi che riguartano tali variabili.
    cDict.minimize() # combino i valori delle chiavi nel dizionario, cioè: 
    # sia 'a': [[['b1'], ['b2', 'b3'], ['b4']], [' aa '], ['c1', 'c2']]
    # ottengo tutte le possibili combinazioni: 'b1b2b4 aa c1', 'b1b2b4 aa c2', 'b1b3b4 aa c1', 'b1b3b4 aa c2' 
#    cDict.printDict()
    f=Files.minimize(cDict.Dict) # preparo il dizionario alla generazione dei file combinando i valori fra tutte le chiavi
    Files.genFiles(f) # ed in fine creo i file 
#    Files.printDict()

#------------------------------
# classe Astratta di Base.
class Abstract(dict):
  
  def __init__(self):
    self.Dict=dict()

  def printDict(self):
    print '__ Dict:', self.Dict

  def add(self):
    raise Exception,'abstract method is not defined: add()!'

  def minimize(self):
    raise Exception,'abstract method is not defined: minimize()!'

#------------------------------
# classe Generatore. Da essa vengono creati i file a contenuto parametrico e la cartella che li conterrà.
class Generator(Abstract):
  
  def __init__(self, pDict):
    self.Dict=pDict
    self.templateLines=[]

  def preGen(self):
    #set the name of the directory for generation
    d = datetime.datetime.fromtimestamp(time.time())
    dir=self.Dict['dirOut']
    print "files-dir= "+os.path.basename(dir)
    os.mkdir(dir)
    self.Dict["files-dir"]=dir

  def add(self):
    #open the template file
    templateFile = open(self.Dict['template'], "r")
    #the tempfilename
    for row in templateFile:
        self.templateLines+=[row]
    templateFile.close()

  def minimize(self,cDict):
    keys=cDict.keys()
    f=self.__redict(keys,listUtils.reduceListMerge(cDict.values()))
    return f
  
  def genFiles(self,f):
    tmpf=tempfile.mkstemp()
    #cycle over the config
    c=0
    for dct in f:
	#generate a file
	c+=1
        fout = open(tmpf[1], "w")
        for row in self.templateLines:
	    retry=1
	    while retry==1:
		retry=0
		for k in dct.keys():
		    value = dct[k]
		    oldrow=row
		    row = string.replace(oldrow, str("$"+k), value)
		#yet another
		if (re.search('\$',row) != None):
		  retry=1
	    #finally write the row
	    fout.write(row)
	fout.close()

	fpath=os.path.join(os.path.abspath(self.Dict['files-dir']), "file_gen_" + str(c))
	os.system("mv " + tmpf[1] + " " + fpath )
    print "Generated files in " + self.Dict['files-dir']

  def __redict(self,keyList, L):
    ret=[]
    for l in L:#this is a list!
      newDict=dict()
      index=0
      for j in l:#this is an elem	
	  newDict[keyList[index]]=j
	  index+=1
      ret+=[newDict]
    return ret 

#------------------------------
# classe ParDict. Da essa vengono creati i diversi dizionari dei parametri necessari per la generazione dei file a contenuto parametrico. 
class ParDict(Abstract):

  def __init__(self):
    Abstract.__init__(self)
    self.inF=''

  def add(self):
    if self.inF!='':
      try:
	print "Opening file", self.inF
	inFile = open(self.inF, "r")
      except IOError,e:
	print e
	sys.exit(-1)
      ldict=[]
      for line in inFile:
	l=self.__stripBegEndWhitespaces(line)
	if (l==""):
	  print "Skipping empty line.."
	  continue
	elif (l[0]=="#"):
	  print "Skipping comment.."
	  continue
	items=line.strip('\n').split(',')
	self.Dict['genConf']=items[0]
	self.Dict['template']=items[1]
	if len(items)>2:
	  self.Dict['dirOut']=items[2]
	else:  
	  d = datetime.datetime.fromtimestamp(time.time())
	  pth = os.path.abspath("files-generated"+str(time.mktime(d.timetuple())))
	  self.Dict['dirOut'] = pth

	copyDict=copy.deepcopy(self.Dict)
	ldict.append(copyDict)

	print "parse(), line=", line
	print "genConf: ", self.Dict['genConf']
	print "template: ", self.Dict['template']
	print "dirOut: ", self.Dict['dirOut']

      return ldict
    else:
      self.__usage()
      sys.exit(-1)

  def __stripBegEndWhitespaces(self,line):
    if line=="":
      return line
    l=re.sub('(^\s+)+','',line) #strip whitespace chars on the beginning
    if len(l)>0:
      l=re.sub('(\s+\Z)+','',l) #strip whitespace chars on the left
    return l

  def initializeArgs(self,sysargs):
    try:
        optlist, list = getopt.getopt(sysargs, 'f:h')
    except getopt.GetoptError:
        print "argument error"
        usage()
        sys.exit(1);

    for opt, a in optlist:
        if opt == "-h":
	  self.__usage();
	  sys.exit(0);
        if opt == "-f":
	  try:
	    l = str(a).strip()
	    if(len(l)>0):
	      self.inF = l
	  except Exception:
	    print "error while reading input_file"
	    sys.exit(-1)

  def __usage(self):
    usage = "usage: " + sys.argv[0] + " -f <input_file> [-h]"
    print usage

#------------------------------
# classe VarDict, utile per la costruzione dei dizionari per i file di configurazione, che al loro interno specificano solo variabili. 
class VarDict(Abstract):

  def __init__(self, pDict):
    Abstract.__init__(self)
    self.pDict=pDict
  
  def add(self):
    try:
      print "Opening file", self.pDict['genConf']
      File = open(self.pDict['genConf'], "r")
    except IOError,e:
      print e
      #usage()
      sys.exit(-1)
    for line in File:
      l=self.__stripBegEndWhitespaces(line)
      if (l==""):
	print "Skipping empty line.."
	continue
      elif (l[0]=="#"):
	print "Skipping comment.."
	continue
      items=line.split('=')
      dx=items[1].strip('\n').split(';')
      dxx=[]
      for j in dx:
	if len(dx)==1:
	  dxx+=j.split(',') 
	else:
	  dxx+=[j.split(',')]
      k=items[0].strip()
      self.Dict[k]=dxx
      print "parse(), line=", line, ", ", k, " -> ", dxx
  
  def __stripBegEndWhitespaces(self,line):
    if line=="":
      return line
    l=re.sub('(^\s+)+','',line) #strip whitespace chars on the beginning
    if len(l)>0:
      l=re.sub('(\s+\Z)+','',l) #strip whitespace chars on the left
    return l
  
  def minimize(self):
    for k in self.Dict.keys():
      self.Dict[k]=listUtils.reduceListConcat(self.Dict[k])

  #2 
  def substitute(self):
    keysForSubstitution=self.__searchForSomeVarInConf() #2.1
    oldKeys = []
    while keysForSubstitution!=oldKeys and keysForSubstitution!=[]:
      oldKeys = keysForSubstitution
      for k in keysForSubstitution:
        self.Dict[k]=self.__searchAndSubstitute(self.Dict[k]) #2.2
      keysForSubstitution=self.__searchForSomeVarInConf()

    if keysForSubstitution!=[]:
      print "WARNING: some variables did not resolved"

  #2.1
  def __searchForSomeVarInConf(self):
    keyList=[]
    for k in self.Dict.keys():
      if self.__searchForVarInList(self.Dict[k])==True:
	keyList+=[k]
    return keyList
  
  #2.1.1
  def __searchForVarInList(self,L):
    for j in L:
      #print "searchForVarInList(): j=", j
      if type(j)==type(list()):
	if self.__searchForVarInList(j)==True:
	  return True
      elif re.search('\$',j)!=None:
	return True
    return False

  #2.2
  def __searchAndSubstitute(self,L):
    for i in range(0,len(L)):
      if type(L[i])==type(list()):
	self.__searchAndSubstitute(L[i])
      else:
#	print "searchAndSubstitute(): L[i]:",L[i]
	if re.search('\$',L[i])!=None: #there is a variable
	  items=re.split('(\$\S+)',L[i])
	  if(len(items)==1): #same as L[i]
	    print "ERROR: entry \"", L[i], "\" not correctly specified"
	    continue
	  newLi=[]
#	  print "searchAndSubstitute(), split:",items
	  for it in items:
	    if it!='':
	      if re.search('\$',it)!=None: #is a var
		k=it.split('$')[1]
		if k in self.Dict.keys():
		  newLi+=[self.Dict[k]]
		else: #not found, pheraps become from a command
		  print "WARN: key", it, " not found, may be is a command variable!"
		  newLi+=[[it]]
	      else: #not a var
		newLi+=[[it]]
	  if(newLi!=[] and len(L)!=1):
	    newLi=listUtils.reduceListConcat(newLi)
	    del L[i]
	    for b in newLi:
	      L.append(b) #substitute
	  else:
	    L[i]=newLi #substitute
#	  print "searchAndSubstitute(), L[i] finally: ",L[i]
    return L

#------------------------------
# classe CommandDict, utile per la costruzione dei dizionari per i file di configurazione e 
# capace di gestire qualsiasi tipo di riga presente nel file di configurazione, cioè sia definizione di varibili che di comandi.
class CommandDict(VarDict):
  
  def delKeys(self, cmdVar):
    for k in cmdVar:
#      print 'deleted item:', k
      del self.Dict[k]

  def __compressAction(self,key,config,cDict):
#    print "compressAction(), config=", config, " key=", key, " fDir=", fDir
    var=""

    #1.split command:$var
    keyItems = key.split(':')
    if len(keyItems)==1: # no var definition
      print "compressActions(), ERROR: no var definition, line=", key, ":", config
      sys.exit(-1)

    #2.parsing var
    else:
      if((keyItems[1].strip())[0]!='$'):
	print "malformed specified var in ", key
	sys.exit(-1)
      else:
	var = (keyItems[1].strip())[1:]

    #2.1build dictionary of configuration, if var was specified
    cDict[var]=config
    #3build compressed files
    #3.1 recognizes commands
    cmd=keyItems[0]
    if(cmd=='%TAR%'):
      f=fileUtils.produceTar(config)     
    elif(cmd=='%ZIP%'):
      f=fileUtils.produceZip(config)
    #3.2 move files on the files-dir directory
    dst=os.path.abspath(self.pDict['files-dir'])
    for l in f:
      src=os.path.abspath(l)
      print os.path.basename(l), "-->", dst
      shutil.move(src,dst)
    return [var,[f]]

  def __fileInAction(self,key,config,cDict):
    print "fileInAction(), key=",key,", config=" , config
    dst=os.path.abspath(self.pDict['files-dir'])
    for l in config:
      src=os.path.abspath(l)
      print os.path.basename(l), "-->", dst
      shutil.copy(src,dst)
    return [None,None]
  #return commandsVar

  __commandToActionDict={
	  'TAR':__compressAction,
	  'ZIP':__compressAction,
	  'FILE_IN':__fileInAction
	  }

  def parseCommands(self, commandPattern='%[a-z,A-Z,0-9,\_,\.]+%'):
    commandsVar=[]
    for k in self.Dict.keys():
      #print "Analyzing key for command processing : " + k
      if re.match(commandPattern,k)!=None:
	cmdKey = self.__getCommandKey(k)
	print "parseCommand(), str: " +  k + ", key: " + cmdKey
	[var,newconfig]=CommandDict.__commandToActionDict[cmdKey](self,k,self.Dict[k],self.Dict)
	if var!=None and newconfig!=None :
	  commandsVar+=[var]
	  self.Dict[var]=newconfig
	del self.Dict[k]
    return commandsVar
  
  def __getCommandKey(self,st):
    pattern='%.*%'
    s=re.search(pattern,st)
    found=s.group(0)
    #print found
    return found[1:len(found)-1]

if __name__=='__main__':
  main()
