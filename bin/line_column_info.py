#!/usr/bin/python3

# This script munges the output of dwarfdump to get a lists simple lists
# where each line is of the form:
# filename:lineno:column

import subprocess
import re
import sys
import os

def munge_line(line):
    # output
    # file:linue:column
    global filename
    #find if address on line
    if re.match("^0x", line):
        lineno_column = re.search(r'(\d+),\s*(\d+)', line).group()
        lineno = re.sub(r'(\d+),\s*(\d+)', r'\1', lineno_column)
        column = re.sub(r'(\d+),\s*(\d+)', r'\2', lineno_column)
        #find if there is a file name on line
        if re.search(r' uri: ', line):
            # get file name and strip off quotes at beginning and end
            filename = re.search(r'\"[^\"]+\"', line).group()
            filename = re.search(r'[^\"]+', filename).group()
        #generate munged line
        print(f'{filename}:{lineno:0>6}:{column:0>3}')

def find_debuginfo(fileointerest):
    # FIXME This should properly report if no debuginfo is found.
    good_debug = fileointerest

    # get a list of links and find the live one for the debuginfo
    process = subprocess.Popen(['dwarfdump', '--print-gnu-debuglink',
                                fileointerest], 
                               stdout=subprocess.PIPE,
                               universal_newlines=True)

    for output in process.stdout:
        output = re.sub(r'\n', "", output)
        if re.search(r'  \[\d+\] ', output):
            possible_debug = re.sub(r'  \[\d+\] ', "", output)
            if os.path.isfile(possible_debug):
                good_debug = possible_debug
    return good_debug

def process_file(fileointerest):
    fileointerest = find_debuginfo(fileointerest)
    process = subprocess.Popen(['dwarfdump', '--print-lines', fileointerest], 
                               stdout=subprocess.PIPE,
                               universal_newlines=True)
    for output in process.stdout:
        munge_line(output.strip())

files = sys.argv
del files[0]
for f in files:
    process_file(f)
