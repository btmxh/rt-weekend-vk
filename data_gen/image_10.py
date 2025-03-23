from .data_gen import RenderData, vec3

data = RenderData()
lam01 = data.add_lambert(vec3(0.5))
data.add_sphere((0.0, 0.0, -1.0, 0.5), lam01)
data.add_sphere((0.0, -100.5, -1.0, 100.0), lam01)

data.serialize("render.dat")
