import time
import matplotlib.pyplot as plt
import numpy as np


class Benchmark:
    def __init__(self):
        self.schedule = []

    def add(self, name, baseline, dareblopy, dareblopy_turbo=None, prehit=()):
        self.schedule.append((name, baseline, dareblopy, dareblopy_turbo, prehit))

    def run(self):
        results = []
        for name, baseline, dareblopy, dareblopy_turbo, prehit in self.schedule:
            print('\n' + '#' * 80)
            print('Benchmarking: %s' % name)
            print('#' * 80)
            if prehit:
                prehit()
            print('With native python:')
            baseline = timeit(baseline)
            average_time_for_baseline = baseline()

            print('With DareBlopy: ')
            dareblopy = timeit(dareblopy)
            average_time_for_dareblopy = dareblopy()
            average_time_for_dareblopy_turbo = average_time_for_dareblopy
            if dareblopy_turbo:
                print('With DareBlopy (turbo): ')
                dareblopy_turbo = timeit(dareblopy_turbo)
                average_time_for_dareblopy_turbo = dareblopy_turbo()

            results.append((name,
                            average_time_for_baseline,
                            average_time_for_dareblopy,
                            average_time_for_dareblopy_turbo))

        x = np.arange(len(results))
        width = 0.2

        has_turbo = np.asarray([(x[2] != x[3]) for x in results])

        fig, ax = plt.subplots(figsize=(12, 6), dpi=120, facecolor='w', edgecolor='k')

        rects1 = ax.bar(x - width * 1.0 + 0.5 * width * np.logical_not(has_turbo), [x[1] for x in results], width, label='Python + zipfile + PIL + numpy')
        rects2 = ax.bar(x - width * 0.0 + 0.5 * width * np.logical_not(has_turbo), [x[2] for x in results], width, label='Python + DareBlopy')
        rects3 = ax.bar(x + width * has_turbo, [x[3] for x in results] * has_turbo, width, label='Python + DareBlopy-turbo')

        # Add some text for labels, title and custom x-axis tick labels, etc.
        ax.set_ylabel('Running time, [ms]. Lowe is better')
        ax.set_title('Running time of DareBlopy and equivalent python code')
        ax.set_xticks(x)
        ax.set_xticklabels([x[0] for x in results])
        ax.legend(loc=4)

        def autolabel(rects, show=None):
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

        autolabel(rects1)
        autolabel(rects2)
        autolabel(rects3, has_turbo)

        fig.tight_layout()

        fig.savefig('test_utils/benchmark_results.png')


def timeit(method):
    def timed(*args, **kw):
        ds = 0.0
        trials = 6
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
