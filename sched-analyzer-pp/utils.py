#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import numpy as np
import settings

start_ts_query = "SELECT start_ts FROM trace_bounds"
end_ts_query = "SELECT end_ts FROM trace_bounds"

def init_states(trace):

    query = "select ts, cpu, state, dur, tid, t.name, p.name as parent \
            from thread_state \
            left join thread as t using(utid) \
            left join process as p using(upid)"

    global trace_states
    trace_states = trace.query(query)

    global df_states
    df_states = trace_states.as_pandas_dataframe()
    if df_states.empty:
        return

def init(trace):
    global trace_start_ts
    global trace_end_ts

    trace_start_ts = trace.query(start_ts_query).as_pandas_dataframe().values.item()
    trace_end_ts = trace.query(end_ts_query).as_pandas_dataframe().values.item()

    init_states(trace)

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
        df = df[~df.index.duplicated()].reindex(new_index, method=method)
        df.ts = df.index

    # Convert to time in seconds starting from 0
    df.ts = df.ts - trace_start_ts
    df.ts = df.ts / 1000000000
    df['_ts'] = df.ts
    df.set_index('ts', inplace=True)

    # Filter timestamps based on user requested range
    df = settings.filter_ts(df)

    if df.dropna().empty:
        return df.dropna()

    return df

def get_df_states():
    global df_states

    return df_states

def get_df_tid(thread, tid):
    global df_states

    df = df_states[df_states.name.str.contains(thread).fillna(False)]
    df_thread = df[df.name == thread]
    df_tid = df_thread[df_thread.tid == int(tid)]

    return df_tid.copy()

#
# Returns df[col] * df_tid['Running'] to obtain a new df that is values of
# df[col] when the tid is RUNNING
#
# df must have been converted with utils.convert_ts()
#
def multiply_df_tid_running(df, col, df_tid):
    global trace_start_ts

    if df.empty or df_tid.empty:
        return df

    # Save last index to chop off after reindexing with ffill
    last_ts = (df_tid.ts.iloc[-1] - trace_start_ts)/1000000000
    df_tid = convert_ts(df_tid, True)
    # Chop of ffilled values after last_ts
    df_tid.loc[df_tid.index > last_ts, 'dur'] = None
    # Convert all positive values to 1 for multiply with freq
    df_tid.loc[df_tid.dur > 0, 'dur'] = 1
    # Drop duration for !Running so we account for freq during
    # Running time only
    df_tid.loc[df_tid.state != 'Running', 'dur'] = None

    # Now we'll get the df[col] seen by the task on @cpu
    df[col] = df[col] * df_tid.dur

    if df.dropna().empty:
        return df.dropna()

    return df

#
# Helper function to create 'residency' of @df grouped by @col
#
def gen_df_duration_groupby(df, col, abs=False):
    if df.empty or df.dropna().empty:
        return df

    df['duration'] = -1 * df._ts.diff(periods=-1)
    # dropna to get accurate total_duration
    total_duration = df.dropna().duration.sum()
    if not total_duration:
        total_duration = 1
    if abs:
        total_duration = 0.1 # Convert to absolute value in ms
    df_duration =  df.groupby(col).duration.sum() * 100 / total_duration

    return df_duration

#
# Helper function to create 'residency' of @df filtered by @filter
#
def gen_df_duration_filtered(df, filter, abs=False):
    if df.empty or df.dropna().empty:
        return df

    df['duration'] = -1 * df._ts.diff(periods=-1)
    # dropna to get accurate total_duration
    total_duration = df.dropna().duration.sum()
    if not total_duration:
        total_duration = 1
    if abs:
        total_duration = 0.1 # Convert to absolute value in ms
    df_duration =  df[filter].duration.sum() * 100 / total_duration

    return df_duration
