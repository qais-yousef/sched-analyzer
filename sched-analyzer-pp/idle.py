#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import numpy as np
import pandas as pd
import settings

query = "select ts, cpu, value as idle from counter as c left join cpu_counter_track as t on c.track_id = t.id where t.name = 'cpuidle'"


def __init():

    global df_idle
    if df_idle is None:
        df_idle = trace_idle.as_pandas_dataframe()
        if df_idle.empty:
            return
        df_idle.ts = df_idle.ts - df_idle.ts[0]
        df_idle.ts = df_idle.ts / 1000000000
        df_idle['_ts'] = df_idle.ts
        df_idle.set_index('ts', inplace=True)

        # This magic value is exit from idle. Values 0 and above are idle
        # states
        df_idle.idle = df_idle.infer_objects(copy=False).idle.replace(4294967295, -1)

        df_idle = settings.filter_ts(df_idle)

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

    if df_idle.empty:
        return

    nr_cpus = len(df_idle.cpu.unique())

    plt.figure(figsize=(8*4,4*num_rows))

    col = 0
    df_idle_cpu = df_idle[df_idle.cpu == 0]
    for cpu in range(nr_cpus):
        df_idle_cpu = df_idle[df_idle.cpu == cpu].copy()
        df_idle_cpu['duration'] = -1 * df_idle_cpu._ts.diff(periods=-1)

        total_duration = df_idle_cpu.duration.sum()
        if not total_duration:
            total_duration = 1
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

def plot_residency_tui(plt):

    func = globals()['num_rows']
    num_rows = func()
    row_pos = 1

    if df_idle.empty:
        return

    cpus = sorted(df_idle.cpu.unique())
    nr_cpus = len(cpus)

    idle_states = sorted(df_idle.idle.unique())
    idle_pct = []
    labels = []

    for state in idle_states:
        state_pct = []
        labels.append("{}".format(state))
        for cpu in range(nr_cpus):
            df_idle_cpu = df_idle[df_idle.cpu == cpu].copy()
            df_idle_cpu['duration'] = -1 * df_idle_cpu._ts.diff(periods=-1)

            total_duration = df_idle_cpu.duration.sum()
            if not total_duration:
                total_duration = 1
            df_duration =  df_idle_cpu[df_idle_cpu.idle == state].duration.sum() * 100 / total_duration
            state_pct.append(df_duration)

        idle_pct.append(state_pct)

    print()
    plt.cld()
    plt.simple_multiple_bar(cpus, idle_pct, labels=labels, width=settings.fig_width_tui, title='CPU Idle Residency %')
    plt.show()
