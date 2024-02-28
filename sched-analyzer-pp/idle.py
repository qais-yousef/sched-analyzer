#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import pandas as pd
import numpy as np

query = "select ts, cpu, value as idle from counter as c left join cpu_counter_track as t on c.track_id = t.id where t.name = 'cpuidle'"


def __init():

    global df_idle
    if df_idle is None:
        df_idle = trace_idle.as_pandas_dataframe()
        df_idle.ts = df_idle.ts - df_idle.ts[0]
        df_idle.ts = df_idle.ts / 1000000000
        df_idle['_ts'] = df_idle.ts
        df_idle.set_index('ts', inplace=True)

        # This magic value is exit from idle. Values 0 and above are idle
        # states
        df_idle.idle = df_idle.infer_objects(copy=False).idle.replace(4294967295, -1)

def init(trace):

    global trace_idle
    trace_idle = trace.query(query)

    global df_idle
    df_idle = None

    __init()

def num_rows():

    return int((len(df_idle.cpu.unique()) + 3) / 4)

def save_csv(prefix):

    df_idle.to_csv(prefix + '_idle.csv')

def plot_residency_matplotlib(plt, prefix):

    func = globals()['num_rows']
    num_rows = func()
    row_pos = 1

    try:
        nr_cpus = len(df_idle.cpu.unique())

        plt.figure(figsize=(8*4,4*num_rows))

        col = 0
        df_idle_cpu = df_idle[df_idle.cpu == 0]
        for cpu in range(nr_cpus):
            df_idle_cpu = df_idle[df_idle.cpu == cpu].copy()
            df_idle_cpu['duration'] = -1 * df_idle_cpu._ts.diff(periods=-1)

            total_duration = df_idle_cpu.duration.sum()
            df_duration =  df_idle_cpu.groupby('idle').duration.sum() * 100 / total_duration

            if col == 0:
                plt.subplot(num_rows, 4, row_pos * 4 - 3)
                col = 1
            elif col == 1:
                plt.subplot(num_rows, 4, row_pos * 4 - 2)
                col = 2
            elif col == 2:
                plt.subplot(num_rows, 4, row_pos * 4 - 1)
                col = 3
            else:
                plt.subplot(num_rows, 4, row_pos * 4 - 0)
                col = 0
                row_pos += 1

            if not df_duration.empty:
                ax = df_duration.plot.bar(title='CPU{}'.format(cpu) + ' Idle residency %', alpha=0.75, color='grey')
                ax.bar_label(ax.containers[0])
                ax.set_xlabel('Idle State')
                plt.grid()

        plt.tight_layout()
        plt.savefig(prefix + '_idle.png')
    except Exception as e:
        # Most likely the trace has no idle info
        # TODO: Better detect this
        print("Error processing idle.plot_residency_matplotlib():", e)
        pass

def plot_residency_tui(plt):

    func = globals()['num_rows']
    num_rows = func()
    row_pos = 1

    try:
        nr_cpus = len(df_idle.cpu.unique())

        plt.cld()
        plt.plot_size(30*4, 10*num_rows)
        plt.subplots(num_rows, 4)

        col = 1
        df_idle_cpu = df_idle[df_idle.cpu == 0]
        for cpu in range(nr_cpus):
            df_idle_cpu = df_idle[df_idle.cpu == cpu].copy()
            df_idle_cpu['duration'] = -1 * df_idle_cpu._ts.diff(periods=-1)

            total_duration = df_idle_cpu.duration.sum()
            df_duration =  df_idle_cpu.groupby('idle').duration.sum() * 100 / total_duration

            plt.subplot(row_pos, col)
            if not df_duration.empty:
                plt.bar(df_duration.index.values, df_duration.values, width=1/5)
                plt.title('CPU' + str(cpu) + ' Idle residency %')

            col = (col + 1) % 5

            if not col:
                col = 1
                row_pos += 1

        plt.show()
    except Exception as e:
        # Most likely the trace has no idle info
        # TODO: Better detect this
        print("Error processing idle.plot_residency_tui():", e)
        pass
