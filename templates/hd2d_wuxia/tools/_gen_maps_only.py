import os
import sys
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
os.chdir(os.path.dirname(here))
from gen_maps import generate_maps
generate_maps(".")
print("maps regenerated")