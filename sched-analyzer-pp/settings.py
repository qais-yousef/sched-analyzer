#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import pandas as pd

def init():
    global ts_start
    global ts_end

    ts_start = 0
    ts_end = -1 + 2**64

def set_ts_start(ts):
    global ts_start

    ts_start = ts

def set_ts_end(ts):
    global ts_end

    ts_end = ts

def filter_ts(df):
    global ts_start
    global ts_end

    return df[(df.index >= ts_start) & (df.index <= ts_end)]
