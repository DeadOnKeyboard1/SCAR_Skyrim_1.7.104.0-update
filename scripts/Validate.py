"""SCAR-specific exact-build verification and private Vortex test staging."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import zipfile
from Preflight import ROOT,TOOLS,module,redact
from NativeReview import review,pefile


def pdb_identity(data):
    if not data.startswith(b'Microsoft C/C++ MSF 7.00\r\n\x1aDS\x00\x00\x00'): raise ValueError('Expected full MSF PDB')
    page,_,pages,length,_,blockmap=struct.unpack_from('<6I',data,32)
    if page not in (512,1024,2048,4096,8192) or pages*page>len(data): raise ValueError('Invalid PDB layout')
    def read(indices,size):
        if any(i>=pages for i in indices): raise ValueError('Invalid PDB page')
        return b''.join(data[i*page:(i+1)*page] for i in indices)[:size]
    count=(length+page-1)//page
    directory=read(struct.unpack_from('<'+'I'*count,data,blockmap*page),length)
    streams=struct.unpack_from('<I',directory)[0]
    sizes=struct.unpack_from('<'+'I'*streams,directory,4)
    cursor=4+streams*4
    for index,size in enumerate(sizes):
        count=0 if size==0xffffffff else (size+page-1)//page
        blocks=struct.unpack_from('<'+'I'*count,directory,cursor); cursor+=count*4
        if index==1:
            info=read(blocks,size)
            return info[12:28],struct.unpack_from('<I',info,8)[0]
    raise ValueError('Missing PDB identity stream')


def main():
    config=json.loads((ROOT/'skse-build.json').read_text())
    dll=(ROOT/config['artifacts']['dll']).read_bytes(); pdb=(ROOT/config['artifacts']['pdb']).read_bytes()
    privacy=module('privacy',TOOLS/'skyrim_1_7_104_compat/validation/check_private_paths.py')
    prefixes=[str(ROOT.parent),str(Path.home())]
    if privacy.scan_data(dll,prefixes,[]): raise ValueError('SCAR DLL privacy failed')
    findings=privacy.scan_data(pdb,prefixes,[])
    policy=config['privateTestSymbols']
    if findings and policy not in ('local-only','full-pdb-user-exception'): raise ValueError('Full PDB privacy unresolved')
    pe=pefile.PE(data=dll)
    if not pe.is_dll() or pe.FILE_HEADER.Machine!=0x8664: raise ValueError('Expected x64 DLL')
    exports={e.name.decode() for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
    if not {'SKSEPlugin_Load','SKSEPlugin_Query','SKSEPlugin_Version'}<=exports: raise ValueError('Missing SKSE exports')
    imports=[i.dll.decode() for i in pe.DIRECTORY_ENTRY_IMPORT]
    if any(n.lower().endswith(('140d.dll','based.dll')) for n in imports): raise ValueError('Debug CRT imported')
    if any(not n.lower().startswith(('api-ms-','ext-ms-')) and not (Path(os.environ['WINDIR'])/'System32'/n).is_file() for n in imports): raise ValueError('Unresolved dependency')
    identity=pdb_identity(pdb)
    rsds=[]
    for entry in pe.DIRECTORY_ENTRY_DEBUG:
        b=dll[entry.struct.PointerToRawData:entry.struct.PointerToRawData+entry.struct.SizeOfData]
        if b.startswith(b'RSDS'): rsds.append((b[4:20],struct.unpack_from('<I',b,20)[0]))
    if identity not in rsds: raise ValueError('DLL/PDB GUID/age mismatch')
    metadata=module('metadata',TOOLS/'SKSEBuildStandard/validation/Verify-SKSE-Plugin.py')
    image=metadata.PE(dll); offset=image.rva_to_off(image.export('SKSEPlugin_Version'))
    if struct.unpack_from('<II',dll,offset+0x304)!=(2,5): raise ValueError('Wrong SKSE runtime flags')
    tests=[]
    for exe in ('scar_tests.exe','scar_trampoline_tests.exe'):
        output=subprocess.check_output([str(ROOT/'build/phase1'/exe)],cwd=ROOT,text=True).strip()
        if not output.startswith('PASS:'): raise ValueError('SCAR regressions failed')
        tests.append(output)
    native=review()
    commit=subprocess.check_output(['git','-C',str(TOOLS/'CommonLibSSE-NG-v12.1.0'),'rev-parse','HEAD'],text=True).strip()
    if commit!='8ffc2b74af015f837de53cc02b0cb470a15678e4': raise ValueError('CommonLib revision changed')
    version=config['version']; digest=hashlib.sha256(dll).hexdigest()
    evidence={'version':version,'dll_sha256':digest,'pdb_sha256':hashlib.sha256(pdb).hexdigest(),
        'pdb_guid':identity[0].hex(),'pdb_age':identity[1],'pdb_matched':True,'dll_findings':0,
        'pdb_findings':len(findings),'pdb_policy':policy,'imports':imports,'exports':sorted(exports),
        'tests':tests,'native_matrix':native,'commonlib':{'tag':'v12.1.0','commit':commit,'mode':'SE+AE flatrim'},'user_confirmed':[]}
    payloads={'SKSE/Plugins/SCAR.dll':dll}
    if not findings or policy=='full-pdb-user-exception': payloads['SKSE/Plugins/SCAR.pdb']=pdb
    report=f'''# SCAR {version}: Phase 1 Private Test

Primary target: Skyrim 1.7.104.0 / SKSE 2.3.1. Release x64.
Original CMake retained, integrated with unchanged central SKSEBuildStandard.
Source modified: Yes; original source snapshot in build/original-1.0.7.
Original build/release and original top-level symbols were not overwritten.
Other mods and shared dependency source: not modified.

## Required Compatibility Work
Newest official stable considered: CommonLibSSE-NG v12.1.0, checked 2026-10-17.
https://github.com/alandtse/CommonLibSSE-NG/releases/tag/v12.1.0
Selected exact source revision: {commit}.
Migration from the existing patched v8 baseline accepted as a Phase 1 candidate:
affected animation graph, Havok, combat action and Scaleform interfaces reviewed;
cheap compile probes, primary EXE CALL/ABI evidence and regressions passed.
No publication or licensing preparation performed.
SE+AE enabled; VR stays unsupported, matching the original DLL's runtime rejection.
Existing SE and AE branches retained. No exact 1.7.104-only lock added.
Native safety checks run both at Load and again before any hook installation.
Each modified five-byte CALL must start at a decoded instruction boundary and
have an executable target. Required distance/action targets use the existing
Address Library IDs; AE angle target is checked too. Primary context validates
R15 attacker, [RBP+0xD0] target, original AL boolean result and return test.
Older SE/AE target-stack-load assumptions are guarded and remain unverified.
An incompatible layout/conflicting patch is rejected before writing native code.
No guessed offsets or speculative replacement hook.

The obsolete optional MainUpdate CALL hook was removed: on the supplied primary
reference its old +0x160 site is outside the decoded function. Debug rendering
continues through the existing menu AdvanceMovie callback, without duplicate
MainUpdate rendering. Overlay registration is deferred until DataLoaded.
The distance cave and two CALL stubs require 86 bytes; actual DKUtil/CommonLib
allocation is regression-tested. CALL rel32 range is checked before enabling
the cave. The animation-event original target is saved before the vtable write.

## Corrections And Safe Optimizations
Second review (1.0.9): action selection skips active clips whose SCAR annotations
are all invalid, then considers later clips. When no valid clip remains, first
attacks fall back to the original engine action instead of suppressing attacks.
Distance/angle probes retain their lightweight marker scan; valid SCAR actions
still enforce the existing range/chance/LOS/pathing rules without vanilla fallback.
Selected action data is returned with the clip, avoiding a second parse.
Non-SCAR animation events forward immediately, before actor/combat-target access.
First-attack interception excludes player/no-process inputs before graph scanning.
The animation event signature now matches the SDK's const event parameter.
Clip class metadata is queried once per node rather than repeatedly.
Hook installation records attempted and successful states separately; DataLoaded
cannot initialize the overlay after failed installation or register it twice.
New tests exercise the production annotation parser, malformed/mixed data,
required fields, weights, action types and cleared optional output buffers.
Hook lifecycle/forwarding was source-reviewed; real engine dispatch and active
graph transitions still require an in-game test. Native patch sites are unchanged.

Missing/invalid INI values retain original defaults and warn, without DKUtil's
fatal parser path. Invalid annotation JSON, non-finite data and missing idle
forms skip the action rather than exiting Skyrim. Existing names/defaults kept.
Chance is constrained to 0..100; zero percent cannot trigger at a zero roll.
Empty combo payload still means 100 percent; null event tags/payloads are safe.
Action types are normalized so lower-case left/power/bash types select the
matching action instead of accidentally falling back to right-hand attacks.
Wrapped angle boundaries are inclusive. All annotation tracks are examined.
Sort/iteration no longer copy full action records or annotation strings.
Unavailable Precision reach falls back to the original equipment reach logic.
Bounds use the SDK shape accessor; rigid-body local translation is initialized,
and the rotation matrix is correctly expanded to 4x4. Bound-cache validity is set.
Optional debug drawing rejects unavailable UI/nonfinite projection data and
expires lines even while its menu is unavailable. Expiration uses linear erase.
Null object/camera/mount guards and unsigned-char ctype calls remove identified
invalid dereferences/undefined behavior. Scaleform logging handles formatting
failure and va_list copies safely. No new worker tasks or synchronization.

## Runtime Matrix
| Runtime | Build/preservation | Native evidence | In-game |
| --- | --- | --- | --- |
| 1.5.97.0 | Supported by build; original SE branch Preserved | IDs resolve; native Unknown | Requires in-game test |
| 1.6.1170.0 | Supported by build; AE branch retained | IDs resolve; native Unknown | Requires in-game test |
| 1.7.104.0 | Supported by build; mandatory primary | Targeted Statically verified | Requires in-game test |
| GOG 1.6.1179.0 | Supported by build | IDs resolve; matching EXE missing; native Unknown | Requires in-game test |
| Other previous AE paths | Original branches Preserved | Unknown; runtime guards may decline | Requires in-game test |
| VR | Not supported | Original DLL already rejected VR | Not applicable |

User confirmed: None for this exact build. No previously verified runtime is
intentionally removed; unqualified native layouts are rejected rather than guessed.

## Validation And Privacy
Build: Passed. Static DLL checks: Passed with runtime-matrix limitations.
Primary hooks/relocations/ABI: Targeted Passed. Older native evidence: Partial.
Tests: {'; '.join(tests)}.
PE architecture, Release imports/dependencies, SKSE exports/flags: Passed.
Useful full PDB GUID/age matches the exact DLL: Passed; symbols not stripped.
Privacy configured before compilation; metadata prefix replacement is >DoK<.
DLL privacy validation: Passed; 0 private-path findings.
Full PDB findings: {len(findings)}. Policy: {policy}.
PDB is NOT privacy-clean when findings remain; an explicit selected-mod private
test exception is required. No DLL/PDB binary patching or blind substitutions.
Public config/text/report private-path findings: 0.
Final staging, ZIP CRC, exact payload and privacy checks: Passed with the stated
PDB exception when applicable. Such a ZIP is NOT fully privacy-clean.

## Artifacts And Test
DLL: build/phase1/SCAR.dll
Matching full PDB: build/phase1/SCAR.pdb
Private Vortex TEST ZIP: dist/SCAR-{version}_SE-AE_PRIVATE_TEST.zip
DLL SHA256: {digest}
Install as a DLL update over the existing SCAR behavior/moveset installation.
Existing INI, movesets and Nemesis/Pandora output are not replaced by this ZIP.
No behavior regeneration is needed solely for this DLL update.
Fully restart Skyrim. Check NPC first attacks, combos, left/right and power/bash
selection, reach/angle/height boundaries, Precision enabled/absent, invalid optional
annotations, debug overlay enabled/disabled and normal combat without SCAR movesets.
No game installation or save was changed automatically.
Licensing deferred completely. Phase 2 not started. Await "alles funktioniert".
'''
    texts={'PHASE1-REPORT.md':report.encode('ascii'),'phase1-validation.json':(json.dumps(evidence,indent=2)+'\n').encode('ascii')}
    public=[ROOT/'CMakeLists.txt',ROOT/'skse-build.json',ROOT/'Build-SCAR.bat',*ROOT.glob('src/**/*.cpp'),*ROOT.glob('src/**/*.h'),*ROOT.glob('scripts/*.py'),*ROOT.glob('tests/*.cpp'),*ROOT.glob('build-hooks/*.ps1')]
    if any(privacy.scan_data(p.read_bytes(),prefixes,[]) for p in public): raise ValueError('Public source/config privacy failed')
    for name,contents in texts.items():
        if privacy.scan_data(contents,prefixes,[]): raise ValueError('Report privacy failed')
        (ROOT/name).write_bytes(contents)
    stage=ROOT/f'dist/stage-{version}'
    for name,contents in payloads.items():
        file=stage/name;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(contents)
    if {p.relative_to(stage).as_posix() for p in stage.rglob('*') if p.is_file()}!=set(payloads): raise ValueError('Unexpected staged input')
    output=ROOT/f'dist/SCAR-{version}_SE-AE_PRIVATE_TEST.zip'
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
        for name,contents in payloads.items(): archive.writestr(name,contents)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None or set(archive.namelist())!=set(payloads): raise ValueError('ZIP structure/CRC failed')
        for name,contents in payloads.items():
            actual=archive.read(name)
            if actual!=contents or (stage/name).read_bytes()!=contents: raise ValueError('Final artifact bytes changed')
            hits=privacy.scan_data(actual,prefixes,[])
            if name.endswith('.pdb') and policy=='full-pdb-user-exception':
                if hits!=findings: raise ValueError('PDB exception mismatch')
            elif hits: raise ValueError('Final ZIP privacy failure')
    output.with_suffix('.zip.sha256').write_text(hashlib.sha256(output.read_bytes()).hexdigest()+'  '+output.name+'\n',encoding='ascii')
    print('\n'.join(tests));print('PASS: SCAR x64 Release, matching full PDB, native evidence and private ZIP')
    print('DLL findings: 0; PDB findings:',len(findings),'policy:',policy)
    print('Artifact:',output.relative_to(ROOT).as_posix())


if __name__=='__main__':
    try: main()
    except (ValueError,OSError,subprocess.CalledProcessError) as error: raise SystemExit(redact(str(error))) from None
