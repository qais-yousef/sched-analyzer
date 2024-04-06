#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Yvonne Yip

import pandas as pd
import numpy as np

query = "select s.ts as ts, s.dur as dur, s.cpu as cpu, p.name as process_name, t.name as thread_name, t.tid as tid \
        from sched_slice as s \
        left join thread as t using(utid) \
        left join process as p using(upid) \
        where p.name like '{}'"


def __init():
    nr_top = 10

    df_top = df_sched_track.sort_values(['tid']) \
        .groupby(['tid', 'thread_name'])         \
        .dur.sum().round(2)                      \
        .sort_values(ascending=False)            \
        .head(nr_top)                            \
        .to_frame()

    global df_top_percent
    df_top_percent = pd.DataFrame()

    for tid in df_top.index.get_level_values('tid'):
        df_tid = df_sched_track[df_sched_track.tid == tid]

        total_duration = df_tid.dur.sum()

        df_tid_percent = df_tid.groupby(['tid', 'thread_name', 'cpu']).dur.sum() * 100 / total_duration
        df_tid_percent = df_tid_percent.to_frame()

        df_top_percent = pd.concat([df_top_percent, df_tid_percent])

def init(trace, process_name):
    pd.set_option('display.max_columns', None)
    pd.set_option('display.max_rows', None)
    pd.set_option('display.width', 100)

    if not any(process_name):
        return

    global sched_process_name
    sched_process_name = process_name[0]

    global sched_track
    sched_track = trace.query(query.format('%{}'.format(sched_process_name)))
    
    global df_sched_track
    df_sched_track = sched_track.as_pandas_dataframe()
    if df_sched_track.empty:
        return
    df_sched_track.ts = df_sched_track.ts - df_sched_track.ts[0]
    df_sched_track.ts = df_sched_track.ts / 1000000000
    df_sched_track.set_index('ts', inplace=True)

    __init()

def summary(plt):
    if df_top_percent.empty:
        return

    for tid in df_top_percent.index.get_level_values('tid'):
        df_tid = df_top_percent.loc[tid]

        if not df_tid.empty:
            plt.cld()
            plt.plot_size(100, 10)
            plt.bar(df_tid.index.get_level_values('cpu'), df_tid.dur, width=1/5)
            plt.title('Task ' + df_tid.index.get_level_values('thread_name')[0] + ' ' + str(tid) + ' Run residency %')
            plt.show()
    

def save_csv(prefix):
    if df_top_percent.empty:
        return
    df_top_percent.to_csv(prefix + '_run_residency_' + sched_process_name + '.csv')