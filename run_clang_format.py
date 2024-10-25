#! /usr/bin/python

# Copyright 2016-2019 NXP
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause

# To use this script run
# $./run_clang_format.py
from __future__ import print_function
import subprocess
import sys,os

#Folders to scan
folders = []
folders.append("lib");

#Files which will be not formatted
exceptions = []

#For windows use "\\" instead of "/" path separators.
if os.environ.get('OS','') == 'Windows_NT':
    for i, folder in enumerate(folders):
        folders[i] = os.path.normpath(folder)

    for i, ext in enumerate(exceptions):
        exceptions[i] = os.path.normpath(ext)

#Files with this extensions will be formatted/
extensions = []
extensions.append(".h")
extensions.append(".c")

#processing formatting
for folder in folders:
    print('*****************************************************************************')
    print(folder);
    for path, subdirs, files in os.walk(folder):
        for name in files:
            if any(ext in name for ext in extensions):
                file = os.path.join(path, name)
                if file in exceptions:
                    print("Ignored: ", file)
                else:
                    print("Formatting: ", file)
                    subprocess.call(["clang-format", "-i", file])
    print('*****************************************************************************\n')
