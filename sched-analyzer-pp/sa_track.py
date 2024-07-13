#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Qais Yousef

import pandas as pd
import settings
import utils

query = "select c.ts as ts, c.value as value, t.name as counter_name \
        from counter as c left join process_counter_track as t on c.track_id = t.id \
        left join process as p using (upid) \
        where p.name like '%sched-analyzer' and counter_name like '{}'"

def __multiply_df_tid_running(df, track):

    try:
        thread = track.split()[0].split('-')[0]
        tid = track.split()[0].split('-')[1]
        df_tid = utils.get_df_tid(thread, tid)
        if df_tid.empty:
            return df
        df = utils.multiply_df_tid_running(df, 'value', df_tid)
        return df
    except:
        return df


def init(trace, signal):

    global sa_track_signal
    sa_track_signal = signal

    global trace_sa_track
    trace_sa_track = trace.query(query.format('% {}'.format(signal)))

    global df_sa_track
    df_sa_track = trace_sa_track.as_pandas_dataframe()
    if df_sa_track.empty:
        return

def sa_track_save_csv(prefix):

    df_sa_track.to_csv(prefix + '_' + sa_track_signal + '.csv')

def plot_sa_track_matplotlib(plt, prefix, tracks=[], multiply_running=False):

    if not any(tracks):
        tracks = sorted(df_sa_track.counter_name.unique())

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
            df = utils.convert_ts(df, True)

            running_str = ''
            if multiply_running:
                df = __multiply_df_tid_running(df, track)
                running_str = ' running'

            plt.subplot(num_rows, 1, row_pos)
            row_pos += 1
            df.value.plot(title=track + running_str,
                          drawstyle='steps-post', alpha=0.75, legend=False,
                          xlim=(settings.ts_start, settings.ts_end))
            plt.grid()

    plt.tight_layout()
    plt.savefig(prefix + '_' + sa_track_signal + running_str.replace(' ', '_') + '.png')

def plot_sa_track_hist_matplotlib(plt, prefix, tracks=[], multiply_running=False):

    if not any(tracks):
        tracks = sorted(df_sa_track.counter_name.unique())

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
            df = utils.convert_ts(df, True)

            running_str = ''
            if multiply_running:
                df = __multiply_df_tid_running(df, track)
                running_str = ' running'

            plt.subplot(num_rows, 1, row_pos)
            row_pos += 1
            plt.title(track + running_str + ' Histogram')
            df.value.hist(bins=100, density=False, grid=True, alpha=0.5, legend=True)

    plt.tight_layout()
    plt.savefig(prefix + '_' + sa_track_signal + running_str.replace(' ', '_') + '_hist.png')

def plot_sa_track_tui(plt, tracks=[], multiply_running=False):

    if not any(tracks):
        tracks = sorted(df_sa_track.counter_name.unique())

    for track in tracks:
        subtracks =  df_sa_track[df_sa_track.counter_name.str.contains(track)].counter_name.unique()
        subtracks = sorted(subtracks)
        for track in subtracks:
            df = df_sa_track[df_sa_track.counter_name == track]
            df = utils.convert_ts(df, True)

            running_str = ''
            if multiply_running:
                df = __multiply_df_tid_running(df, track)
                running_str = ' running'

            plt.cld()
            plt.plot_size(settings.fig_width_tui, settings.fig_height_tui)
            plt.plot(df.index.values, df.value.values)
            plt.xfrequency(settings.xfrequency_tui)
            plt.title(track + running_str)
            plt.show()

def plot_sa_track_hist_tui(plt, tracks=[], multiply_running=False):

    if not any(tracks):
        tracks = sorted(df_sa_track.counter_name.unique())

    for track in tracks:
        subtracks =  df_sa_track[df_sa_track.counter_name.str.contains(track)].counter_name.unique()
        subtracks = sorted(subtracks)
        for track in subtracks:
            df = df_sa_track[df_sa_track.counter_name == track]
            df = utils.convert_ts(df, True)

            running_str = ''
            if multiply_running:
                df = __multiply_df_tid_running(df, track)
                running_str = ' running'

            df_hist = pd.Series(df.value.value_counts(ascending=True))

            plt.cld()
            plt.plot_size(settings.fig_width_tui, settings.fig_height_tui)
            plt.bar(df_hist.index, df_hist.values, width=1/5)
            plt.title(track + running_str + ' Histogram')
            plt.show()

def plot_sa_track_residency_tui(plt, tracks=[], multiply_running=False, abs=False):

    if not any(tracks):
        tracks = sorted(df_sa_track.counter_name.unique())

    for track in tracks:
        subtracks =  df_sa_track[df_sa_track.counter_name.str.contains(track)].counter_name.unique()
        subtracks = sorted(subtracks)
        for track in subtracks:
            df = df_sa_track[df_sa_track.counter_name == track]
            df = utils.convert_ts(df, True)

            running_str = ''
            if multiply_running:
                df = __multiply_df_tid_running(df, track)
                running_str = ' running'

            df_duration = utils.gen_df_duration_groupby(df, 'value', abs)
            if not df_duration.empty:
                print()
                plt.cld()
                plt.plot_size(settings.fig_width_tui, settings.fig_height_tui)
                plt.simple_bar(df_duration.index.values, df_duration.values, width=settings.fig_width_tui,
                        title=track + running_str + ' residency {}'.format('(ms)' if abs else '%'))
                plt.show()

def plot_sa_track_residency_abs_tui(plt, tracks=[], multiply_running=False):

    plot_sa_track_residency_tui(plt, tracks, multiply_running, True)
