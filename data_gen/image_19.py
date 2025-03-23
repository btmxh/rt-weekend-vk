from .data_gen import RenderData
from math import pi, cos

data = RenderData()

R = cos(pi / 4)

mat_left = data.add_lambert((0.0, 0.0, 1.0))
mat_right = data.add_lambert((1.0, 0.0, 0.0))

data.add_sphere((-R, 0.0, -1.0, R), mat_left)
data.add_sphere((R, 0.0, -1.0, R), mat_right)

data.serialize("render.dat")
