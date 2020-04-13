import time
import matplotlib.pyplot as plt
import numpy as np
import gc


class Benchmark:
    def __init__(self):
        self.schedule = []

    def add(self, name, baseline, dareblopy, dareblopy_turbo=None, preheat=()):
        self.schedule.append((name, baseline, dareblopy, dareblopy_turbo, preheat))

    def run(self, title, label_baseline, output_file, loc, figsize=(12, 6), caption=None):
        locmap = {'ul': 2, 'ur': 1, 'll': 3, 'lr': 4}
        if loc in locmap:
            loc = locmap[loc]

        results = []
        for name, baseline, dareblopy, dareblopy_turbo, preheat in self.schedule:
            print('\n' + '#' * 80)
            print('Benchmarking: %s' % name)
            print('#' * 80)
            if preheat:
                preheat()
            print('With native python:')
            baseline = timeit(baseline)
            gc.collect()
            time.sleep(0.1)
            average_time_for_baseline = baseline()

            print('With DareBlopy: ')
            dareblopy = timeit(dareblopy)
            gc.collect()
            time.sleep(0.1)
            average_time_for_dareblopy = dareblopy()
            average_time_for_dareblopy_turbo = average_time_for_dareblopy
            if dareblopy_turbo:
                print('With DareBlopy (turbo): ')
                dareblopy_turbo = timeit(dareblopy_turbo)
                gc.collect()
                time.sleep(0.1)
                average_time_for_dareblopy_turbo = dareblopy_turbo()

            results.append((name,
                            average_time_for_baseline,
                            average_time_for_dareblopy,
                            average_time_for_dareblopy_turbo))

        x = np.arange(len(results))
        width = 0.2

        has_turbo = np.asarray([(x[2] != x[3]) for x in results])

        fig, ax = plt.subplots(figsize=figsize, dpi=120, facecolor='w', edgecolor='k')

        rects1 = ax.bar(x - width * 1.0 + 0.5 * width * np.logical_not(has_turbo), [x[1] for x in results], width, label=label_baseline)
        rects2 = ax.bar(x - width * 0.0 + 0.5 * width * np.logical_not(has_turbo), [x[2] for x in results], width, label='DareBlopy')
        if np.any(has_turbo):
            rects3 = ax.bar(x + width * has_turbo, [x[3] for x in results] * has_turbo, width, label='DareBlopy +libjpeg-turbo')

        # Add some text for labels, title and custom x-axis tick labels, etc.
        ax.set_ylabel('Running time, [ms]. Lower is better')
        ax.set_title(title)
        ax.set_xticks(x)
        ax.set_xticklabels([x[0] for x in results])
        ax.legend(loc=loc)

        if caption:
            fig.text(0.5, 0.03, caption, wrap=True, horizontalalignment='center', fontsize=12)
            plt.subplots_adjust(left=.1, right=0.9, top=0.92, bottom=0.22)

        autolabel(ax, rects1)
        autolabel(ax, rects2)
        if np.any(has_turbo):
            autolabel(ax, rects3, has_turbo)
        fig.savefig(output_file)


def timeit(method):
    def timed(*args, **kw):
        ds = 0.0
        trials = 10
        # preheat
        method(*args, **kw)

        for i in range(trials):
            ts = time.time()
            method(*args, **kw)
            te = time.time()
            delta = te - ts
            ds += delta
            print('%r  %2.2f ms' % (method.__name__, delta * 1000))
        ds /= trials
        print(method.__name__)
        print('Total: %r  %2.2f ms' % (method.__name__, ds * 1000))
        return ds * 1000
    return timed


def autolabel(ax, rects, show=None):
    if show is None:
        show = np.ones(len(rects))
    for rect, s in zip(rects, show):
        if s:
            height = rect.get_height()
            ax.annotate('{:.2f}'.format(height),
                        xy=(rect.get_x() + rect.get_width() / 2, height),
                        xytext=(0, 3),
                        textcoords="offset points",
                        ha='center', va='bottom')


def do_simple_plot(results, figsize, title, output_file):
    fig, ax = plt.subplots(figsize=figsize, dpi=120, facecolor='w', edgecolor='k')
    x = np.arange(len(results))
    width = 0.2
    rects1 = ax.bar(x, [x[0] for x in results], width)
    # Add some text for labels, title and custom x-axis tick labels, etc.
    ax.set_ylabel('Running time, [ms]. Lower is better')
    ax.set_title(title)
    ax.set_xticks(x)
    ax.set_xticklabels([x[1] for x in results])

    autolabel(ax, rects1)
    fig.tight_layout()

    fig.savefig(output_file)
