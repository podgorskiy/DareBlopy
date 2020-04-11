# Copyright 2019-2020 Stanislav Pidhorskyi
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================


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
            grid[:, y * height + padding : y * height + padding + height - padding,
                    x * width + padding : x * width + padding + width - padding] = tensor[k]
            k = k + 1
    return np.transpose(grid, (1, 2, 0))


def display_grid(tensor, nrow=8, padding=2):
    import IPython.display as display
    import PIL.Image
    display.display(PIL.Image.fromarray(make_grid(tensor, nrow, padding)))
