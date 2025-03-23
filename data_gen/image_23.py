from .data_gen import RenderData
from math import sqrt
from random import choices, random


def random_color(min: float = 0.0, max: float = 1.0) -> tuple[float, float, float]:
    return (
        random() * (max - min) + min,
        random() * (max - min) + min,
        random() * (max - min) + min,
    )


def mul_tuple(
    a: tuple[float, float, float], b: tuple[float, float, float]
) -> tuple[float, float, float]:
    return (a[0] * b[0], a[1] * b[1], a[2] * b[2])


def sub_tuple(
    a: tuple[float, float, float], b: tuple[float, float, float]
) -> tuple[float, float, float]:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def length(a: tuple[float, float, float]) -> float:
    return sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


data = RenderData()
k = 3

for a in range(-k, k + 1):
    for b in range(-k, k + 1):
        center_x = float(a) + 0.9 * random()
        center_y = 0.2
        center_z = float(b) + 0.9 * random()
        radius = 0.2

        sphere = (center_x, center_y, center_z, radius)
        if length(sub_tuple((center_x, center_y, center_z), (4.0, 0.2, 0.0))) <= 0.9:
            continue

        match choices(["lambert", "dielectric", "metal"], weights=[0.8, 0.15, 0.05])[0]:
            case "lambert":
                albedo = mul_tuple(random_color(), random_color())
                idx = data.add_lambert(albedo)
            case "dielectric":
                idx = data.add_dielectric(1.5)
            case "metal":
                albedo = random_color(0.5, 1.0)
                fuzz = random() * 0.5
                idx = data.add_metal(albedo, fuzz)
            case _:
                raise ValueError("invalid material type")
        data.add_sphere(sphere, idx)

data.add_sphere((0.0, -1000.0, 0.0, 1000.0), data.add_lambert((0.5, 0.5, 0.5)))
data.add_sphere((0.0, 1.0, 0.0, 1.0), data.add_dielectric(1.5))
data.add_sphere((-4.0, 1.0, 0.0, 1.0), data.add_lambert((0.4, 0.2, 0.1)))
data.add_sphere((4.0, 1.0, 0.0, 1.0), data.add_metal((0.7, 0.6, 0.5), 0.0))

data.serialize("render.dat")
