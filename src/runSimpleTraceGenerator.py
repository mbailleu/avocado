#!/usr/bin/python

import sys, getopt, random
from simpleTraceGenerator import SimpleTraceGenerator
import numpy as np

def main(argv):
    
    keyNo = 1000
    exponentA = 0.99
    requestNo = 100000

    try:
        opts, args = getopt.getopt(argv,"hw:k:a:")
    except getopt.GetoptError:
        print "-w <requestNo> -k <keyNo> -a <Zipf_exponent>"
        sys.exit(2)
    for opt, arg in opts:
        if opt == "-h":
            print "-w <requestNo> -k <keyNo> -s <server_No> -c <cache_size> -i <poison_interval_in_nanosecs> -r <write_rate> -a <Zipf_exponent>" 
            sys.exit()
        elif opt == "-a":
            exponentA = float(arg)
        elif opt == "-w":
            requestNo = int(arg)
        elif opt == "-k":
            keyNo = int(arg)

    tg = SimpleTraceGenerator(exponentA, keyNo, requestNo)
    tg.produceTrace()

if __name__ == "__main__":
    main(sys.argv[1:])
