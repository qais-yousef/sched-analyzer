#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import numpy as np
import pandas as pd
import settings
import utils

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

        df_freq.freq = df_freq.freq / 1000000

    __find_clusters()

def init(trace):

    global trace_freq
    global df_freq
    global clusters
    global df_states

    trace_freq = trace.query(query)
    df_freq = None
    clusters = None

    __init()
    df_states = utils.get_df_states()

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
        df_freq_cpu = utils.convert_ts(df_freq_cpu, True)

        if df_freq_cpu.empty:
            continue

        plt.subplot(num_rows, 1, row_pos)
        row_pos += 1
        df_freq_cpu.freq.plot(title='CPU' + str(cpu) + ' Frequency', alpha=0.75,
                drawstyle='steps-post', style='-', color=color[i], xlim=(settings.ts_start, settings.ts_end))
        plt.grid()

        i += 1
        if i == 3:
            i = 0

    plt.tight_layout()
    plt.savefig(prefix + '_frequency.png')

def plot_residency_matplotlib(plt, prefix, abs=False):

    color = ['b', 'y', 'r']
    i = 0

    num_rows = len(clusters)
    row_pos = 1

    if df_freq.empty:
        return

    plt.figure(figsize=(8,4*num_rows))

    for cpu in clusters:
        df_freq_cpu = df_freq[df_freq.cpu == cpu].copy()
        df_freq_cpu = utils.convert_ts(df_freq_cpu, True)

        df_duration = utils.gen_df_duration_groupby(df_freq_cpu, 'freq', abs)

        plt.subplot(num_rows, 1, row_pos)
        row_pos += 1
        if not df_duration.empty:
            ax = df_duration.plot.bar(title='CPU' + str(cpu) + ' Frequency residency {}'.format('(ms)' if abs else '%'), alpha=0.75, color=color[i])
            ax.bar_label(ax.containers[0])
            plt.grid()

        i += 1
        if i == 3:
            i = 0

    plt.tight_layout()
    plt.savefig(prefix + '_frequency_residency.png')

def plot_residency_abs_matplotlib(plt, prefix):

    plot_residency_matplotlib(plt, prefix, True)

def plot_tui(plt):

    if df_freq.empty:
        return

    for cpu in clusters:
        df_freq_cpu = df_freq[df_freq.cpu == cpu].copy()
        df_freq_cpu = utils.convert_ts(df_freq_cpu, True)

        if not df_freq_cpu.empty:
            print()
            plt.cld()
            plt.plot_size(settings.fig_width_tui, settings.fig_height_tui)
            plt.plot(df_freq_cpu.index.values, df_freq_cpu.freq.values)
            plt.title('CPU' + str(cpu) + ' Frequency')
            plt.show()

def plot_residency_tui(plt, abs=False):

    if df_freq.empty:
        return

    for cpu in clusters:
        df_freq_cpu = df_freq[df_freq.cpu == cpu].copy()
        df_freq_cpu = utils.convert_ts(df_freq_cpu, True)

        df_duration = utils.gen_df_duration_groupby(df_freq_cpu, 'freq', abs)
        if not df_duration.empty:
            print()
            plt.cld()
            plt.plot_size(settings.fig_width_tui, settings.fig_height_tui)
            plt.simple_bar(df_duration.index.values, df_duration.values, width=settings.fig_width_tui, title='CPU' + str(cpu) + ' Frequency residency {}'.format('(ms)' if abs else '%'))
            plt.show()

def plot_residency_abs_tui(plt):

    plot_residency_tui(plt, True)

def plot_task_tui(plt, threads=[]):

    if df_freq.empty:
        return

    if df_states.empty:
        return

    for thread in threads:
        df = df_states[df_states.name.str.contains(thread).fillna(False)]

        for thread in sorted(df.name.unique()):
            df_thread = df[df.name == thread]

            for tid in sorted(df_thread.tid.unique()):
                df_tid = df_thread[df_thread.tid == tid]
                df_tid_running = df_tid[df_tid.state == 'Running']

                for cpu in sorted(df_tid_running.cpu.unique()):
                    df_freq_cpu = df_freq[df_freq.cpu == cpu].copy()
                    df_freq_cpu = utils.convert_ts(df_freq_cpu, True)

                    df_tid_cpu = df_tid[(df_tid.cpu == cpu) | (df_tid.cpu.isna())].copy()

                    df_freq_cpu = utils.multiply_df_tid_running(df_freq_cpu, 'freq', df_tid_cpu)

                    if not df_freq_cpu.empty:
                        print()
                        plt.cld()
                        plt.plot_size(settings.fig_width_tui, settings.fig_height_tui)
                        plt.plot(df_freq_cpu.index.values, df_freq_cpu.freq.values)
                        plt.title('{} {}'.format(tid, thread) + ' CPU' + str(cpu) + ' Frequency')
                        plt.show()

def plot_task_residency_tui(plt, threads=[], abs=False):

    if df_freq.empty:
        return

    if df_states.empty:
        return

    for thread in threads:
        df = df_states[df_states.name.str.contains(thread).fillna(False)]

        for thread in sorted(df.name.unique()):
            df_thread = df[df.name == thread]

            for tid in sorted(df_thread.tid.unique()):
                df_tid = df_thread[df_thread.tid == tid]
                df_tid_running = df_tid[df_tid.state == 'Running']

                for cpu in sorted(df_tid_running.cpu.unique()):
                    df_freq_cpu = df_freq[df_freq.cpu == cpu].copy()
                    df_freq_cpu = utils.convert_ts(df_freq_cpu, True)

                    df_tid_cpu = df_tid[(df_tid.cpu == cpu) | (df_tid.cpu.isna())].copy()

                    df_freq_cpu = utils.multiply_df_tid_running(df_freq_cpu, 'freq', df_tid_cpu)

                    df_duration = utils.gen_df_duration_groupby(df_freq_cpu, 'freq', abs)
                    if not df_duration.empty:
                        print()
                        plt.cld()
                        plt.plot_size(settings.fig_width_tui, settings.fig_height_tui)
                        plt.simple_bar(df_duration.index.values, df_duration.values, width=settings.fig_width_tui,
                                title='{} {}'.format(tid, thread) + ' CPU' + str(cpu) + ' Frequency residency {}'.format('(ms)' if abs else '%'))
                        plt.show()

def plot_task_residency_abs_tui(plt, threads=[]):

    plot_task_residency_tui(plt, threads, True)
