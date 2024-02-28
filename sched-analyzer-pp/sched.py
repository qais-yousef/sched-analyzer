#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import pandas as pd


def init_states(trace):

    query = "select ts, cpu, state, dur, tid, name \
            from thread_state left join thread using(utid)"

    global trace_states
    trace_states = trace.query(query)

    global df_states
    df_states = trace_states.as_pandas_dataframe()
    if df_states.empty:
        return
    df_states.ts = df_states.ts - df_states.ts[0]
    df_states.ts = df_states.ts / 1000000000
    df_states.dur = df_states.dur.astype(float) / 1000
    df_states.set_index('ts', inplace=True)

def init(trace):

    pd.set_option('display.max_columns', None)
    pd.set_option('display.max_rows', None)
    pd.set_option('display.width', 100)

    init_states(trace)

def states_summary(plt, threads=[]):

    for thread in threads:
        df = df_states[df_states.name.str.contains(thread)]

        for thread in df.name.unique():

            print()
            print("--::", thread, "::--")
            print("+"*100)

            for tid in df.tid.unique():
                df_tid = df[df.tid == tid]
                df_tid_running = df_tid[df_tid.state == 'Running']

                print()
                states = sorted(df_tid.state.unique())
                if 'S' in states:
                    states.remove('S')
                data = []
                for state in states:
                    data.append([df_tid[df_tid.state == state].dur.sum()])

                plt.clf()
                plt.cld()
                plt.simple_stacked_bar([tid], data, width=100, labels=states)
                plt.show()

                print()
                cpus = sorted(df_tid_running.cpu.unique())
                labels = ['CPU{}'.format(cpu) for cpu in cpus]
                data = []
                for cpu in cpus:
                    df_cpu = df_tid_running[df_tid_running.cpu == cpu]
                    data.append([df_cpu.dur.sum()])

                plt.clf()
                plt.cld()
                plt.simple_stacked_bar([tid], data, width=100, labels=labels)
                plt.show()

                print("-"*100)

            df_runnable = df[(df.state == 'R') | (df.state == 'R+')]
            df_running = df[df.state == 'Running']
            df_sleeping = df[df.state == 'S']
            df_usleep = df[df.state == 'D']

            if not df_runnable.empty:
                print()
                print("Runnable Time(us):")
                print("-"*100)
                print(df_runnable.groupby(['tid']).dur.describe(percentiles=[.75, .90, .95, .99]).round(2))

            if not df_running.empty:
                print()
                print("Running Time(us):")
                print("-"*100)
                print(df_running.groupby(['tid']).dur.describe(percentiles=[.75, .90, .95, .99]).round(2))

            if not df_usleep.empty:
                print()
                print("Uninterruptible Sleep Time(us):")
                print("-"*100)
                print(df_usleep.groupby(['tid']).dur.describe(percentiles=[.75, .90, .95, .99]).round(2))

def states_save_csv(prefix):

    df_states.to_csv(prefix + '_sched_states.csv')
