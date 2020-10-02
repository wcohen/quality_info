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
    #find if there is a file name on line
    if re.search(r'\(mtime: (\d+), length: (\d+)\)$', line):
        filename = re.sub(r'^\s*([^\s]+)[^.]*', r'\1', line)
    #find if address on line
    if re.match("^\s*(\d+):(\d+)", line):
        lineno_column = re.search(r'^\s*(\d+):(\d+)', line).group()
        lineno = re.sub(r'^\s*(\d+):(\d+)', r'\1', lineno_column)
        column = re.sub(r'^\s*(\d+):(\d+)', r'\2', lineno_column)
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
    process = subprocess.Popen(['eu-readelf', '--debug-dump=decodedline', fileointerest],
                               stdout=subprocess.PIPE,
                               universal_newlines=True)
    for output in process.stdout:
        munge_line(output.strip())

files = sys.argv
del files[0]
for f in files:
    process_file(f)
