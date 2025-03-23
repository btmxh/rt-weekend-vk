# render data generation API
#
# This data can be generated directly in main.cpp
# (the sample data is initialized this way), but tweaking
# it trigger recompilation, which is very slow!
import struct
from enum import IntEnum
from typing import final


class MaterialKind(IntEnum):
    Lambert = 0
    Dielectric = 1
    Metal = 2


class MaterialIndex:
    index: int

    def __init__(self, index: int):
        self.index = index


def vec3(x: float) -> tuple[float, float, float]:
    return (x, x, x)


@final
class RenderData:
    NUM_SPHERES = 128
    NUM_MATERIALS = 128

    def __init__(self, width: int = 640, height: int = 360):
        self.width = width
        self.height = height
        self.materials = [
            ((0.0, 0.0, 0.0, 0), MaterialKind.Lambert)
        ] * self.NUM_MATERIALS
        self.spheres = [[0.0, 0.0, 0.0, 0.0]] * self.NUM_SPHERES
        self.sphere_mat = [0] * self.NUM_SPHERES
        self.num_spheres = 0
        self.num_materials = 0

    def add_lambert(self, color: tuple[float, float, float]) -> MaterialIndex:
        return self.set_material(self.num_materials, (*color, 0), MaterialKind.Lambert)

    def add_metal(
        self, color: tuple[float, float, float], fuzz: float = 0.0
    ) -> MaterialIndex:
        if not (0 <= fuzz <= 1):
            raise ValueError("Material fuzz value must be in [0, 1]")
        fuzz_int = int(fuzz * (2**30 - 1))
        return self.set_material(
            self.num_materials, (*color, fuzz_int), MaterialKind.Metal
        )

    def add_dielectric(self, refractive_index: float) -> MaterialIndex:
        return self.set_material(
            self.num_materials, (refractive_index, 0, 0, 0), MaterialKind.Dielectric
        )

    def set_material(
        self, index: int, color: tuple[float, float, float, int], kind: MaterialKind
    ) -> MaterialIndex:
        if not (0 <= index < self.NUM_MATERIALS):
            raise ValueError("Invalid material index")
        self.materials[index] = (color, kind)
        self.num_materials = max(self.num_materials, index + 1)
        return MaterialIndex(index)

    def add_sphere(
        self,
        sphere: tuple[float, float, float, float],
        material_index: MaterialIndex,
    ):
        if not (self.num_spheres < self.NUM_SPHERES):
            raise ValueError("Too many spheres!")
        if not (0 <= material_index.index < self.NUM_MATERIALS):
            raise ValueError("Invalid material index")
        self.spheres[self.num_spheres] = list(sphere)
        self.sphere_mat[self.num_spheres] = material_index.index
        self.num_spheres += 1

    def serialize(self, filename: str):
        with open(filename, "wb") as f:
            _ = f.write(struct.pack("2I", self.width, self.height))
            for color, kind in self.materials:
                w = color[3] << 2 | kind.value
                _ = f.write(struct.pack("3f I", *color[:3], w))
            for sphere in self.spheres:
                _ = f.write(struct.pack("4f", *sphere))
            _ = f.write(struct.pack(f"{self.NUM_SPHERES}i", *self.sphere_mat))
            _ = f.write(struct.pack("2i", self.num_spheres, self.num_materials))
