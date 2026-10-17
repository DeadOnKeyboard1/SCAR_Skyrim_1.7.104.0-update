"""SCAR-specific qualification; shared standard owns build orchestration."""
import ctypes
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT.parent/'_toolchain'
OUT = ROOT/'build/preflight'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def redact(text):
    text = re.sub(r'(?i)[a-z]:[\\/][^\r\n]*?(?=\(\d+(?:,\d+)?\):)', '>DoK<', text)
    return re.sub(r'(?i)[a-z]:[\\/][^\r\n"<>|]*', '>DoK<', text)


def run(args, cwd=ROOT):
    result = subprocess.run(list(map(str,args)), cwd=cwd, capture_output=True, text=True, errors='replace')
    text = redact(result.stdout+result.stderr)
    with (OUT/'preflight.log').open('a', encoding='utf-8') as stream: stream.write(text)
    print(text, end='', flush=True)
    if result.returncode: raise ValueError('SCAR probe failed; see build/preflight/preflight.log')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT/'preflight.log').write_text('', encoding='ascii')
    deps = TOOLS/'vcpkg-installed/x64-windows-static-md'
    libraries = [deps/'lib'/name for name in ('spdlog.lib','fmt.lib','DirectXTK.lib')]
    if not all(p.is_file() for p in libraries): raise ValueError('Missing link inputs')
    probe = OUT/'qualification.cpp'
    probe.write_text('#include <spdlog/spdlog.h>\n#include <source_location>\nextern "C" __declspec(dllexport) const char* probe(){auto p=std::source_location::current().file_name();spdlog::info("{}",p);return p;}\n', encoding='ascii')
    run(['cl.exe','/nologo','/std:c++latest','/O2','/MD','/Z7','/EHsc','/LD',
         '/experimental:deterministic','/pathmap:'+os.environ['USERPROFILE']+'=>DoK<',
         '/DSPDLOG_COMPILED_LIB','/DSPDLOG_FMT_EXTERNAL','/I'+str(deps/'include'),probe,
         '/Fo'+str(OUT/'qualification.obj'),'/Fe'+str(OUT/'qualification.dll'),'/link',*libraries,
         '/DEBUG:FULL','/INCREMENTAL:NO','/PDB:'+str(OUT/'qualification.pdb'),'/PDBALTPATH:%_PDB%'])
    privacy = module('privacy',TOOLS/'skyrim_1_7_104_compat/validation/check_private_paths.py')
    prefixes = [str(ROOT.parent),str(Path.home())]
    findings = {kind:len(privacy.scan_data((OUT/('qualification.'+kind)).read_bytes(),prefixes,[])) for kind in ('dll','pdb')}
    (OUT/'privacy-qualification.json').write_text(json.dumps({'findings':findings,'mapping':'>DoK<',
        'inputs':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in libraries}},indent=2),encoding='ascii')
    print('Early SCAR DLL/PDB private findings:',findings,flush=True)
    if findings['dll']: raise ValueError('DLL privacy qualification failed')
    config = json.loads((ROOT/'skse-build.json').read_text())
    privacy_blocked = findings['pdb'] and config['privateTestSymbols'] not in ('local-only','full-pdb-user-exception')
    from NativeReview import review
    review()
    run([os.environ['SKSE_CMAKE_EXE'],'-S','.', '-B','build/phase1','-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DSCAR_PHASE1_BUILD=ON','-DSKSEPluginAuthor=Maxsu','-DCMAKE_MAKE_PROGRAM='+os.environ['SKSE_NINJA_EXE']])
    count = ctypes.c_int()
    parse = ctypes.windll.shell32.CommandLineToArgvW
    parse.argtypes = [ctypes.c_wchar_p,ctypes.POINTER(ctypes.c_int)]
    parse.restype = ctypes.POINTER(ctypes.c_wchar_p)
    for entry in json.loads((ROOT/'build/phase1/compile_commands.json').read_text()):
        file = Path(entry['file'])
        if ROOT not in file.parents or 'CommonLibSSE' in file.parts or file.suffix!='.cpp': continue
        data = parse(entry['command'],ctypes.byref(count))
        try: args = [data[i] for i in range(count.value)]
        finally: ctypes.windll.kernel32.LocalFree(ctypes.cast(data,ctypes.c_void_p))
        if not any(a.lower().startswith('/pathmap:') and a.endswith('=>DoK<') for a in args): raise ValueError('Actual source mapping missing')
        run([*[a for a in args if not a.startswith(('/Yu','/Yc','/Fp','/Fo','/Fd'))],'/Y-','/Zs'],entry['directory'])
    if privacy_blocked:
        raise ValueError('Cheap source/native probes complete; useful full PDB qualification still blocks all full builds pending selected-mod artifact policy')
    run([os.environ['SKSE_CMAKE_EXE'],'--build','build/phase1','--target','scar_tests','--parallel','6'])
    run([ROOT/'build/phase1/scar_tests.exe'])
    run([os.environ['SKSE_CMAKE_EXE'],'--build','build/phase1','--target','scar_trampoline_tests','--parallel','6'])
    run([ROOT/'build/phase1/scar_trampoline_tests.exe'])


if __name__=='__main__':
    try: main()
    except (ValueError,OSError) as error: raise SystemExit(redact(str(error))) from None
