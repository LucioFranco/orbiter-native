#!/usr/bin/env python3
"""Stage an original NASSP scenario without editing its bytes or vessel state.

Only runtime paths and module filenames map to the native build. Original
planet configuration (including NASSP radii/masses) and all vessels are retained.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
from nassp_demo import verify_data, verify_textures, DataError, PROFILES

def file_sha256(path):
    h=hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b""):h.update(chunk)
    return h.hexdigest()

def content_hashes(path):
    if path.is_file():return {".":file_sha256(path)}
    return {str(file.relative_to(path)):file_sha256(file)
            for file in path.rglob("*") if file.is_file()}

def asset_plan(root,addon,textures):
    plan={}
    for name in ("Meshes","Textures"):
        sources=[root/name,root/"Addons/NASSP"/name]
        if addon != root/"Addons/NASSP":sources.append(addon/name)
        if name=="Textures" and textures:sources.append(textures.resolve())
        for source in sources:
            for file in source.rglob("*"):
                if file.is_file():plan[Path(name)/file.relative_to(source)]=file.resolve()
    return plan

def validate_output(out,plan,bindings):
    # Fail before copying anything when output contains user-owned assets.
    # Directory links could otherwise redirect writes into a foreign checkout.
    if out.is_symlink():raise DataError("Runtime output must not be a symlink: "+str(out))
    if os.name == "nt" and out.exists() and any(out.iterdir()):
        raise DataError("Windows runtime output must be fresh: "+str(out))
    for name in bindings:
        target=out/name
        if target.exists() and not target.is_symlink():
            raise DataError("Runtime binding collision: "+str(target)+"; use a fresh output directory")
    for name in ("Config","Scenarios","Meshes","Textures"):
        target=out/name
        if target.is_symlink():raise DataError("Runtime directory collision: "+str(target))
        for directory,dirs,files in os.walk(target,followlinks=False):
            for child in dirs:
                if (Path(directory)/child).is_symlink():
                    raise DataError("Runtime directory collision: "+str(Path(directory)/child))
            if name not in ("Meshes","Textures"):continue
            for child in files:
                file=Path(directory)/child
                if not file.is_symlink() or file.relative_to(out) not in plan:
                    raise DataError("Runtime asset collision: "+str(file)+"; use a fresh output directory")

def refresh_link(target,source):
    if target.is_symlink():target.unlink()
    target.parent.mkdir(parents=True,exist_ok=True)
    target.symlink_to(source,target_is_directory=source.is_dir())

def copy_binding(target,source):
    target.parent.mkdir(parents=True,exist_ok=True)
    if source.is_dir():shutil.copytree(source,target,dirs_exist_ok=True)
    else:shutil.copy2(source,target)

def stage(args,root):
    asset_addon=root/"Addons/NASSP"
    addon=args.addon.resolve() if args.addon else asset_addon
    provenance=verify_data(addon,args.profile)
    provenance.update(verify_textures(args.textures))
    build=args.build.resolve()
    output=args.output if args.output else build/"runtime-original-insertion"
    if output.is_symlink():raise DataError("Runtime output must not be a symlink: "+str(output))
    out=output.resolve()
    for protected in (root,addon,asset_addon,build):
        if protected==out or protected.is_relative_to(out):
            raise DataError("Runtime output must be a separate directory")
    if out.is_relative_to(addon) or out.is_relative_to(asset_addon):
        raise DataError("Runtime output must not be inside NASSP source/data")
    bindings={"Modules":build/"runtime/Modules"}
    if not getattr(args,"resources_in_output",False):
        bindings["Resources"]=build/"runtime/Resources"
    elif not (out/"Resources/NASSP/csm_main_panel.bmp").is_file():
        raise DataError("Preview runtime needs preflighted NASSP bitmaps in Resources/NASSP")
    for name in ("GravityModels","Missions","Doc","Sound"):
        bindings[name]=next((p/name for p in (addon,asset_addon,root) if (p/name).exists()),root/name)
    plan=asset_plan(root,addon,args.textures)
    validate_output(out,plan,bindings)
    shutil.copytree(root/"Config",out/"Config",dirs_exist_ok=True)
    celbody=root/"Src/Celbody"
    for config in sorted(celbody.rglob("Config")):
        if config.is_dir():shutil.copytree(config,out/"Config",dirs_exist_ok=True)
    shutil.copytree(addon/"Config",out/"Config",dirs_exist_ok=True)
    for name,source in bindings.items():
        (copy_binding if os.name == "nt" else refresh_link)(out/name,source)
    for name,source in plan.items():
        (copy_binding if os.name == "nt" else refresh_link)(out/name,source)
    codes={"Sun":("E","sun"),"Mercury":("B","mer"),"Venus":("B","ven"),"Earth":("B","ear"),"Mars":("B","mar"),"Jupiter":("B","jup"),"Saturn":("B","sat"),"Uranus":("B","ura"),"Neptune":("B","nep")}
    for body,(series,code) in codes.items():
        target=out/"Config"/body/"Data";target.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(celbody/f"Vsop87/Data/Vsop87{series}_{code}.dat",target/f"Vsop87{series}.dat")
    for body,family,name in (("Jupiter","Galsat","ephem_e15.dat"),("Saturn","Satsat","tass17.dat")):
        shutil.copyfile(celbody/family/name,out/"Config"/body/"Data"/name)
    source=addon/"Scenarios/Project Apollo - NASSP"/args.scenario
    target=out/"Scenarios"/args.scenario;target.parent.mkdir(parents=True,exist_ok=True)
    if target.is_symlink():raise DataError("Runtime scenario collision: "+str(target))
    shutil.copyfile(source,target)
    assert source.read_bytes()==target.read_bytes()
    actual_bindings={name:str((out/name).resolve(strict=True)) for name in bindings}
    if os.name == "nt":
        assert not any(path.is_symlink() for path in out.rglob("*"))
        assert all(content_hashes(out/name)==content_hashes(source)
                   for name,source in bindings.items())
        assert all(file_sha256(out/name)==file_sha256(source) for name,source in plan.items())
    else:
        assert all((out/name).resolve(strict=True)==source.resolve(strict=True) for name,source in bindings.items())
        assert all((out/name).resolve(strict=True)==source for name,source in plan.items())
    binaries={"orbiter":file_sha256(build/("orbiter.exe" if os.name == "nt" else "orbiter"))}
    for file in sorted((out/"Modules").rglob("*")):
        if file.is_file():binaries[str(file.relative_to(out))]=file_sha256(file)
    resources={str(file.relative_to(out)):file_sha256(file) for file in sorted((out/"Resources").rglob("*")) if file.is_file()}
    provenance["runtime_provenance"]={"build":str(build),"bindings":actual_bindings,
        "asset_file_links_verified":len(plan) if os.name != "nt" else 0,
        "asset_files_copied_verified":len(plan) if os.name == "nt" else 0,
        "staging_mode":"copy" if os.name == "nt" else "link",
        "binary_sha256":binaries,"resource_sha256":resources,
        "reused_output_policy":"Fresh output required on Windows; Unix refreshes helper-owned links"}
    (out/"mission-source.json").write_text(json.dumps({"source":str(source),"sha256":file_sha256(source),
        "scenario_modified":False,"reference_run":"not yet available","data_provenance":provenance},indent=2)+"\n")
    print(out)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--scenario", default="Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion.scn")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--addon", type=Path, help="External original NASSP data root")
    parser.add_argument("--profile", choices=("auto", *PROFILES), default="auto")
    parser.add_argument("--textures", type=Path, help="Explicit external Orbiter Textures directory")
    parser.add_argument("--resources-in-output", action="store_true",
                        help="Keep caller-generated bitmap resources in this fresh runtime")
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    try:stage(args,root)
    except (DataError,OSError) as error:parser.exit(2,"NASSP mission staging: "+str(error)+"\n")

if __name__=="__main__":main()
