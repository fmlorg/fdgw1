#!/usr/bin/env python
#
# cachepurgy - script to reduce the size of a jftpgw cache
#
# (C) 2001 Julian Einwag <julian@brightstar.swin.de> 
#

from sys import *
from os import *

from string import *
from stat import *
from time import *

maxsize = 40*1024*1024

cachedir = "/tmp/cache"

# This class stores the information of an object in cache (size, age, etc...)

class fileinfo:
    def __init__(self, name):
        statobj = stat(name)
        self.age = time()-statobj[ST_CTIME]
        self.size = statobj[ST_SIZE]
        self.name = name
        self.isempty = 0
        if path.isdir(name):
            self.isdir = 1
            self.isempty = isempty(name)
        else:
            self.isdir = 0
                
    def __cmp__(self, other):

        # We want to have older items first

        return  cmp(other.age, self.age)
        
        
# Checks if a dir is empty

def isempty(dir):
    if len(listdir(dir)) == 0:
        return 1
    else:
        return 0

# Caclulates the size of the cache

def cachesize(stats):
    size = 0
    for file in stats:
        size = size + file.size
    return size

# This removes empty dirs from the cache

def removedirs(stats):
    for file in stats:
        if file.isdir and file.isempty:
            print "Removing directory: ", file.name
            rmdir(file.name)
    
# Cleans the cache

def cleancache(stats):
    if cachesize(stats) > maxsize:
        if (not stats[0].isdir):
            print "Delete: %s" % stats[0].name
            try:
                unlink(stats[0].name)
            except OSError:
                stdout.write("File %s does not exist!" % stats[0].name)

        # Yeah, I love LISP and recursion
        
        cleancache(stats[1:])
    else:
        return
    
def main():
    input = popen("find %s -print 2> /dev/null" % cachedir, 'r')
    
    cacheindex = input.readlines()
    input.close()

    try:
        chdir(cachedir)
    except OSError:
        stderr.write("Cachedir %s does not exist!\n" % cachedir)
        exit(1)
        
    cacheindex = map(rstrip, cacheindex)
    stats = map(fileinfo, cacheindex)
    stats.sort()

    cleancache(stats)
    removedirs(stats)
    
    
if __name__ == '__main__':
    main()
    
