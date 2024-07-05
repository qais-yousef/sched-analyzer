#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import numpy as np
import settings

start_ts_query = "SELECT start_ts FROM trace_bounds"
end_ts_query = "SELECT end_ts FROM trace_bounds"

def init(trace):
    global trace_start_ts
    global trace_end_ts

    trace_start_ts = trace.query(start_ts_query).as_pandas_dataframe().values.item()
    trace_end_ts = trace.query(end_ts_query).as_pandas_dataframe().values.item()

def convert_ts(df, reindex=False, method='ffill'):
    global trace_start_ts
    global trace_end_ts

    if df.empty:
        return df

    # Act on a copy, don't modify the original dataframe which could be a slice
    df = df.copy()

    # Upsample to insert more accurate intermediate data and match
    # trace_start/end timestamps
    if reindex:
        df['index'] = df.ts
        df.set_index('index', inplace=True)
        # Sample every 100us
        new_index = np.arange(trace_start_ts, trace_end_ts, 100 * 1000)
        df = df.reindex(new_index, method=method)
        df.ts = df.index

    # Convert to time in seconds starting from 0
    df.ts = df.ts - trace_start_ts
    df.ts = df.ts / 1000000000
    df['_ts'] = df.ts
    df.set_index('ts', inplace=True)

    # Filter timestamps based on user requested range
    df = settings.filter_ts(df)

    return df
