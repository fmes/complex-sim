#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import re
import string
import shutil
import sys, getopt
import time

#from xml.dom.ext.reader import Sax2
#from xml.dom.ext import PrettyPrint
#from xml.dom.NodeFilter import NodeFilter

paramsDict = {
    "filterFileName": "filter.txt",
    "listingFileName": "listing.txt",
    "confdir": "./conf",
    "testCaseDirPrefix": ".",
    "onlyTemplate": "0"}

testCaseDirPrefix='work'

def usage():
	print "usage: generateTestCases [-l listing_filename] [-f filter_filename] [-c confdir] [ -t ]"
	print "-t generates only a filter template"

def initializeArgs(sysargs):
    try:
        optlist, list = getopt.getopt(sysargs, ':f:l:hc:s:t')
    except getopt.GetoptError:
        print "argument error"
        usage(); 
	rollback()
        sys.exit(1)
    
    #print "optlist =", optlist
    #print "list =", list
    for opt, a in optlist:
        #print opt
        if opt == '-f':
            print "selected filter file: ", a
            paramsDict["filterFileName"] = str(a).strip();
        elif opt == '-t':
            print "Selected only template generation.."
            paramsDict['onlyTemplate']='1'
        elif opt == '-l':
            l=str(a).strip(); 
            if len(l)>0:
                paramsDict["listingFileName"] = l
	        print "selected listing file: ", paramsDict["listingFileName"];
	    else:
		print "bad argument to ", opt, " option"
                print "listing file: ", paramsDict["listingFileName"];
        elif opt == '-c': 
            l = str(a).strip();
            if len(l)>0: 
                paramsDict["confdir"] = l
                print "selected conf dir: ", paramsDict["confdir"]; 
            else: 
                print "bad argument to ", opt, " option"
                print "conf dir: ", paramsDict["confdir"];

        elif opt == '-h':
            usage();
	    rollback
            sys.exit(-1);


#----------------------------------------------------------
class AbstractTestCaseGenerator:
    def __init__(self, confDir, fileSuffix):
        self.confDir = confDir
        #error
        self.fileSuffix = fileSuffix
        self.testCaseFileNamePrefix = 'work'

    def initializeOutput( self, testCaseFileNameXml ): abstract
    def finalizeOutput( self, testCaseFileNameXml ): abstract

    def generate(self, arr, arrind):
        testCaseFileName=self.testCaseFileNamePrefix

        i = 0
        while i<len(arr):
            el = arr[i]
            currEl=el[arrind[i]]
            b=currEl.split('/')
            testCaseFileName+="."+b[len(b)-1]
            i+=1

        self.testCaseDirName = "d"+testCaseFileName
        testCaseFileName+=self.fileSuffix
        #print testCaseFileName
        self.testCaseFileNamePath = os.path.join(paramsDict["testCaseDirPrefix"],testCaseFileName);
	print self.testCaseFileNamePath
        #if (copyTemplate == true):
        #    shutil.copyfile(skeletonFileName, testCaseFileNamePath)
        self.initializeOutput( )

        # main loop
        i=0
        while i<len(arr):
            el = arr[i]
            currEl=el[arrind[i]]

            infile = open(currEl)
            for inputString in infile:
                instruction = inputString.strip()

                if len(instruction)==0 or instruction.startswith("#"):
                    # line made of blank(s), or comment
                    continue
                self.parseDirectiveAndInsert( instruction )
            infile.close()
            i+=1

        self.finalizeOutput( )

        return self.testCaseFileNamePath

#-----------------------------------------------------------------
class TestCaseGeneratorXml(AbstractTestCaseGenerator):
    def __init__(self, confDir):
        AbstractTestCaseGenerator.__init__(self, confDir, ".xml")
        self.xmlreader = Sax2.Reader()
        self.testCaseDoc = 0


    def initializeOutput( self ):

        testCaseFileXml = open(self.skeletonFileName,"r")
        self.testCaseDoc = self.xmlreader.fromStream(testCaseFileXml)
        testCaseFileXml.close()

        self.parseDirectiveAndInsert( "general/@workDir=\""+paramsDict["testCaseDirPrefix"]+"/"+self.testCaseDirName+"\"")

    def finalizeOutput( self ):
        outfile = open(self.testCaseFileNamePath, "w")
        print "saving..."
        PrettyPrint(self.testCaseDoc, outfile)
        #os.mkdir(testCaseDirPrefix+"/"+testCaseDirName)
        outfile.close()


    def parseDirectiveAndInsert(self, instruction):
        doc = self.testCaseDoc

        if instruction.startswith("/"):
            instruction = instruction[1:]
        # analizza direttiva

        res = instruction.rsplit( "=", 1 )
        pathTo = res[0].strip()

        value = res[1].strip()
        if value.startswith("\""):
            value=value[1:]
        if value.endswith("\""):
            value=value[:len(value)-1]

        # separa in base a "/", per ognuno controlla se il nodo c'è, 
        # se non c'è crea 
        res = pathTo.rsplit( "/" )
        currNode = doc.documentElement

        conditions = False
        for elem in res:
            #individua nomi di nodi con selettore
            # nome[@attr=value]
            el = elem
            p=re.compile('(\w+)\s*\[@\s*(\w+)\s*=\s*"\s*(\w+)\s*"\s*\]')
            ris = p.match( elem )
            if ris!=None:
                el=ris.group(1)
                condName=ris.group(2)
                condValue=ris.group(3)
                conditions=True
            
            # se inizia per @, è un attributo - va aggiunto al nodo corrente
            # altrimenti è un elemento
            if not el.startswith("@"):
                found = False
                child = currNode.firstChild

                # raggiungi nodo/attributo puntato; se i nodi lungo la strada 
                # mancano, aggiungili
                while not found and child != None:
                    if child.nodeType==child.ELEMENT_NODE and child.nodeName==el:
                        if not conditions:
                            currNode = child
                            found = True
                        else:
                            attrs=child.attributes
                            for i in range(attrs.length):
                                if attrs.item(i).name==condName and attrs.item(i).value==condValue:
                                    currNode = child
                                    found = True
                                    break
                    child = child.nextSibling

                if not found:
                    newNode = doc.createElement( el )
                    if conditions:
                        newNode.setAttribute(condName, condValue)
                    currNode.insertBefore(newNode,currNode.firstChild)
                    currNode = newNode

            #modifica doc opportunamente
            else:
                currNode.setAttribute(el[1:], value)
                #continua il ciclo comunque

        #print("=============================")
        #print(instruction)
        #print("-------------------")
        #PrettyPrint(testCaseDoc)
#----------------------------------------------------------
class TestCaseGeneratorPlain(AbstractTestCaseGenerator):
    def __init__(self, confDir):
        AbstractTestCaseGenerator.__init__(self, confDir, "")

        self.testCaseDoc = 0
        self.outfile = ""

    def initializeOutput( self ):
        if self.testCaseFileNamePath=="":
            return
        #shutil.copyfile(self.skeletonFileName, self.testCaseFileNamePath)

        self.outfile=open( self.testCaseFileNamePath, "w" )
        #self.parseDirectiveAndInsert( "workDir=\""+testCaseDirPrefix+"/"+self.testCaseDirName+"\"")

    def finalizeOutput( self ):
        if self.outfile=="":
            return
        self.outfile.close()


    def parseDirectiveAndInsert(self, instruction):
        self.outfile.write( instruction + "\n" )

#----------------------------------------------------------
class Generator:

    def printCurrentString(self, arr, arrind):
        print arrind
        i = 0
        while i<len(arr):
            el = arr[i]
            print el[arrind[i]]
            i+=1

    def mylistdir(self, directory):
        """ thanks to http://mail.python.org/pipermail/tutor/2004-April/029019.html """
        """A specialized version of os.listdir() that ignores files that
        start with a leading period."""
	try:
          filelist = os.listdir(directory)
	except Exception, e:
	  print e
	  usage()
	  sys.exit(-1)
        return [x for x in filelist
                if not (x.startswith('.'))]


    def readFilter(self, filterFileName):
        filter = {}
        try:
            filterFile = open(filterFileName)

            for line in filterFile:
                #print line
                sep = line.split('=')
                key = sep[0].strip()
                #print key
                values = sep[1].split(",")
                valuesS = []
                for el in values:
                    valuesS.append(el.strip())
                filter[key]=valuesS

            filterFile.close()
        except IOError:
            print "Filter file not found!!"
	    usage()
	    rollback()
	    sys.exit(-1)	
            # do nothing

        return filter
    def generateTemplate(self):	
        #confDir = testCaseDirPrefix+"/"+"conf"	
        confDir = paramsDict["confdir"]
	templatefname = os.path.abspath("generated-filter-template_"+str(time.time()))
	templatef = open(templatefname, 'w');

        testCaseGenerator = TestCaseGeneratorPlain(confDir)
        # azzera vettore conf

        confArray = []
        currentStringIndex = []

        testCasesDirs = self.mylistdir(confDir+'/')
        testCasesDirs.sort()

        # read configuration
        for adir in testCasesDirs:
        #    inizializza vettore locale
        #
            confArrayElem = []
            adirAbs = os.path.join(confDir,adir)
            adirElem = self.mylistdir(adirAbs)

	    templatef.write(adir + "=")
	    init=1
	    
	    for d in adirElem:
		if(init==0):
		    templatef.write(",")
		else:
		    init=0		    
		templatef.write(d)

	    templatef.write("\n");

	print "Template conf in " + templatefname
	templatef.close()	    

    def generate(self):
        #confDir = testCaseDirPrefix+"/"+"conf"	
        confDir = paramsDict["confdir"]

        testCaseGenerator = TestCaseGeneratorPlain(confDir)
        # azzera vettore conf

        confArray = []
        currentStringIndex = []

        testCasesDirs = self.mylistdir(confDir+'/')
        testCasesDirs.sort()

        filter = self.readFilter(paramsDict["filterFileName"])

        # read configuration
        for adir in testCasesDirs:
        #    inizializza vettore locale
        #
            confArrayElem = []
            adirAbs = os.path.join(confDir,adir)
            adirElem = self.mylistdir(adirAbs)
            filterElements = [] 
            if (adir in filter):
                filterElements = filter[adir]

        #   per ogni elemento della dir
            for elem in adirElem:
        #       se esiste un filtro e l'elemento non è stato filtrato 
        #       (file di configurazione)
                if not filter or (elem in filterElements):
        #           aggiungilo al vettore locale
                    confArrayElem.append(os.path.join(adirAbs,elem))

            confArray.append(confArrayElem)
            currentStringIndex.append(0)

        print confArray

        print "opening file ", paramsDict["listingFileName"]
        listingFile = open(paramsDict["listingFileName"], 'w')

        #printCurrentString(confArray, currentStringIndex)
        fname = testCaseGenerator.generate(confArray, currentStringIndex)

        if len(fname)!=0:
            listingFile.write(fname+"\n")

        #print "debug"
        endGeneration = False

        index = 0

        while not endGeneration:
            endIncrement = False
            while (index < len(currentStringIndex)) and (not endIncrement):
                # se indice non è ultimo possibile
                if (currentStringIndex[index]<len(confArray[index])-1):
                    # incrementa ed esci da questo ciclo
                    endIncrement = True
                    currentStringIndex[index]+=1
                    index=0
                else:
                    #poni a zero elemento valore corrispondente a indice corrente, 
                    currentStringIndex[index]=0
                    #e incrementa indice corrente
                    index+=1
            #stampa la stringa corrispondente a currentStringIndex
            if (index >= len(currentStringIndex)):
               endGeneration = True
            else:
                #printCurrentString(confArray, currentStringIndex)
                fname = testCaseGenerator.generate(confArray, currentStringIndex)
                if len(fname)!=0:
                    listingFile.write(fname+"\n")

def rollback():

        if(os.path.exists(paramsDict['testCaseDirPrefix'])):
	
	    try:
		os.rmdir(paramsDict['testCaseDirPrefix'])
	    except Error,e:
		os.remove(paramsDict['testCaseDirPrefix'] + "/*")
		os.rmdir(paramsDict['testCaseDirPrefix'])


def main():
    initializeArgs(sys.argv[1:])
    maingenerator = Generator()
    if (paramsDict['onlyTemplate'] == '1'):
	maingenerator.generateTemplate()
    else:
	paramsDict['testCaseDirPrefix']=os.path.abspath("generated-test-case"+str(time.time()))
        os.mkdir(paramsDict['testCaseDirPrefix'])
        maingenerator.generate()

if __name__ == "__main__":
    main()
