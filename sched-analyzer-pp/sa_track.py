#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import pandas as pd

query = "select c.ts as ts, c.value as value, t.name as counter_name \
        from counter as c left join process_counter_track as t on c.track_id = t.id \
        left join process as p using (upid) \
        where p.name like '%sched-analyzer' and counter_name like '{}'"


def init(trace, signal):

    global sa_track_signal
    sa_track_signal = signal

    global trace_sa_track
    trace_sa_track = trace.query(query.format('% {}'.format(signal)))

    global df_sa_track
    df_sa_track = trace_sa_track.as_pandas_dataframe()
    if df_sa_track.empty:
        return
    df_sa_track.ts = df_sa_track.ts - df_sa_track.ts[0]
    df_sa_track.ts = df_sa_track.ts / 1000000000
    df_sa_track.set_index('ts', inplace=True)

def sa_track_save_csv(prefix):

    df_sa_track.to_csv(prefix + '_' + sa_track_signal + '.csv')

def plot_sa_track_matplotlib(plt, prefix, tracks=[]):

    if not any(tracks):
        tracks = df_sa_track.counter_name.unique()

    num_rows = 0
    for track in tracks:
        subtracks =  df_sa_track[df_sa_track.counter_name.str.contains(track)].counter_name.unique()
        num_rows += len(subtracks)
    row_pos = 1

    if not num_rows:
        return

    plt.figure(figsize=(8,4*num_rows))

    for track in tracks:
        subtracks =  df_sa_track[df_sa_track.counter_name.str.contains(track)].counter_name.unique()
        subtracks = sorted(subtracks)
        for track in subtracks:
            df = df_sa_track[df_sa_track.counter_name == track]

            plt.subplot(num_rows, 1, row_pos)
            row_pos += 1
            df.value.plot(title=track,
                          drawstyle='steps-post', alpha=0.75, legend=True)
            plt.grid()

    plt.tight_layout()
    plt.savefig(prefix + '_' + sa_track_signal + '.png')

def plot_sa_track_hist_matplotlib(plt, prefix, tracks=[]):

    if not any(tracks):
        tracks = df_sa_track.counter_name.unique()

    num_rows = 0
    for track in tracks:
        subtracks =  df_sa_track[df_sa_track.counter_name.str.contains(track)].counter_name.unique()
        num_rows += len(subtracks)
    row_pos = 1

    if not num_rows:
        return

    plt.figure(figsize=(8,4*num_rows))

    for track in tracks:
        subtracks =  df_sa_track[df_sa_track.counter_name.str.contains(track)].counter_name.unique()
        subtracks = sorted(subtracks)
        for track in subtracks:
            df = df_sa_track[df_sa_track.counter_name == track]

            plt.subplot(num_rows, 1, row_pos)
            row_pos += 1
            plt.title(track + ' Histogram')
            df.value.hist(bins=100, density=False, grid=True, alpha=0.5, legend=True)

    plt.tight_layout()
    plt.savefig(prefix + '_' + sa_track_signal + '_hist.png')

def plot_sa_track_tui(plt, tracks=[]):

    if not any(tracks):
        tracks = df_sa_track.counter_name.unique()

    for track in tracks:
        subtracks =  df_sa_track[df_sa_track.counter_name.str.contains(track)].counter_name.unique()
        subtracks = sorted(subtracks)
        for track in subtracks:
            df = df_sa_track[df_sa_track.counter_name == track]

            plt.cld()
            plt.plot_size(100, 10)
            plt.plot(df.index.values, df.value.values)
            plt.title(track)
            plt.show()

def plot_sa_track_hist_tui(plt, tracks=[]):

    if not any(tracks):
        tracks = df_sa_track.counter_name.unique()

    for track in tracks:
        subtracks =  df_sa_track[df_sa_track.counter_name.str.contains(track)].counter_name.unique()
        subtracks = sorted(subtracks)
        for track in subtracks:
            df = df_sa_track[df_sa_track.counter_name == track]

            df_hist = pd.Series(df.value.value_counts(ascending=True))

            plt.cld()
            plt.plot_size(100, 10)
            plt.bar(df_hist.index, df_hist.values, width=1/5)
            plt.title(track + ' Histogram')
            plt.show()
