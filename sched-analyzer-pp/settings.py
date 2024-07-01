#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import pandas as pd

start_ts_query = "SELECT start_ts FROM trace_bounds"
end_ts_query = "SELECT end_ts FROM trace_bounds"

def init(trace):
    global trace_start
    global trace_end

    global ts_start
    global ts_end

    global fig_width_tui
    global fig_height_tui

    trace_start = trace.query(start_ts_query).as_pandas_dataframe().values.item()
    trace_end = trace.query(end_ts_query).as_pandas_dataframe().values.item()

    ts_start = 0
    ts_end = (trace_end - trace_start)/1000000000

    fig_width_tui = 100
    fig_height_tui = 10

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

def set_fig_width_tui(width):
    global fig_width_tui

    fig_width_tui = width

def set_fig_height_tui(height):
    global fig_height_tui

    fig_height_tui = height
