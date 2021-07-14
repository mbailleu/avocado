#!/usr/bin/python

import sys, getopt, random
import numpy as np

class SimpleTraceGenerator:
    def __init__(self, expA, keyNo, requestNo):
        self.expA = expA
        self.keyNo = keyNo
        self.requestNo = requestNo

        self.keyPopularity = [0] * self.keyNo
        self.relativeKeyPopularity = [0] * self.keyNo

        self.calcKeyPopularities()
        #print self.hotKeyPopularity
        
    def produceTrace(self):
        batch_size = 10000
        for i in range(0, self.requestNo / batch_size):                  # input from standard input
            keys =  np.random.choice(self.keyNo, batch_size, p = self.relativeKeyPopularity)
            for key in keys:
                print key

    def calcKeyPopularities(self):
        popularity_of_all_keys = 0
        for i in range(1, self.keyNo + 1):
            #Start section <changed recently>
            if self.expA == 0: #this is for uniform
                keyPopularity = 1 / float(self.keyNo)
            else:
                keyPopularity = 1 / pow(i, self.expA)
            #End section <changed recently>
            self.keyPopularity[i-1] = keyPopularity
            popularity_of_all_keys += keyPopularity

        for i in range(1, self.keyNo + 1):
            self.relativeKeyPopularity[i - 1] = self.keyPopularity[i - 1] / popularity_of_all_keys
