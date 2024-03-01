#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import pandas as pd
import numpy as np

query = "select ts, cpu, value as freq from counter as c left join cpu_counter_track as t on c.track_id = t.id where t.name = 'cpufreq'"

def __find_clusters():

    global clusters
    if clusters:
        return

    nr_cpus = len(df_freq.cpu.unique())

    clusters = [0]
    df_freq_cpu = df_freq[df_freq.cpu == 0]
    for cpu in range(nr_cpus):
        df_freq_next_cpu = df_freq[df_freq.cpu == cpu]

        if np.array_equal(df_freq_cpu.freq.values, df_freq_next_cpu.freq.values):
            continue

        df_freq_cpu = df_freq[df_freq.cpu == cpu]
        clusters.append(cpu)

def __init():

    global df_freq
    if df_freq is None:
        df_freq = trace_freq.as_pandas_dataframe()
        if df_freq.empty:
            return
        df_freq.ts = df_freq.ts - df_freq.ts[0]
        df_freq.ts = df_freq.ts / 1000000000
        df_freq['_ts'] = df_freq.ts
        df_freq.freq = df_freq.freq / 1000000
        df_freq.set_index('ts', inplace=True)

    __find_clusters()

def init(trace):

    global trace_freq
    global df_freq
    global clusters

    trace_freq = trace.query(query)
    df_freq = None
    clusters = None

    __init()

def save_csv(prefix):

    df_freq.to_csv(prefix + '_freq.csv')

def plot_matplotlib(plt, prefix):

    color = ['b', 'y', 'r']
    i = 0

    num_rows = len(clusters)
    row_pos = 1

    if df_freq.empty:
        return

    plt.figure(figsize=(8,4*num_rows))

    for cpu in clusters:
        df_freq_cpu = df_freq[df_freq.cpu == cpu].copy()
        df_freq_cpu['duration'] = -1 * df_freq_cpu._ts.diff(periods=-1)

        total_duration = df_freq_cpu.duration.sum()
        df_duration =  df_freq_cpu.groupby('freq').duration.sum() * 100 / total_duration

        plt.subplot(num_rows, 1, row_pos)
        row_pos += 1
        df_freq_cpu.freq.plot(title='CPU' + str(cpu) + ' Frequency', alpha=0.75, drawstyle='steps-post', style='-', color=color[i], xlim=(df_freq.index[0], df_freq.index[-1]))
        plt.grid()

        i += 1
        if i == 3:
            i = 0

    plt.tight_layout()
    plt.savefig(prefix + '_frequency.png')

def plot_residency_matplotlib(plt, prefix):

    color = ['b', 'y', 'r']
    i = 0

    num_rows = len(clusters)
    row_pos = 1

    if df_freq.empty:
        return

    plt.figure(figsize=(8,4*num_rows))

    for cpu in clusters:
        df_freq_cpu = df_freq[df_freq.cpu == cpu].copy()
        df_freq_cpu['duration'] = -1 * df_freq_cpu._ts.diff(periods=-1)

        total_duration = df_freq_cpu.duration.sum()
        df_duration =  df_freq_cpu.groupby('freq').duration.sum() * 100 / total_duration

        plt.subplot(num_rows, 1, row_pos)
        row_pos += 1
        if not df_duration.empty:
            ax = df_duration.plot.bar(title='CPU' + str(cpu) + ' Frequency residency %', alpha=0.75, color=color[i])
            ax.bar_label(ax.containers[0])
            plt.grid()

        i += 1
        if i == 3:
            i = 0

    plt.tight_layout()
    plt.savefig(prefix + '_frequency_residency.png')

def plot_tui(plt):

    if df_freq.empty:
        return

    for cpu in clusters:
        df_freq_cpu = df_freq[df_freq.cpu == cpu].copy()
        df_freq_cpu['duration'] = -1 * df_freq_cpu._ts.diff(periods=-1)

        total_duration = df_freq_cpu.duration.sum()
        df_duration =  df_freq_cpu.groupby('freq').duration.sum() * 100 / total_duration

        if not df_duration.empty:
            plt.cld()
            plt.plot_size(100, 10)
            plt.plot(df_freq_cpu.index.values, df_freq_cpu.freq.values)
            plt.title('CPU' + str(cpu) + ' Frequency')
            plt.show()

def plot_residency_tui(plt):

    if df_freq.empty:
        return

    for cpu in clusters:
        df_freq_cpu = df_freq[df_freq.cpu == cpu].copy()
        df_freq_cpu['duration'] = -1 * df_freq_cpu._ts.diff(periods=-1)

        total_duration = df_freq_cpu.duration.sum()
        df_duration =  df_freq_cpu.groupby('freq').duration.sum() * 100 / total_duration

        if not df_duration.empty:
            plt.cld()
            plt.plot_size(100, 10)
            plt.bar(df_duration.index.values, df_duration.values, width=1/5)
            plt.title('CPU' + str(cpu) + ' Frequency residency %')
            plt.show()
