#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

from perfetto.trace_processor import TraceProcessor as tp
from perfetto.trace_processor import TraceProcessorConfig as tpc

def which(program):
    import os
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_exe(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            exe_file = os.path.join(path, program)
            if is_exe(exe_file):
                return exe_file

    return None

def get_trace(file):
    bin_path=which('trace_processor_shell')
    if bin_path:
        config = tpc(bin_path=bin_path)
        trace = tp(file_path=file, config=config)
    else:
        trace = tp(file_path=file)

    return trace
