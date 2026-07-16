from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
required = [
    root / "CMakeLists.txt",
    root / "Source" / "DistrhoPluginInfo.h",
    root / "Source" / "AuroraUpliftPlugin.cpp",
    root / ".github" / "workflows" / "build-vst2.yml",
]
missing = [str(path.relative_to(root)) for path in required if not path.exists()]
if missing:
    print("ERRO: arquivos ausentes:")
    for item in missing:
        print(f" - {item}")
    sys.exit(1)

cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
workflow = (root / ".github" / "workflows" / "build-vst2.yml").read_text(encoding="utf-8")
info = (root / "Source" / "DistrhoPluginInfo.h").read_text(encoding="utf-8")
source = (root / "Source" / "AuroraUpliftPlugin.cpp").read_text(encoding="utf-8")

checks = {
    "target VST2 DPF": "TARGETS vst2" in cmake,
    "sem VST3 no CMake": "TARGETS vst3" not in cmake and "VST3" not in cmake,
    "sintetizador": "DISTRHO_PLUGIN_IS_SYNTH 1" in info,
    "entrada MIDI": "DISTRHO_PLUGIN_WANT_MIDI_INPUT 1" in info,
    "32 parametros": "kParameterCount" in source and source.count("{\"") >= 32,
    "workflow VST2": "AuroraUplift-vst2" in workflow,
    "artefato DLL": "Aurora Uplift.dll" in workflow,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    print("ERRO: validacao falhou:")
    for name in failed:
        print(f" - {name}")
    sys.exit(1)

print("OK: projeto VST2-only, sintetizador MIDI, 32 parametros e workflow Windows consistentes.")
