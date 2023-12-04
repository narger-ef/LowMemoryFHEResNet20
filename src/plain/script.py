import sys

if len(sys.argv) == 1:
    print('Launch this script followed by a filename (e.g. \'plain.py "../inputs/luis.png"\')')
    exit(0)

import torch
from torchvision import transforms
from PIL import Image
import numpy as np

model = torch.hub.load("chenyaofo/pytorch-cifar-models", "cifar10_resnet20", pretrained = True, verbose=False)
model.eval()

img = Image.open(sys.argv[1])
convert_tensor = transforms.ToTensor()
img = convert_tensor(img)
img = img.unsqueeze(0)

np.set_printoptions(precision=3)
np.set_string_function(lambda x: repr(x), repr=False)

result = model(img)

result_list = my_formatted_list = list(np.around(result[0].detach().numpy(),3))

print("Plain:  " + str(result_list))