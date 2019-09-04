import math
import numpy as np


def make_grid(tensor, nrow=8, padding=2):
    nmaps = tensor.shape[0]
    xmaps = min(nrow, nmaps)
    ymaps = int(math.ceil(float(nmaps) / xmaps))
    height, width = int(tensor.shape[2] + padding), int(tensor.shape[3] + padding)

    grid = np.zeros((3, height * ymaps + padding, width * xmaps + padding), dtype=np.uint8)
    k = 0
    for y in range(ymaps):
        for x in range(xmaps):
            if k >= nmaps:
                break
            grid[:,
            y * height + padding : y * height + padding + height - padding,
            x * width + padding : x * width + padding + width - padding] = tensor[k]
            k = k + 1
    return np.transpose(grid, (1, 2, 0))


def display_grid(tensor, nrow=8, padding=2):
    import IPython.display as display
    import PIL.Image
    display.display(PIL.Image.fromarray(make_grid(tensor, nrow, padding)))
