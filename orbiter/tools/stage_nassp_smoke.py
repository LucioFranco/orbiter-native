#!/usr/bin/env python3
"""Stage a headless Saturn V startup probe, not a complete Apollo mission."""
import argparse
from pathlib import Path
import shutil
import re
import hashlib
import json
from nassp_demo import verify_data, DataError, PROFILES

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path, help="Native CMake build directory")
stage = parser.add_mutually_exclusive_group()
stage.add_argument("--pad", action="store_true", help="Stage original Saturn V and mobile launcher at LC-39")
stage.add_argument("--pad-countdown", action="store_true", help="Stage original Apollo 11 T-20 minute daytime launch-pad scenario")
stage.add_argument("--liftoff", action="store_true", help="Stage Eagle on the Moon one minute before Apollo 11 liftoff")
stage.add_argument("--pair", action="store_true", help="Stage Columbia and Eagle together before Apollo 11 powered descent")
stage.add_argument("--lm", action="store_true", help="Stage standalone Apollo 11 Eagle before powered descent, using native ephemerides")
stage.add_argument("--csm", action="store_true", help="Stage a standalone Apollo 11 CSM with SCS manual translation powered, using native ephemerides")
stage.add_argument("--insertion", action="store_true", help="Stage the saved Apollo 11 orbital insertion state")
parser.add_argument("--solar", action="store_true", help="Use native Sun/Earth/Moon ephemeris modules in a separate runtime")
parser.add_argument("--addon", type=Path, help="Selected external NASSP data root")
parser.add_argument("--profile", choices=("auto", *PROFILES), default="auto")
parser.add_argument("--output", type=Path, help="Separate runtime directory for isolated tests")
parser.add_argument("--textures", type=Path, help="Downloaded Orbiter Textures directory to merge into a solar runtime")
args = parser.parse_args()
if args.pad_countdown: args.pad = True
if args.liftoff: args.lm = True
if args.csm or args.lm or args.pair or args.pad: args.solar = True
if args.textures and (not args.solar or not args.textures.is_dir()):
    parser.error("--textures requires a solar runtime and an existing Textures directory")
root = Path(__file__).resolve().parents[1]
asset_addon = root / "Addons/NASSP"
addon = args.addon.resolve() if args.addon else asset_addon
try:
    provenance = verify_data(addon, args.profile)
except DataError as error:
    parser.error(str(error))
output = args.build.resolve() / ("runtime-liftoff" if args.liftoff else "runtime-pair" if args.pair else "runtime-lm" if args.lm else "runtime-csm" if args.csm else "runtime-solar" if args.solar else "runtime")
if args.output is not None: output = args.output.resolve()
elif args.pad: output = args.build.resolve() / "runtime-pad"
shutil.copytree(root / "Tests/Fixtures/NativeWorld", output, dirs_exist_ok=True)
shutil.copytree(addon / "Config", output / "Config", dirs_exist_ok=True)
shutil.copytree(root / "Config/CSphere", output / "Config/CSphere", dirs_exist_ok=True)
if args.solar:
    modules=output / "Modules"
    if not modules.exists():
        modules.symlink_to(args.build.resolve() / "runtime/Modules", target_is_directory=True)
    resources=output / "Resources"
    if not resources.exists():resources.symlink_to(args.build.resolve() / "runtime/Resources",target_is_directory=True)
    gravity=output / "GravityModels"
    if not gravity.exists():gravity.symlink_to(root / "GravityModels",target_is_directory=True)
    celbody=root / "Src/Celbody"
    for body in ("Sun", "Earth", "Moon"):
        source_dir=celbody / ("Moon" if body=="Moon" else f"Vsop87/{body}")
        shutil.copytree(source_dir / "Config", output / "Config", dirs_exist_ok=True)
    for body,series in (("Sun","E"),("Earth","B")):
        target=output / "Config" / body / "Data"
        target.mkdir(parents=True,exist_ok=True)
        source_name="sun" if body=="Sun" else "ear"
        shutil.copyfile(celbody / "Vsop87/Data" / f"Vsop87{series}_{source_name}.dat", target / f"Vsop87{series}.dat")
    (output / "Config/NativeSolar.cfg").write_text("Name = NativeSolar\nStar1 = Sun\nPlanet1 = Earth\nEarth:Moon1 = Moon\n")
for name in ("Meshes", "Textures", "Missions", "Doc", "Sound"):
    target = output / name
    if args.solar and name in ("Meshes", "Textures"):
        # Merge engine and addon assets without modifying either source tree.
        if target.is_symlink():
            target.unlink()
        target.mkdir(parents=True,exist_ok=True)
        source_roots = [root / name, asset_addon / name]
        if addon != asset_addon: source_roots.append(addon / name)
        if name == "Textures" and args.textures:
            source_roots.insert(0, args.textures.resolve())
        for source_root in source_roots:
            for source_file in source_root.rglob("*"):
                if not source_file.is_file():continue
                destination=target / source_file.relative_to(source_root)
                destination.parent.mkdir(parents=True,exist_ok=True)
                if destination.is_symlink():destination.unlink()
                if not destination.exists():destination.symlink_to(source_file)
    else:
        source_root = next((tree / name for tree in (addon, asset_addon) if (tree / name).exists()), addon / name)
        if target.is_symlink() and target.resolve() != source_root.resolve(): target.unlink()
        if not target.exists(): target.symlink_to(source_root, target_is_directory=True)
scenario_path = "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion.scn" if args.insertion else "Apollo 11 - Launch.scn"
if args.pad_countdown:
    scenario_path = "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 01 - Launch T-20m.scn"
if args.csm:
    scenario_path = "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 05 - MCC-2 T-15m.scn"
if args.lm or args.pair:
    scenario_path = "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m.scn"
if args.liftoff:
    scenario_path = "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m.scn"
source = (addon / "Scenarios/Project Apollo - NASSP" / scenario_path).read_bytes()
scenario = source.decode("latin1").replace("\r\n", "\n")
start = scenario.index("Eagle:" if args.lm else "Columbia:")
end = scenario.index("\nEND\n", start) + len("\nEND\n")
vessel = "\n".join(line for line in scenario[start:end].split("\n")
                   if not line.strip().startswith("BASE "))
if args.csm:
    # The LM is not part of this standalone CSM probe. Configure actual panel
    # switches for SCS translation; retain the saved electrical/propellant state.
    vessel = re.sub(r"^  DOCKINFO .*\n", "", vessel, flags=re.M)
    vessel = vessel.replace("  SCContSwitch 1 0", "  SCContSwitch 0 0")
    vessel = vessel.replace("  TransContrSwitch 0 0", "  TransContrSwitch 1 0")
    for controller in (1, 2):
        vessel = vessel.replace(f"  RotPowerDirect{controller}Switch 1 0", f"  RotPowerDirect{controller}Switch 2 0")
    vessel = re.sub(r"^(  (?:Pitch[AC][34]|Yaw[BD][34]|BdRoll[BD][12]|AcRoll[AC][12])Switch) 1 0$", r"\1 2 0", vessel, flags=re.M)
if args.pair or args.pad:
    ships=[]
    for ship in (("Columbia", "ML", "Floodlight") if args.pad else ("Columbia", "Eagle")):
        start=scenario.index(ship+":")
        end=scenario.index("\nEND\n",start)+len("\nEND\n")
        ships.append(re.sub(r"^  BASE .*\n", "", scenario[start:end], flags=re.M))
    vessel="".join(ships)
header = """BEGIN_DESC
Native Saturn V startup probe; simplified Sun/Earth world, no launch infrastructure.
END_DESC
BEGIN_ENVIRONMENT
  System TestSystem
  Date MJD 40418.397222333
END_ENVIRONMENT
BEGIN_FOCUS
  Ship Columbia
END_FOCUS
BEGIN_CAMERA
  TARGET Columbia
  MODE Extern
  POS 2.44 0 -90
END_CAMERA
BEGIN_SHIPS
"""
if args.insertion or args.csm or args.lm or args.pair or args.pad:
    date = re.search(r"Date MJD ([0-9.]+)", scenario).group(1)
    header = header.replace("40418.397222333", date).replace("Native Saturn V startup probe", "Native Apollo 11 insertion probe")
if args.solar:
    header=header.replace("System TestSystem", "System NativeSolar").replace("simplified Sun/Earth world", "native Sun/Earth/Moon ephemerides")
if args.csm:
    header=header.replace("Native Apollo 11 insertion probe", "Standalone Apollo 11 CSM manual-flight probe (SCS, THC powered, LM omitted)")
if args.lm:
    header=header.replace("Columbia", "Eagle").replace("Native Apollo 11 insertion probe", "Standalone Apollo 11 LM before powered descent")
if args.pair:
    header=header.replace("Native Apollo 11 insertion probe", "Apollo 11 Columbia and Eagle before powered descent")
if args.liftoff:
    header=header.replace("Standalone Apollo 11 LM before powered descent", "Apollo 11 Eagle on the Moon before liftoff")
name = "NasspLiftoff" if args.liftoff else "NasspPair" if args.pair else "NasspLM" if args.lm else "NasspCSM" if args.csm else "NasspInsertion" if args.insertion else "NasspStartup"
if args.pad:
    name = "NasspPad"
    header=header.replace("Native Apollo 11 insertion probe; native Sun/Earth/Moon ephemerides, no launch infrastructure.",
                          "Apollo 11 Saturn V, mobile launcher and original pad floodlights; native rendering probe.")
    header=header.replace("  POS 2.44 0 -90", "  POS 3 0 0\n  TRACKMODE Ground Earth\n  GROUNDLOCATION -80.6075 28.6083 25")
(output / f"Scenarios/{name}.scn").write_text(header + vessel + "END_SHIPS\n")
(output / "smoke-source.json").write_text(json.dumps({"addon":str(addon), "data_provenance":provenance, "scenario":scenario_path,
    "upstream_scenario_sha256":hashlib.sha256(source).hexdigest(), "fixture_modified":True,
    "systems_config_sha256":{name:hashlib.sha256((addon/"Config/ProjectApollo"/name).read_bytes()).hexdigest()
        for name in ("LEMSystems.cfg","SaturnSystems.cfg") if (addon/"Config/ProjectApollo"/name).exists()}},indent=2)+"\n")
print(output)
