from .data_gen import RenderData

data = RenderData()

mat_ground = data.add_lambert((0.8, 0.8, 0.0))
mat_center = data.add_lambert((0.1, 0.2, 0.5))
mat_left = data.add_dielectric(1.33**-1)
mat_right = data.add_metal((0.8, 0.6, 0.2), fuzz=1.0)

data.add_sphere((0.0, -100.5, -1.0, 100.0), mat_ground)
data.add_sphere((0.0, 0.0, -1.2, 0.5), mat_center)
data.add_sphere((-1.0, 0.0, -1.0, 0.5), mat_left)
data.add_sphere((1.0, 0.0, -1.0, 0.5), mat_right)

data.serialize("render.dat")
