#!/usr/bin/env python3
"""Vanta source-to-native-C crosspiler with x86 admission by the canonical compiler."""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = [ROOT / 'stdlib' / name for name in (
    'kernel.llr', 'memory.llr', 'topology.llr', 'tensor.llr', 'io.llr',
    'hash.llr', 'autodiff.llr', 'text_io.llr', 'vector.llr', 'map.llr',
    'sha256.llr', 'spatial.llr')]

class Lit(str):
    pass


def tokenize(data: str):
    out = []
    i = 0
    while i < len(data):
        c = data[i]
        if c.isspace():
            i += 1
        elif c == ';':
            i = data.find('\n', i)
            if i < 0:
                break
        elif c in '()':
            out.append(c)
            i += 1
        elif c == '"':
            i += 1
            b = bytearray()
            while i < len(data) and data[i] != '"':
                if data[i] == '\\':
                    i += 1
                    if i >= len(data):
                        raise ValueError('unterminated string escape')
                    escapes = {'n': 10, 'r': 13, 't': 9, '0': 0}
                    b.append(escapes.get(data[i], ord(data[i])))
                else:
                    b.extend(data[i].encode('utf-8'))
                i += 1
            if i >= len(data):
                raise ValueError('unterminated string')
            out.append(Lit(bytes(b).decode('latin1')))
            i += 1
        else:
            j = i
            while j < len(data) and data[j] not in '();" \t\r\n':
                j += 1
            if i == j:
                raise ValueError('invalid token')
            out.append(data[i:j])
            i = j
    return out


def parse(tokens):
    def one(i):
        if i >= len(tokens):
            raise ValueError('unexpected end of source')
        if tokens[i] == '(':
            items = []
            i += 1
            while i < len(tokens) and tokens[i] != ')':
                value, i = one(i)
                items.append(value)
            if i >= len(tokens):
                raise ValueError('unbalanced source')
            return items, i + 1
        if tokens[i] == ')':
            raise ValueError('unexpected closing parenthesis')
        return tokens[i], i + 1
    forms = []
    index = 0
    while index < len(tokens):
        form, index = one(index)
        forms.append(form)
    return forms


def ident(name):
    if not isinstance(name, str) or isinstance(name, Lit):
        raise ValueError('identifier expected')
    return 'v_' + name.encode('utf-8').hex()


def c_string(value):
    raw = value.encode('latin1')
    return '"' + ''.join(f'\\x{c:02x}""' for c in raw) + '"'


def literal(value):
    n = int(value)
    if n < -(1 << 63) or n >= (1 << 63):
        raise ValueError('integer outside i64 range')
    return 'INT64_MIN' if n == -(1 << 63) else f'INT64_C({n})'


INTRINSICS = {
    'mem-new':'mem_new','mem-free':'mem_free','mem-len':'mem_len',
    'mem-domain':'mem_domain','mem-sub':'mem_sub','mem-seal':'mem_seal',
    'mem-load8':'mem_load8','mem-load64':'mem_load64',
    'mem-store8':'mem_store8','mem-store64':'mem_store64',
    'domain-new':'domain_new','domain-used':'domain_used','domain-free':'domain_free',
    'topology-new':'topology_new','topology-add':'topology_add',
    'topology-can':'topology_can','topology-free':'topology_free',
    'fabric-new':'fabric_new','fabric-link':'fabric_link',
    'fabric-cost':'fabric_cost','fabric-free':'fabric_free',
    'mem-transfer':'mem_transfer','tensor-new2':'tensor_new2',
    'tensor-rows':'tensor_rows','tensor-cols':'tensor_cols',
    'tensor-free':'tensor_free','tensor-get64':'tensor_get64',
    'tensor-set64':'tensor_set64','tensor-matmul64':'tensor_matmul64',
    'tensor-matmul-f64':'tensor_matmul_f64','tensor-transfer':'tensor_transfer',
    'print-int':'print_int','text-len':'text_len','text-eq':'text_eq',
    'text-free':'text_free','print-text':'print_text',
    'file-open-read':'file_open_read','file-open-write':'file_open_write',
    'file-read':'file_read','file-write':'file_write','file-close':'file_close',
    'hash-mem64':'hash_mem64','vec-new':'vec_new','vec-len':'vec_len',
    'vec-capacity':'vec_capacity','vec-push':'vec_push','vec-get':'vec_get',
    'vec-set':'vec_set','vec-free':'vec_free','map-new':'map_new',
    'map-len':'map_len','map-capacity':'map_capacity','map-put':'map_put',
    'map-has':'map_has','map-get':'map_get','map-free':'map_free',
    'sha256-mem':'sha256_mem','sha256-word':'sha256_word',
    'sha256-free':'sha256_free','dual-var':'dual_var','dual-const':'dual_const',
    'dual-add':'dual_add','dual-sub':'dual_sub','dual-mul':'dual_mul',
    'dual-div':'dual_div','dual-value':'dual_value','dual-grad':'dual_grad',
    'dual-free':'dual_free','spatial-new':'spatial_new','spatial-add':'spatial_add',
    'spatial-select':'spatial_select','spatial-alloc':'spatial_alloc',
    'spatial-count':'spatial_count','spatial-free':'spatial_free',
}

class Emitter:
    def __init__(self, funcs):
        self.funcs = funcs
        self.serial = 0
    def temp(self):
        self.serial += 1
        return f't{self.serial}'
    def seq(self, exprs):
        if not exprs:
            return '({ I z=0; z; })'
        return '({ I v=0; ' + ' '.join(f'v={self.expr(e)};' for e in exprs) + ' v; })'
    def call(self, name, args):
        if name not in self.funcs:
            raise ValueError(f'unknown function {name}')
        if len(args) != self.funcs[name]:
            raise ValueError(f'function arity mismatch: {name}')
        bindings = [f'I {self.temp()}={self.expr(arg)};' for arg in args]
        names = [part.split('=')[0].split()[-1] for part in bindings]
        return '({ ' + ' '.join(bindings) + f' {ident(name)}('+','.join(names)+'); })'
    def expr(self, node):
        if isinstance(node, Lit):
            return '(I)(uintptr_t)' + c_string(node)
        if isinstance(node, str):
            if re.fullmatch(r'-?[0-9]+', node):
                return literal(node)
            return ident(node)
        if not isinstance(node, list) or not node:
            raise ValueError('invalid expression')
        op, *args = node
        if op == 'block':
            return self.seq(args)
        if op == 'let':
            if len(args)!=2:
                raise ValueError('let arity')
            return f'({ident(args[0])}={self.expr(args[1])})'
        if op == 'set':
            if len(args)!=2:
                raise ValueError('set arity')
            return f'({ident(args[0])}={self.expr(args[1])})'
        if op == 'return':
            if len(args)!=1:
                raise ValueError('return arity')
            return '({ I r='+self.expr(args[0])+'; return r; (I)0; })'
        if op == 'if':
            if len(args) not in (2,3):
                raise ValueError('if arity')
            other = self.expr(args[2]) if len(args)==3 else 'INT64_C(0)'
            return '({ I r=0; if ('+self.expr(args[0])+') r='+self.expr(args[1])+'; else r='+other+'; r; })'
        if op == 'while':
            if len(args)!=2:
                raise ValueError('while arity')
            return '({ while ('+self.expr(args[0])+') (void)'+self.expr(args[1])+'; (I)0; })'
        if op == 'assert':
            return '({ if (!('+self.expr(args[0])+')) v_trap(125); (I)0; })'
        if op == 'call':
            return self.call(args[0],args[1:])
        if op == 'on-call':
            return self.call(args[1],[args[0]]+args[2:])
        if op == 'text':
            if len(args)!=1 or not isinstance(args[0],Lit):
                raise ValueError('text requires literal')
            return self.call('__vanta_priv_text_from_raw',[args[0],str(len(args[0]))])
        if op in INTRINSICS:
            return self.call('__vanta_priv_'+INTRINSICS[op],args)
        if op.startswith('syscall') and op[7:].isdigit():
            if int(op[7:]) != len(args)-1 or not isinstance(args[0],str):
                raise ValueError('syscall arity or number')
            num=int(args[0])
            if num not in {0,1,3,9,11,60,257,318}:
                raise ValueError(f'unsupported Linux syscall {num}')
            vals=[self.expr(arg) for arg in args[1:]]
            bindings=[f'I t{self.serial+i+1}={v};' for i,v in enumerate(vals)]
            names=[f't{self.serial+i+1}' for i in range(len(vals))]
            self.serial+=len(vals)
            return '({ '+' '.join(bindings)+f' v_sys_{num}('+','.join(names)+'); })'
        if op in ('load8','load64','store8','store64'):
            n=2 if op.startswith('load') else 3
            if len(args)!=n: raise ValueError('memory arity')
            return self.ordered('v_'+op,args)
        if op in ('neg','!','f64-bits','f64-from-i64','f64-sqrt','f64-abs','f64-neg','f64-isnan'):
            if len(args)!=1: raise ValueError('unary arity')
            return self.ordered('v_'+{'!':'not','neg':'neg'}.get(op,op.replace('-','_')),args)
        if op == 'f64-env':
            if args: raise ValueError('f64-env arity')
            return 'v_f64_env()'
        binary={'+':'add','-':'sub','*':'mul','/':'div','%':'mod','<<':'shl','>>':'shr',
                '&':'and','|':'or','^':'xor','==':'eq','!=':'ne','<':'lt','<=':'le','>':'gt','>=':'ge',
                'wrap+':'wrap_add','wrap-':'wrap_sub','wrap*':'wrap_mul',
                'f64+':'f64_add','f64-':'f64_sub','f64*':'f64_mul','f64/':'f64_div',
                'f64==':'f64_eq','f64!=':'f64_ne','f64<':'f64_lt','f64<=':'f64_le',
                'f64>':'f64_gt','f64>=':'f64_ge'}
        if op in binary:
            if len(args)!=2: raise ValueError('binary arity')
            return self.ordered('v_'+binary[op],args)
        raise ValueError(f'unsupported form {op}')
    def ordered(self, fn, args):
        names=[]; bindings=[]
        for arg in args:
            name=self.temp(); names.append(name)
            bindings.append(f'I {name}={self.expr(arg)};')
        return '({ '+' '.join(bindings)+f' {fn}('+','.join(names)+'); })'


def locals_of(node, found):
    if isinstance(node,list):
        if node and node[0]=='let':
            if node[1] in found: raise ValueError(f'duplicate local {node[1]}')
            found.add(node[1])
        for child in node[1:]: locals_of(child,found)

HELPERS = r'''
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <fenv.h>
#include <math.h>
#if defined(__x86_64__)
#include <xmmintrin.h>
#elif !defined(__aarch64__)
#error "Vanta crosspiler supports x86-64 and AArch64 targets"
#endif
typedef int64_t I;
typedef uint64_t U;
static void v_trap(int n) { _Exit(n); }
static I v_add(I a,I b){I x;if(__builtin_add_overflow(a,b,&x))v_trap(122);return x;}
static I v_sub(I a,I b){I x;if(__builtin_sub_overflow(a,b,&x))v_trap(122);return x;}
static I v_mul(I a,I b){I x;if(__builtin_mul_overflow(a,b,&x))v_trap(122);return x;}
static I v_neg(I a){return v_sub(0,a);}
static I v_div(I a,I b){if(!b)v_trap(120);if(a==INT64_MIN&&b==-1)v_trap(121);return a/b;}
static I v_mod(I a,I b){if(!b)v_trap(120);if(a==INT64_MIN&&b==-1)v_trap(121);return a%b;}
static I v_shl(I a,I b){if(b<0||b>63)v_trap(123);return (I)((U)a<<b);}
static I v_shr(I a,I b){if(b<0||b>63)v_trap(123);return a>>b;}
static I v_and(I a,I b){return (I)((U)a&(U)b);}
static I v_or(I a,I b){return (I)((U)a|(U)b);}
static I v_xor(I a,I b){return (I)((U)a^(U)b);}
static I v_eq(I a,I b){return a==b;}
static I v_ne(I a,I b){return a!=b;}
static I v_lt(I a,I b){return a<b;}
static I v_le(I a,I b){return a<=b;}
static I v_gt(I a,I b){return a>b;}
static I v_ge(I a,I b){return a>=b;}
static I v_not(I a){return !a;}
static I v_wrap_add(I a,I b){return (I)((U)a+(U)b);}
static I v_wrap_sub(I a,I b){return (I)((U)a-(U)b);}
static I v_wrap_mul(I a,I b){return (I)((U)a*(U)b);}
static I v_load8(I p,I o){return *((uint8_t*)(uintptr_t)((U)p+(U)o));}
static I v_load64(I p,I o){I x;memcpy(&x,(void*)(uintptr_t)((U)p+(U)o),8);return x;}
static I v_store8(I p,I o,I x){*((uint8_t*)(uintptr_t)((U)p+(U)o))=(uint8_t)x;return x;}
static I v_store64(I p,I o,I x){memcpy((void*)(uintptr_t)((U)p+(U)o),&x,8);return x;}
static double v_double(I bits){double x;memcpy(&x,&bits,8);return x;}
static I v_bits(double x){I bits;memcpy(&bits,&x,8);return bits;}
static I v_f64_bits(I x){return x;}
static I v_f64_from_i64(I x){volatile double d=(double)x;return v_bits(d);}
static I v_f64_sqrt(I x){volatile double d=sqrt(v_double(x));return v_bits(d);}
static I v_f64_abs(I x){return (I)((U)x&UINT64_C(0x7fffffffffffffff));}
static I v_f64_neg(I x){return (I)((U)x^UINT64_C(0x8000000000000000));}
static I v_f64_isnan(I x){return isnan(v_double(x));}
static I v_f64_add(I a,I b){volatile double x=v_double(a)+v_double(b);return v_bits(x);}
static I v_f64_sub(I a,I b){volatile double x=v_double(a)-v_double(b);return v_bits(x);}
static I v_f64_mul(I a,I b){volatile double x=v_double(a)*v_double(b);return v_bits(x);}
static I v_f64_div(I a,I b){volatile double x=v_double(a)/v_double(b);return v_bits(x);}
static I v_f64_eq(I a,I b){return v_double(a)==v_double(b);}
static I v_f64_ne(I a,I b){return v_double(a)!=v_double(b);}
static I v_f64_lt(I a,I b){return v_double(a)<v_double(b);}
static I v_f64_le(I a,I b){return v_double(a)<=v_double(b);}
static I v_f64_gt(I a,I b){return v_double(a)>v_double(b);}
static I v_f64_ge(I a,I b){return v_double(a)>=v_double(b);}
static I v_f64_env(void){
#if defined(__x86_64__)
  return (I)_mm_getcsr();
#elif defined(__aarch64__)
  U fpcr=0; __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
  return fpcr==0 ? INT64_C(8064) : 0;
#endif
}
static int v_init_f64(void){
#if defined(__x86_64__)
  _mm_setcsr(0x1f80);
#elif defined(__aarch64__)
  U fpcr=0, fpsr=0;
  __asm__ volatile("msr fpcr, %0" :: "r"(fpcr));
  __asm__ volatile("msr fpsr, %0" :: "r"(fpsr));
#endif
  return fesetround(FE_TONEAREST);
}
static I v_ret(long r){return r==-1?-(I)errno:(I)r;}
static I v_sys_0(I fd,I p,I n){return v_ret(syscall(SYS_read,(int)fd,(void*)(uintptr_t)p,(size_t)n));}
static I v_sys_1(I fd,I p,I n){return v_ret(syscall(SYS_write,(int)fd,(void*)(uintptr_t)p,(size_t)n));}
static I v_sys_3(I fd){return v_ret(syscall(SYS_close,(int)fd));}
static I v_sys_9(I addr,I len,I prot,I flags,I fd,I off){void *p=mmap((void*)(uintptr_t)addr,(size_t)len,(int)prot,(int)flags,(int)fd,(off_t)off);return p==MAP_FAILED?-(I)errno:(I)(uintptr_t)p;}
static I v_sys_11(I addr,I len){return v_ret(syscall(SYS_munmap,(void*)(uintptr_t)addr,(size_t)len));}
static I v_sys_60(I code){_Exit((int)code);return 0;}
static I v_sys_257(I dir,I path,I flags,I mode){return v_ret(syscall(SYS_openat,(int)dir,(char*)(uintptr_t)path,(int)flags,(mode_t)mode));}
static I v_sys_318(I ptr,I len,I flags){return v_ret(syscall(SYS_getrandom,(void*)(uintptr_t)ptr,(size_t)len,(unsigned)flags));}
'''


def source_to_c(paths):
    funcs={}; bodies={}
    for path in paths:
        for form in parse(tokenize(path.read_text())):
            if not isinstance(form,list) or len(form)<4 or form[0]!='fn':
                raise ValueError(f'bad function in {path}')
            name,params=form[1:3]
            if name in funcs or not isinstance(params,list) or len(params)>6:
                raise ValueError(f'duplicate or invalid function {name}')
            funcs[name]=len(params)
            bodies[name]=(params,form[3:])
    if funcs.get('main')!=2: raise ValueError('main must accept argc and argv')
    emit=Emitter(funcs)
    definitions=[]
    for name,(params,body) in bodies.items():
        if name=='__vanta_priv_table_base':
            definitions.append(f'''static I {ident(name)}(void) {{
  static void *table;
  if (!table) {{ table=mmap(NULL,1048576,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(table==MAP_FAILED) v_trap(126);
    I magic=INT64_C(6215335084561486405); memcpy(table,&magic,8);
  }}
  return (I)(uintptr_t)table;
}}''')
            continue
        local_names=set(params)
        for expr in body: locals_of(expr,local_names)
        additions=sorted(local_names-set(params))
        init=' '.join(f'I {ident(n)}=0;' for n in additions)
        param_use=' '.join(f'(void){ident(n)};' for n in params)
        params_c=','.join(f'I {ident(p)}' for p in params) or 'void'
        definitions.append(f'static I {ident(name)}({params_c}) {{\n  {init} {param_use}\n  (void){emit.seq(body)};\n  return 0;\n}}')
    prototypes='\n'.join(f'static I {ident(name)}('+(','.join('I' for _ in range(arity)) or 'void')+');' for name,arity in funcs.items())
    entry=f'''int main(int argc,char **argv){{
  if(v_init_f64()) return 126;
  I rc={ident('main')}((I)argc,(I)(uintptr_t)argv);
  if(rc==0 && {'1' if '__vanta_priv_resource_audit' in funcs else '0'})
    {ident('__vanta_priv_resource_audit') if '__vanta_priv_resource_audit' in funcs else 'v_trap'}();
  return (int)(unsigned char)rc;
}}'''
    c=HELPERS+'\n'+prototypes+'\n'+'\n'.join(definitions)+'\n'+entry
    return c.replace('static I ', 'static __attribute__((unused)) I ').replace('; ', ';\n  ')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('program',type=Path)
    p.add_argument('--emit-c',type=Path)
    p.add_argument('-o','--output',type=Path)
    p.add_argument('--cc',default=os.environ.get('VANTA_CC','cc'))
    ns=p.parse_args()
    if not ns.emit_c and not ns.output:
        p.error('specify --emit-c or -o')
    program=ns.program.resolve()
    if program.suffix!='.vnt' or not program.is_file():
        p.error('program must be an existing .vnt file')
    # The canonical Vanta compiler owns all .vnt static admission checks.
    with tempfile.TemporaryDirectory(prefix='vanta-cross-admission-') as temp:
        subprocess.run([str(ROOT/'bin/vantac-linux-x86_64'),
                        *map(str,RUNTIME),str(program),'-o',str(Path(temp)/'admitted')],
                       check=True)
    c=source_to_c(RUNTIME+[program])
    if ns.emit_c:
        ns.emit_c.write_text(c)
    if ns.output:
        with tempfile.TemporaryDirectory(prefix='vanta-cross-build-') as temp:
            path=ns.emit_c or Path(temp)/'program.c'
            if not ns.emit_c: path.write_text(c)
            subprocess.run([ns.cc,'-std=gnu11','-O2','-Wall','-Wextra','-Werror','-fno-strict-aliasing',
                            '-fno-fast-math','-ffp-contract=off','-frounding-math',
                            str(path),'-lm','-o',str(ns.output)],check=True)
    return 0

if __name__=='__main__':
    try: sys.exit(main())
    except (ValueError,OSError,subprocess.CalledProcessError) as error:
        print(f'vantax: {error}',file=sys.stderr)
        sys.exit(1)
