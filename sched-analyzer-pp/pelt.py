#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import text

query = "select c.ts as ts, c.value as value, t.name as counter_name \
        from counter as c left join process_counter_track as t on c.track_id = t.id \
        left join process as p using (upid) \
        where p.name like '%sched-analyzer' and counter_name like '{}'"

query_util_avg = query.format('% util_avg')


def init(trace):

    global trace_util_avg
    trace_util_avg = trace.query(query_util_avg)

    global df_util_avg
    df_util_avg = trace_util_avg.as_pandas_dataframe()
    df_util_avg.ts = df_util_avg.ts - df_util_avg.ts[0]
    df_util_avg.ts = df_util_avg.ts / 1000000000
    df_util_avg.set_index('ts', inplace=True)

def util_avg_save_csv(prefix):

    df_util_avg.to_csv(prefix + '_util_avg.csv')

def plot_util_avg_matplotlib(plt, prefix, tracks=[]):

    if not any(tracks):
        tracks = df_util_avg.counter_name.unique()

    num_rows = 0
    for track in tracks:
        subtracks =  df_util_avg[df_util_avg.counter_name.str.contains(track)].counter_name.unique()
        num_rows += len(subtracks)
    row_pos = 1

    if not num_rows:
        return

    plt.figure(figsize=(8,4*num_rows))

    for track in tracks:
        subtracks =  df_util_avg[df_util_avg.counter_name.str.contains(track)].counter_name.unique()
        subtracks = sorted(subtracks)
        for track in subtracks:
            df = df_util_avg[df_util_avg.counter_name == track]

            plt.subplot(num_rows, 1, row_pos)
            row_pos += 1
            df.value.plot(title=track,
                          drawstyle='steps-post', alpha=0.75, legend=True)
            plt.grid()

    plt.tight_layout()
    plt.savefig(prefix + '_util_avg.png')

def plot_util_avg_hist_matplotlib(plt, prefix, tracks=[]):

    if not any(tracks):
        tracks = df_util_avg.counter_name.unique()

    num_rows = 0
    for track in tracks:
        subtracks =  df_util_avg[df_util_avg.counter_name.str.contains(track)].counter_name.unique()
        num_rows += len(subtracks)
    row_pos = 1

    if not num_rows:
        return

    plt.figure(figsize=(8,4*num_rows))

    for track in tracks:
        subtracks =  df_util_avg[df_util_avg.counter_name.str.contains(track)].counter_name.unique()
        subtracks = sorted(subtracks)
        for track in subtracks:
            df = df_util_avg[df_util_avg.counter_name == track]

            plt.subplot(num_rows, 1, row_pos)
            row_pos += 1
            plt.title(track + ' Histogram')
            df.value.hist(bins=100, density=False, grid=True, alpha=0.5, legend=True)

    plt.tight_layout()
    plt.savefig(prefix + '_util_avg_hist.png')

def plot_util_avg_tui(plt, tracks=[]):

    if not any(tracks):
        tracks = df_util_avg.counter_name.unique()

    for track in tracks:
        subtracks =  df_util_avg[df_util_avg.counter_name.str.contains(track)].counter_name.unique()
        subtracks = sorted(subtracks)
        for track in subtracks:
            df = df_util_avg[df_util_avg.counter_name == track]

            plt.cld()
            plt.plot_size(100, 10)
            plt.plot(df.index.values, df.value.values)
            plt.title(track + ' util_avg')
            plt.show()

def plot_util_avg_hist_tui(plt, tracks=[]):

    if not any(tracks):
        tracks = df_util_avg.counter_name.unique()

    for track in tracks:
        subtracks =  df_util_avg[df_util_avg.counter_name.str.contains(track)].counter_name.unique()
        subtracks = sorted(subtracks)
        for track in subtracks:
            df = df_util_avg[df_util_avg.counter_name == track]

            df_hist = pd.Series(df.value.value_counts(ascending=True))

            plt.cld()
            plt.plot_size(100, 10)
            plt.bar(df_hist.index, df_hist.values, width=1/5)
            plt.title(track + ' util_avg Histogram')
            plt.show()
