# -*- coding: utf-8 -*-
# 1) PARSING:  (str -> list), e.g., a=a1,a2;$c --> a:[ [a1,a2] , [$c ] / c=c1;c2 --> c:[[c1],[c2]]
# 2) ITERATIVE VARIABLES SUBSTITUTIONS: a: [ [a1,a2], [ [c1],[c2] ] ]
# 3) RECURSIVE CONCAT: --> a:[ [a1,a2], [c1c2] ] -> [ a1c1c2 , a2c1c2 ] / --> c:[ [c1c2] ]
# 4) COMB_LIST: --> [ [a1c1c2,c1c2], [a2c1c2,c1c2]]
# 5) REDICT: --> [{a: a1c1c2,c:c1c2}, {a:a2c1c2,c:c1c2}]
import listUtils
import re
import copy
import sys

def parseCommands(f, cmdToActionsDict, cnfDict,commandPattern='%[a-z,A-Z,0-9,\_,\.]+%'):

    #print "parseCommands():", cnfDict

    try:
	print "parseCommands(), Opening file",f
	confFile = open(f, "r")
    except IOError,e:
	#print "Error opening file " + paramsDict["jdlGenConf"]
	print e
	#usage()
	sys.exit(-1)

    #additional config entries generated by parsing of the commands
    #commandConfDict=dict()

    cnfDictCopy=copy.deepcopy(cnfDict)

    #print "parseCommands(), pattern: ",commandPattern
    for l in confFile:
	l=stripBegEndWhitespaces(l)
	if(l!='') and re.match(commandPattern,l)!=None: #is a valid command or special key	  	
	  parse(l,cnfDictCopy)
	else:
	  print "parseCommands(), skipping \""+l+"\" , not a command or special key"

    #print cnfDictCopy

	#items=l.split('=')
	#cmdItem=items[0]

    #the cnf Dictionary now has additional keys (%CMD%[:$var]=LL)

    #for all key->config doAction()
    #return all the obtained configurations
    #commandConfig=[]

    #print cnfDictCopy

    commandsVar=[]
    for k in cnfDictCopy.keys():
      #print "Analyzing key for command processing : " + k
      if re.match(commandPattern,k)!=None:
	cmdKey = getCommandKey(k)
	print "parseCommand(), str: " +  k + ", key: " + cmdKey
	[var,newconfig]=cmdToActionsDict[cmdKey](k,cnfDictCopy[k],cnfDictCopy)
	#print newconfig
	if var!=None and newconfig!=None :
		commandsVar+=[var]
		cnfDict[var]=newconfig

    return [commandsVar,cnfDict]

#extract the cmd from the cmdKey which include the 
#markers '%'
def getCommandKey(st):
  pattern='%.*%'
  s=re.search(pattern,st)
  found=s.group(0)
  #print found
  return found[1:len(found)-1]

def stripBegEndWhitespaces(line):
    if line=="":
      return line
    l=re.sub('(^\s+)+','',line) #strip whitespace chars on the beginning
    if len(l)>0:
      l=re.sub('(\s+\Z)+','',l) #strip whitespace chars on the left
    return l

def parseConfig1(f, excludePrefixPatternList):

    cnfDict=dict()

    try:
	print "Opening file",f
	confFile = open(f, "r")
    except IOError,e:
	#print "Error opening file " + paramsDict["jdlGenConf"]
	print e
	#usage()
	sys.exit(-1)

    print "Parsing.."
    for l in confFile:
	#1 PARSING
	parse(l,cnfDict, excludePrefixPatternList)
    #2 VARIABLE SUBSTITUTION
    print "Substitution.."
    substitute(cnfDict)
    #3 RECURSIVE LIST CONCAT
    print "Recursive Reduction"
    configReduce(cnfDict) #TODO
    return cnfDict

#3
def configReduce(confDict):
  for k in confDict.keys():
    confDict[k]=listUtils.reduceListConcat(confDict[k])

#5 
def redict(keyList, L):

  ret=[]
  for l in L:#this is a list!
    newDict=dict()
    index=0
    for j in l:#this is an elem	
	newDict[keyList[index]]=j
	index+=1
    ret+=[newDict]

  return ret  

#1
def parse(line,cnfDict, excludePrefixPatternList=[]):
	l=stripBegEndWhitespaces(line)
	if (l==""):
	    print "Skipping empty line.."
	    return
	elif (l[0]=="#"):
	    print "Skipping comment.."
	    return
	else:
	    for p in excludePrefixPatternList:
	      if re.search('(^'+p+'+)',l)!=None:
		print "WARN: Skipping line \"" + l + "\" due to excludePatternList inclusion"
		return

	items=line.split("=");
	#print str(items)
	dx=items[1].strip('\n').split(";")
	dxx=[]
	for j in dx:
	  if len(dx)==1:
	    dxx+=j.split(',') 
	  else:
	    dxx+=[j.split(',')]
	k=items[0].strip()
	cnfDict[k]=dxx
	print "parse(), line=", line, ", ", k, " -> ", dxx


def searchForVarInList(L):
  for j in L:
    #print "searchForVarInList(): j=", j
    if type(j)==type(list()):
      if searchForVarInList(j)==True:
	return True
    elif re.search('\$',j)!=None:
      return True
  return False

#2 
def substitute(cnfDict):
    keysForSubstitution=searchForSomeVarInConf(cnfDict) #2.1
    oldKeys = []
    while keysForSubstitution!=oldKeys and keysForSubstitution!=[]:
      oldKeys = keysForSubstitution
      for k in keysForSubstitution:
        cnfDict[k]=searchAndSubstitute(cnfDict[k],cnfDict) #2.2
      keysForSubstitution=searchForSomeVarInConf(cnfDict)

    if keysForSubstitution!=[]:
      print "WARNING: some variables did not resolved"

#2.1
def searchForSomeVarInConf(confDict):
  keyList=[]
  for k in confDict.keys():
    if searchForVarInList(confDict[k])==True:
      keyList+=[k]

  return keyList

#2.2
def searchAndSubstitute(L,confDict):
    for i in range(0,len(L)):
      if type(L[i])==type(list()):
	searchAndSubstitute(L[i],confDict)
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
		if k in confDict.keys():
		  newLi+=[confDict[k]]
		else: #not found, pheraps become from a command
		  print "WARN: key", it, " not found, may be is a command variable!"
		  newLi+=[[it]]
	      else: #not a var
		newLi+=[[it]]

	  if(newLi!=[]):
	    L[i]=newLi #substitute
#	  print "searchAndSubstitute(), L[i] finally: ",L[i]

    return L

def test():
    parseConfig("test.config.txt")
