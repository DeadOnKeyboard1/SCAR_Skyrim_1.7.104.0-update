"""Read-only SCAR hook evidence, bound to the registered reference hashes."""
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT.parent/'_toolchain'
sys.path.insert(0,str(TOOLS/'skyrim_1_7_104_compat/validation/python-packages'))
import pefile
import capstone


def database(path, wanted, version):
    data = path.read_bytes()
    if struct.unpack_from('<4I',data,4) != tuple(map(int,version.split('-')[0].split('.'))): raise ValueError('Database runtime mismatch')
    fmt = struct.unpack_from('<I',data)[0]
    if fmt == 5:
        count = struct.unpack_from('<I',data,92)[0]
        if len(data) < 96+count*4 or max(wanted)>=count: raise ValueError('Truncated dense database')
        result = {key:struct.unpack_from('<I',data,96+key*4)[0] for key in wanted}
        if not all(result.values()): raise ValueError('Missing dense relocation')
        return result
    if fmt not in (1,2): raise ValueError('Unknown database format')
    name = struct.unpack_from('<I',data,20)[0]
    pointer,count = struct.unpack_from('<II',data,24+name)
    if pointer != 8 or count>2000000: raise ValueError('Invalid compressed database')
    position, previous_id, previous_offset = 32+name,0,0
    result = {}
    def take(size):
        nonlocal position
        if position+size>len(data): raise ValueError('Truncated compressed database')
        value = int.from_bytes(data[position:position+size],'little'); position+=size
        return value
    def value(kind, previous):
        if kind==0: return take(8)
        if kind==1: return previous+1
        if kind in (2,3,4,5): return previous+(1 if kind%2==0 else -1)*take(1 if kind<4 else 2)
        if kind in (6,7): return take(2 if kind==6 else 4)
        raise ValueError('Invalid database encoding')
    for _ in range(count):
        kind = take(1)
        ident = value(kind&15,previous_id)
        high = kind>>4
        offset = value(high&7,previous_offset//pointer if high&8 else previous_offset)
        if high&8: offset*=pointer
        if ident in wanted: result[ident]=offset
        previous_id,previous_offset=ident,offset
    if set(result)!=set(wanted): raise ValueError('Required SCAR relocation missing')
    return result


def review():
    registry = json.loads((TOOLS/'SKSEBuildStandard/toolchain.json').read_text())['runtimeReferences']
    records, fixtures = {}, {}
    decoder = capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
    for version,ref in registry.items():
        ids = {48139,48140,43239,35551,261399,519579,519618} if version=='1.5.97.0' else {49170,49171,44440,47297,36544,36564,207890,406126,406160}
        db = TOOLS/ref['addressLibrary']; mapping=database(db,ids,version)
        record = {'ids_resolve':sorted(ids),'database_sha256':hashlib.sha256(db.read_bytes()).hexdigest(),
                  'native':'Unknown; original branch retained with guarded runtime validation; in-game test required'}
        if ref.get('image'):
            image = TOOLS/ref['image']; record['image_sha256']=hashlib.sha256(image.read_bytes()).hexdigest()
            bound = json.loads((TOOLS/ref['evidence']).read_text())
            if record['image_sha256']!=bound['image']['sha256'] or record['database_sha256']!=bound['addressLibrary']['sha256']: raise ValueError('Registered reference hash mismatch')
            if version=='1.7.104.0':
                pe = pefile.PE(str(image)); base=pe.OPTIONAL_HEADER.ImageBase
                calls = [(49170,0x3F1,47297),(49170,0x404,49171),(49170,0x435,44440)]
                for function,offset,target in calls:
                    rva = mapping[function]
                    unwind = next(e.struct for e in pe.DIRECTORY_ENTRY_EXCEPTION if e.struct.BeginAddress==rva)
                    body = pe.get_data(rva,unwind.EndAddress-rva)
                    decoded=list(decoder.disasm(body,base+rva))
                    if sum(i.size for i in decoded)!=len(body): raise ValueError('Incomplete SCAR function decoding')
                    instruction=next(i for i in decoded if i.address==base+rva+offset)
                    if instruction.mnemonic!='call' or instruction.size!=5 or instruction.op_str!=hex(base+mapping[target]): raise ValueError('SCAR CALL identity/boundary mismatch')
                    fixtures[str(function)]={'base':base+rva,'code':body.hex()}
                code=bytes.fromhex(fixtures['49170']['code'])
                if code[0x3BF:0x3C6]!=bytes.fromhex('488b8dd0000000') or code[0x3EE:0x3F1]!=bytes.fromhex('498bcf') or code[0x3FE:0x404]!=bytes.fromhex('488bd3498bcd') or code[0x409:0x40B]!=bytes.fromhex('84c0'): raise ValueError('SCAR primary register/stack ABI context mismatch')
                sink=struct.unpack('<Q',pe.get_data(mapping[207890]+8,8))[0]
                section=pe.get_section_by_rva(sink-base)
                if not section or not section.Characteristics&0x20000000: raise ValueError('Non-executable SCAR sink')
                record.update(native='Targeted Statically verified',calls=[{'function':f,'offset':hex(o),'target_id':t} for f,o,t in calls],
                              distance_abi='R15 attacker; target [RBP+0xD0]; native CALL bool result AL; callback shadow space 32 bytes; paired pushes preserve alignment',sink=hex(sink))
                obsolete=next(e.struct for e in pe.DIRECTORY_ENTRY_EXCEPTION if e.struct.BeginAddress==mapping[36544])
                old_size=obsolete.EndAddress-obsolete.BeginAddress
                if old_size!=0x36 or 0x160<old_size: raise ValueError('Obsolete debug-hook evidence changed')
                record['removed_debug_hook']={'id':36544,'old_offset':'0x160','actual_function_size':hex(old_size),
                    'reason':'Old site is outside this function; retained menu AdvanceMovie callback supplies debug rendering'}
                fixtures['calls']=[{'function':str(f),'offset':o,'target':base+mapping[t]} for f,o,t in calls]
                pe.close()
        records[version]=record
    out=ROOT/'build/preflight'; out.mkdir(parents=True,exist_ok=True)
    (out/'native-fixtures.json').write_text(json.dumps(fixtures),encoding='ascii')
    print('PASS: registered SCAR IDs/reference hashes and primary CALL/ABI/static evidence',flush=True)
    return records


if __name__=='__main__': review()
