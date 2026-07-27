#!/usr/bin/env python3
"""
Unpacks a glTF 2.0 scene into the flat asset layout CoriEngine currently understands:

  <out>/meshes/Sponza_Mesh_NNN.obj    one OBJ per glTF primitive (the engine's OBJ
  <out>/meshes/Sponza_Mesh_NNN.json   loader makes one Mesh out of a whole file, so a
                                      primitive == a file == a draw)
  <out>/materials/Sponza_Mat_NN.json  one Material per glTF material
  <out>/effects/Sponza_Effect_NN.json one ShaderEffect per material (all point at TestShader)
  <out>/textures/Sponza_Tex_NN.png    base-colour texture, transcoded to PNG
  <out>/textures/Sponza_Tex_NN.json

Coordinate handling
-------------------
The node's TRS scale is baked into the vertices so the OBJ is in metres.
By default the axes are also converted from glTF's Y-up to Z-up ((x, y, z) -> (x, -z, y)),
because SceneRenderer::Stage1 currently hardcodes `lookAt(..., up = (0,0,1))` and the
Transform component has no rotation. Pass `--up y` to keep the glTF orientation.

UVs are written as (u, 1 - v): VulkanMeshManager::LoadObjToEngine flips V on load, so this
round-trips back to the original glTF UV.
"""

import argparse
import base64
import colorsys
import json
import shutil
import struct
from pathlib import Path

import numpy as np
from PIL import Image

COMPONENT_DTYPES = {
    5120: np.int8,
    5121: np.uint8,
    5122: np.int16,
    5123: np.uint16,
    5125: np.uint32,
    5126: np.float32,
}

TYPE_COMPONENTS = {
    "SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4,
    "MAT2": 4, "MAT3": 9, "MAT4": 16,
}


def load_buffers(gltf, base_dir):
    buffers = []
    for buf in gltf["buffers"]:
        uri = buf.get("uri")
        if uri is None:
            raise RuntimeError("GLB-embedded buffers are not supported, feed me a .gltf + .bin")
        if uri.startswith("data:"):
            buffers.append(bytearray(base64.b64decode(uri.split(",", 1)[1])))
        else:
            buffers.append(bytearray((base_dir / uri).read_bytes()))
    return buffers


def read_accessor(gltf, buffers, index):
    acc = gltf["accessors"][index]
    dtype = COMPONENT_DTYPES[acc["componentType"]]
    ncomp = TYPE_COMPONENTS[acc["type"]]
    count = acc["count"]

    if "bufferView" not in acc:
        return np.zeros((count, ncomp), dtype=dtype)

    view = gltf["bufferViews"][acc["bufferView"]]
    data = buffers[view.get("buffer", 0)]
    base = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    elem_size = np.dtype(dtype).itemsize * ncomp
    stride = view.get("byteStride", elem_size)

    if stride == elem_size:
        flat = np.frombuffer(data, dtype=dtype, count=count * ncomp, offset=base)
        out = flat.reshape(count, ncomp)
    else:
        raw = np.frombuffer(data, dtype=np.uint8, count=(count - 1) * stride + elem_size, offset=base)
        idx = (np.arange(count) * stride)[:, None] + np.arange(elem_size)[None, :]
        out = raw[idx].copy().view(dtype).reshape(count, ncomp)

    return np.ascontiguousarray(out)


def node_scale(gltf):
    """Uniform-ish scale of the single root node (Sponza ships as one node with a TRS scale)."""
    scene = gltf["scenes"][gltf.get("scene", 0)]
    roots = scene["nodes"]
    if len(roots) != 1:
        raise RuntimeError(f"expected exactly one root node, got {len(roots)}")
    node = gltf["nodes"][roots[0]]
    if "matrix" in node:
        m = np.array(node["matrix"], dtype=np.float64).reshape(4, 4).T
        return np.array([np.linalg.norm(m[:3, i]) for i in range(3)])
    return np.array(node.get("scale", [1.0, 1.0, 1.0]), dtype=np.float64)


def tint(i, count, base=0.588, saturation=0.45):
    """A per-material colour tint: same perceived brightness, 'count' evenly spaced hues."""
    r, g, b = colorsys.hsv_to_rgb(i / count, 1.0, 1.0)
    mix = lambda c: round(base * ((1.0 - saturation) + saturation * c), 4)
    return [mix(r), mix(g), mix(b), 1.0]


def metadata(typename):
    return {
        "assetTypename": typename,
        "assetType": "eSecondary",
        "assetDeletionPolicy": "eRefCounted",
    }


def write_json(path, obj):
    path.write_text(json.dumps(obj, indent=4) + "\n")


def write_obj(path, name, positions, uvs, normals, indices):
    parts = [f"o {name}\n"]

    parts.append("".join(f"v {p[0]:.6f} {p[1]:.6f} {p[2]:.6f}\n" for p in positions))
    # the engine flips V when reading, so pre-flip here to round-trip the glTF UV
    parts.append("".join(f"vt {t[0]:.6f} {1.0 - t[1]:.6f}\n" for t in uvs))
    parts.append("".join(f"vn {n[0]:.6f} {n[1]:.6f} {n[2]:.6f}\n" for n in normals))

    tris = indices.reshape(-1, 3) + 1
    parts.append("".join(
        f"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}\n" for a, b, c in tris
    ))

    path.write_text("".join(parts))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gltf", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--app-root-prefix", default="assets/Sponza",
                    help="path of the output dir relative to APP_ROOT, used inside the JSONs")
    ap.add_argument("--shader", default="assets/Shaders/TestShader.json")
    ap.add_argument("--prefix", default="Sponza")
    ap.add_argument("--up", choices=["y", "z"], default="z",
                    help="target up axis, 'z' rotates the glTF Y-up data into Z-up")
    ap.add_argument("--keep-source-format", action="store_true",
                    help="copy textures verbatim instead of transcoding them to PNG")
    args = ap.parse_args()

    base_dir = args.gltf.parent
    gltf = json.loads(args.gltf.read_text())
    buffers = load_buffers(gltf, base_dir)

    out = args.out
    for sub in ("meshes", "materials", "effects", "textures"):
        (out / sub).mkdir(parents=True, exist_ok=True)

    ref = args.app_root_prefix.rstrip("/")
    px = args.prefix

    # ---------------------------------------------------------------- materials
    materials = gltf["materials"]
    for i, mat in enumerate(materials):
        pbr = mat.get("pbrMetallicRoughness", {})
        bct = pbr.get("baseColorTexture")

        tex_stem = f"{px}_Tex_{i:02d}"
        if bct is not None:
            src = base_dir / gltf["images"][gltf["textures"][bct["index"]]["source"]]["uri"]
            if args.keep_source_format:
                image_name = tex_stem + src.suffix.lower()
                shutil.copyfile(src, out / "textures" / image_name)
            else:
                image_name = tex_stem + ".png"
                Image.open(src).save(out / "textures" / image_name, optimize=True)
        else:
            image_name = tex_stem + ".png"
            Image.new("RGBA", (4, 4), (255, 255, 255, 255)).save(out / "textures" / image_name)

        write_json(out / "textures" / f"{tex_stem}.json", {
            "Metadata": metadata("Texture2"),
            "AssetData": {"image": image_name},
        })

        write_json(out / "effects" / f"{px}_Effect_{i:02d}.json", {
            "Metadata": metadata("ShaderEffect"),
            "AssetData": {
                "shaderPair": args.shader,
                # SceneRenderer binds a reverse-Z depth attachment cleared to 0, hence eGreater
                "pipelineState": {
                    "cullMode": ["eNone"],
                    "frontFace": "eCounterClockwise",
                    "depthCompareOp": "eGreater",
                    "depthTestEnable": True,
                    "depthWriteEnable": True,
                    "depthBoundsTestEnable": False,
                    "depthBiasEnable": False,
                    "stencilTestEnable": False,
                    "logicOpEnable": False,
                },
                "customData": {f"custom{k}": 0.0 for k in range(1, 9)},
            },
        })

        write_json(out / "materials" / f"{px}_Mat_{i:02d}.json", {
            "Metadata": metadata("Material"),
            "AssetData": {
                "materialData": {
                    "colorFactor": tint(i, len(materials)),
                    "albedoTexture": f"{ref}/textures/{tex_stem}.json",
                    "albedoSampler": "SponzaRepeat",
                },
                "shaderEffect": f"{ref}/effects/{px}_Effect_{i:02d}.json",
            },
        })

    # ---------------------------------------------------------------- meshes
    scale = node_scale(gltf)
    prims = [p for mesh in gltf["meshes"] for p in mesh["primitives"]]

    mesh_material = []
    total_v = total_t = 0
    lo = np.full(3, np.inf)
    hi = np.full(3, -np.inf)

    for i, prim in enumerate(prims):
        if prim.get("mode", 4) != 4:
            raise RuntimeError(f"primitive {i} is not TRIANGLES (mode {prim.get('mode')})")

        attrs = prim["attributes"]
        pos = read_accessor(gltf, buffers, attrs["POSITION"]).astype(np.float64) * scale
        nrm = (read_accessor(gltf, buffers, attrs["NORMAL"]).astype(np.float64)
               if "NORMAL" in attrs else np.tile([0.0, 1.0, 0.0], (len(pos), 1)))
        uv = (read_accessor(gltf, buffers, attrs["TEXCOORD_0"]).astype(np.float64)
              if "TEXCOORD_0" in attrs else np.zeros((len(pos), 2)))
        idx = read_accessor(gltf, buffers, prim["indices"]).astype(np.uint32).ravel()

        if args.up == "z":
            # (x, y, z) -> (x, -z, y); a proper rotation, so triangle winding is preserved
            pos = np.column_stack((pos[:, 0], -pos[:, 2], pos[:, 1]))
            nrm = np.column_stack((nrm[:, 0], -nrm[:, 2], nrm[:, 1]))

        lo = np.minimum(lo, pos.min(axis=0))
        hi = np.maximum(hi, pos.max(axis=0))
        total_v += len(pos)
        total_t += len(idx) // 3

        stem = f"{px}_Mesh_{i:03d}"
        write_obj(out / "meshes" / f"{stem}.obj", stem, pos, uv, nrm, idx)
        write_json(out / "meshes" / f"{stem}.json", {
            "Metadata": metadata("Mesh"),
            "AssetData": {"obj": f"{stem}.obj"},
        })

        mesh_material.append(prim.get("material", 0))

    print(f"{len(prims)} meshes, {len(materials)} materials, "
          f"{total_v} vertices, {total_t} triangles")
    print(f"bounds min {np.round(lo, 3).tolist()}  max {np.round(hi, 3).tolist()}")
    print(f"center     {np.round((lo + hi) / 2, 4).tolist()}  size {np.round(hi - lo, 3).tolist()}")
    print("material per mesh:")
    print(", ".join(str(m) for m in mesh_material))


if __name__ == "__main__":
    main()
