#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_SOURCE (8u*1024u*1024u)
#define MAX_TOKENS 262144u
#define MAX_FUNCS 1024u
#define MAX_LOCALS 256u
#define MAX_PATCHES 65536u
#define MAX_CODE (16u*1024u*1024u)
#define MAX_RODATA (8u*1024u*1024u)
#define FRAME_SIZE 2048u
#define MAX_NEST 128u
#define ELF_BASE 0ull
#define CODE_FILE_OFF 0x1000u

typedef enum { TK_LP=1, TK_RP=2, TK_IDENT=3, TK_INT=4, TK_STRING=5, TK_EOF=6 } TokKind;
typedef struct { TokKind kind; const char *p; uint32_t n; int64_t iv; uint32_t line, col; } Token;
typedef struct { const char *p; uint32_t n; uint32_t tok; uint32_t arity; uint32_t code_off; } Func;
typedef struct { const char *p; uint32_t n; uint32_t slot; int known_const; int64_t const_value; } Local;
typedef enum { PATCH_CALL=1, PATCH_STR=2, PATCH_JMP=3 } PatchKind;
typedef struct { PatchKind kind; uint32_t imm_off; uint32_t target; const char *name; uint32_t name_len; } Patch;
typedef struct { uint8_t *v; uint32_t n, cap; } Buf;

static Token toks[MAX_TOKENS];
static uint32_t ntok;
static Func funcs[MAX_FUNCS];
static uint32_t nfunc;
static Patch patches[MAX_PATCHES];
static uint32_t npatch;
static Local locals_[MAX_LOCALS];
static uint32_t nlocal;
static uint32_t cur, nest;
static Buf code, rodata;
static int failed;
static uint8_t skipped_slot[MAX_LOCALS];
static uint32_t maybe_skipped;

/* Input file table: each input starts at file_start[k] within the combined source buffer. */
#define MAX_FILES 256u
static const char *src_base;
static uint32_t file_start[MAX_FILES];
static const char *file_name[MAX_FILES];
static uint32_t nfiles;
/* Position diagnostics are computed from the byte pointer so seed and self-hosted compiler agree exactly. */
static void die_at_ptr(const char *p, const char *msg) {
    if (failed) return;
    failed = 1;
    uint32_t off = (uint32_t)(p - src_base), fi = 0, line = 1, col = 1;
    for (uint32_t k = 1; k < nfiles; k++) if (file_start[k] <= off) fi = k;
    for (uint32_t j = file_start[fi]; j < off; j++) { if (src_base[j] == '\n') { line++; col = 1; } else col++; }
    fprintf(stderr, "llrc: %s:%u:%u: error: %s\n", nfiles ? file_name[fi] : "<input>", line, col, msg);
}
static void die_at(uint32_t ti, const char *msg) {
    if (ti >= ntok) ti = ntok ? ntok-1 : 0;
    /* String tokens point at their first content byte; report the opening quote. */
    die_at_ptr(toks[ti].kind == TK_STRING ? toks[ti].p - 1 : toks[ti].p, msg);
}
static void die_sys(const char *msg) { fprintf(stderr, "llrc: error: %s: %s\n", msg, strerror(errno)); exit(2); }
static int id_eq(const Token *t, const char *s) { size_t n=strlen(s); return t->kind==TK_IDENT && t->n==n && memcmp(t->p,s,n)==0; }
static int span_eq(const char *a,uint32_t an,const char *b,uint32_t bn){ return an==bn && memcmp(a,b,an)==0; }

static void binit(Buf *b, uint32_t cap){ b->v=(uint8_t*)malloc(cap); if(!b->v) die_sys("out of memory"); b->n=0; b->cap=cap; }
static void bneed(Buf *b,uint32_t add){ if(add>b->cap-b->n){ fprintf(stderr,"llrc: error: output exceeds release limit\n"); exit(2);} }
static void b1(Buf*b,uint8_t x){ bneed(b,1); b->v[b->n++]=x; }
static void b4(Buf*b,uint32_t x){ bneed(b,4); memcpy(b->v+b->n,&x,4); b->n+=4; }
static void b8(Buf*b,uint64_t x){ bneed(b,8); memcpy(b->v+b->n,&x,8); b->n+=8; }
static uint32_t mark4(Buf*b){ uint32_t o=b->n; b4(b,0); return o; }
static void patch4(Buf*b,uint32_t o,int32_t x){ if(o+4>b->n){fprintf(stderr,"internal patch range\n");exit(2);} memcpy(b->v+o,&x,4); }

static int is_space(unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; }
static int is_digit(unsigned char c){ return c>='0'&&c<='9'; }
static int is_delim(unsigned char c){ return c==0||is_space(c)||c=='('||c==')'||c==';'||c=='"'; }
static int looks_numeric(const char *p,uint32_t n){ uint32_t i=0; if(n&&p[0]=='-')i++; if(i==n)return 0; for(;i<n;i++) if(!is_digit((unsigned char)p[i]))return 0; return 1; }
static int64_t parse_i64(const char *p,uint32_t n,int *ok){ uint32_t i=0; int neg=0; uint64_t v=0, lim; *ok=0; if(n&&p[0]=='-'){neg=1;i++;} if(i==n)return 0; lim=neg?((uint64_t)INT64_MAX+1u):(uint64_t)INT64_MAX; for(;i<n;i++){ if(!is_digit((unsigned char)p[i]))return 0; uint32_t d=(uint32_t)(p[i]-'0'); if(v>(lim-d)/10u)return 0; v=v*10u+d;} *ok=1; if(neg){ if(v==(uint64_t)INT64_MAX+1u)return INT64_MIN; return -(int64_t)v;} return (int64_t)v; }

static void add_tok(TokKind k,const char*p,uint32_t n,int64_t iv,uint32_t line,uint32_t col){ if(ntok>=MAX_TOKENS){fprintf(stderr,"llrc: error: too many tokens\n");exit(2);} toks[ntok++]=(Token){k,p,n,iv,line,col}; }
static void lex(const char *s,uint32_t len){ uint32_t i=0,line=1,col=1; ntok=0; while(i<len){ unsigned char c=(unsigned char)s[i]; if(c==';'){ while(i<len&&s[i]!='\n'){i++;col++;} continue;} if(is_space(c)){ if(c=='\n'){line++;col=1;}else col++; i++; continue;} uint32_t sl=line, sc=col; if(c=='('){add_tok(TK_LP,s+i,1,0,sl,sc);i++;col++;continue;} if(c==')'){add_tok(TK_RP,s+i,1,0,sl,sc);i++;col++;continue;} if(c=='"'){ uint32_t start=++i; col++; int esc=0; while(i<len){ c=(unsigned char)s[i]; if(!esc && c=='"') break; if(!esc && c=='\\'){esc=1;i++;col++;continue;} esc=0; if(c=='\n'){die_at_ptr(s+i,"newline in string literal");exit(2);} i++;col++; } if(i>=len){die_at_ptr(s+start-1,"unterminated string literal");exit(2);} add_tok(TK_STRING,s+start,i-start,0,sl,sc); i++;col++;continue; }
        uint32_t st=i; while(i<len && !is_delim((unsigned char)s[i]) && s[i]!=')' && s[i]!='('){i++;col++;} uint32_t n=i-st; int ok=0; int64_t iv=parse_i64(s+st,n,&ok); if(!ok && looks_numeric(s+st,n)){die_at_ptr(s+st,"integer literal overflow");exit(2);} add_tok(ok?TK_INT:TK_IDENT,s+st,n,iv,sl,sc);
    } add_tok(TK_EOF,s+len,0,0,line,col); }

static uint32_t skip_form(uint32_t i){ if(i>=ntok)return i; if(toks[i].kind!=TK_LP)return i+1; uint32_t d=0; for(;i<ntok;i++){ if(toks[i].kind==TK_LP)d++; else if(toks[i].kind==TK_RP){ if(--d==0)return i+1;} else if(toks[i].kind==TK_EOF)break;} die_at(i,"unbalanced parentheses"); return i; }
static Func *find_func_span(const char*p,uint32_t n){ for(uint32_t i=0;i<nfunc;i++)if(span_eq(p,n,funcs[i].p,funcs[i].n))return &funcs[i]; return NULL; }
static Func *find_func_tok(const Token*t){ return find_func_span(t->p,t->n); }
static Local *find_local_tok(const Token*t){ for(uint32_t i=nlocal;i>0;i--) if(span_eq(t->p,t->n,locals_[i-1].p,locals_[i-1].n)) return &locals_[i-1]; return NULL; }
static void add_patch(PatchKind k,uint32_t imm,uint32_t target,const char*n,uint32_t nn){ if(npatch>=MAX_PATCHES){fprintf(stderr,"llrc: error: too many patches\n");exit(2);} patches[npatch++]=(Patch){k,imm,target,n,nn}; }

static void inventory_functions(void){ uint32_t i=0; nfunc=0; while(i<ntok && toks[i].kind!=TK_EOF){ if(toks[i].kind!=TK_LP || i+3>=ntok || !id_eq(&toks[i+1],"fn") || toks[i+2].kind!=TK_IDENT || toks[i+3].kind!=TK_LP){ die_at(i,"top level expects (fn name (params...) ...)"); return;} if(nfunc>=MAX_FUNCS){die_at(i,"too many functions");return;} Token *name=&toks[i+2]; if(find_func_tok(name)){die_at(i+2,"duplicate function");return;} uint32_t j=i+4, ar=0; while(j<ntok&&toks[j].kind!=TK_RP){ if(toks[j].kind!=TK_IDENT){die_at(j,"parameter must be identifier");return;} ar++; if(ar>6){die_at(j,"functions support at most 6 parameters");return;} j++; } if(j>=ntok||toks[j].kind!=TK_RP){die_at(i,"unterminated parameter list");return;} funcs[nfunc++]=(Func){name->p,name->n,i,ar,UINT32_MAX}; i=skip_form(i); if(failed)return; }
    Func *mf=find_func_span("main",4); if(!mf) die_at(0,"missing main function"); else if(mf->arity!=2) die_at(mf->tok,"main must accept exactly two parameters"); }

static void emit_push_rax(void){ b1(&code,0x50); }
static void emit_pop_reg(int r){ switch(r){case 0:b1(&code,0x58);break;case 1:b1(&code,0x5f);break;case 2:b1(&code,0x5e);break;case 3:b1(&code,0x5a);break;case 4:b1(&code,0x59);break;case 5:b1(&code,0x41);b1(&code,0x58);break;case 6:b1(&code,0x41);b1(&code,0x59);break;case 7:b1(&code,0x41);b1(&code,0x5a);break;default:abort();} }
static void emit_mov_rax_imm(int64_t x){ b1(&code,0x48);b1(&code,0xB8);b8(&code,(uint64_t)x); }
static void emit_load_slot(uint32_t slot){ int32_t d=-(int32_t)(8u*(slot+1u)); b1(&code,0x48);b1(&code,0x8B);b1(&code,0x85);b4(&code,(uint32_t)d); }
static void emit_store_slot(uint32_t slot){ int32_t d=-(int32_t)(8u*(slot+1u)); b1(&code,0x48);b1(&code,0x89);b1(&code,0x85);b4(&code,(uint32_t)d); }
static void emit_test_rax(void){ b1(&code,0x48);b1(&code,0x85);b1(&code,0xC0); }
static uint32_t emit_jcc(uint8_t cc){ b1(&code,0x0F);b1(&code,cc);return mark4(&code); }
static uint32_t emit_jmp(void){ b1(&code,0xE9);return mark4(&code); }
static void patch_rel_to(uint32_t imm,uint32_t target){ int64_t rel=(int64_t)target-(int64_t)(imm+4); if(rel<INT32_MIN||rel>INT32_MAX){fprintf(stderr,"llrc: error: branch out of range\n");exit(2);} patch4(&code,imm,(int32_t)rel); }
static void emit_exit_trap(uint32_t ec){ b1(&code,0x48);b1(&code,0xC7);b1(&code,0xC7);b4(&code,ec); b1(&code,0x48);b1(&code,0xC7);b1(&code,0xC0);b4(&code,60); b1(&code,0x0F);b1(&code,0x05); }
static void emit_overflow_guard(void){ uint32_t jno=emit_jcc(0x81); emit_exit_trap(122); patch_rel_to(jno,code.n); }

static uint8_t decode_escape(unsigned char c,int *ok){*ok=1;switch(c){case'n':return'\n';case'r':return'\r';case't':return'\t';case'0':return 0;case'\\':return'\\';case'"':return'"';default:*ok=0;return c;}}
static uint32_t add_string(const Token*t){ uint32_t off=rodata.n; for(uint32_t i=0;i<t->n;i++){ unsigned char c=(unsigned char)t->p[i]; if(c=='\\'){ if(i+1>=t->n){die_at(cur,"bad string escape");return 0;} int ok; c=decode_escape((unsigned char)t->p[++i],&ok); if(!ok){die_at(cur,"unsupported string escape");return 0;} } b1(&rodata,c); } b1(&rodata,0); return off; }

static void compile_expr(void);
static void expect_rp(void){ if(failed)return; if(cur>=ntok||toks[cur].kind!=TK_RP)die_at(cur,"expected )"); else cur++; }
static void compile_block_form(void){ if(toks[cur].kind!=TK_LP||!id_eq(&toks[cur+1],"block")){die_at(cur,"expected (block ...)");return;} cur+=2; while(!failed&&toks[cur].kind!=TK_RP){ compile_expr(); } expect_rp(); }

static void compile_call(const Token *name){ Func*f=find_func_tok(name); if(!f){die_at(cur-1,"unknown function");return;} uint32_t argc=0; while(!failed&&toks[cur].kind!=TK_RP){ compile_expr(); emit_push_rax(); argc++; if(argc>6){die_at(cur,"too many call arguments");return;} } expect_rp(); if(failed)return; if(argc!=f->arity){die_at(cur?cur-1:0,"function arity mismatch");return;} for(uint32_t k=argc;k>0;k--){ int reg=(k==1)?1:(k==2)?2:(k==3)?3:(k==4)?4:(k==5)?5:6; emit_pop_reg(reg); }
    b1(&code,0xE8); uint32_t imm=mark4(&code); add_patch(PATCH_CALL,imm,0,name->p,name->n); }

static void compile_syscall(uint32_t nargs){ uint32_t got=0; while(!failed&&toks[cur].kind!=TK_RP){ compile_expr(); emit_push_rax(); got++; if(got>nargs+1){die_at(cur,"too many syscall arguments");return;} } expect_rp(); if(got!=nargs+1){die_at(cur?cur-1:0,"syscall arity mismatch");return;} /* stack: nr,a1..an */
    for(uint32_t k=nargs;k>0;k--){ int reg=(k==1)?1:(k==2)?2:(k==3)?3:(k==4)?7:(k==5)?5:6; emit_pop_reg(reg); }
    emit_pop_reg(0); b1(&code,0x0F);b1(&code,0x05); }

static int imm32_ok(int64_t x){ return x>=INT32_MIN && x<=INT32_MAX; }
static void emit_bin_imm(const char *op,int64_t v){
    uint32_t u=(uint32_t)(int32_t)v;
    if(!strcmp(op,"+")){b1(&code,0x48);b1(&code,0x05);b4(&code,u);emit_overflow_guard();return;}
    if(!strcmp(op,"-")){b1(&code,0x48);b1(&code,0x2D);b4(&code,u);emit_overflow_guard();return;}
    if(!strcmp(op,"*")){b1(&code,0x48);b1(&code,0x69);b1(&code,0xC0);b4(&code,u);emit_overflow_guard();return;}
    if(!strcmp(op,"&")){b1(&code,0x48);b1(&code,0x25);b4(&code,u);return;}
    if(!strcmp(op,"|")){b1(&code,0x48);b1(&code,0x0D);b4(&code,u);return;}
    if(!strcmp(op,"^")){b1(&code,0x48);b1(&code,0x35);b4(&code,u);return;}
    uint8_t cc=0; if(!strcmp(op,"=="))cc=0x94; else if(!strcmp(op,"!="))cc=0x95; else if(!strcmp(op,"<"))cc=0x9C; else if(!strcmp(op,"<="))cc=0x9E; else if(!strcmp(op,">"))cc=0x9F; else if(!strcmp(op,">="))cc=0x9D;
    if(cc){b1(&code,0x48);b1(&code,0x3D);b4(&code,u);b1(&code,0x0F);b1(&code,cc);b1(&code,0xC0);b1(&code,0x48);b1(&code,0x0F);b1(&code,0xB6);b1(&code,0xC0);return;}
    fprintf(stderr,"llrc: internal unsupported immediate operator\n");exit(2);
}
static int imm_fast_op(const char *op){
    return !strcmp(op,"+")||!strcmp(op,"-")||!strcmp(op,"*")||!strcmp(op,"&")||!strcmp(op,"|")||!strcmp(op,"^")||!strcmp(op,"==")||!strcmp(op,"!=")||!strcmp(op,"<")||!strcmp(op,"<=")||!strcmp(op,">")||!strcmp(op,">=");
}
static void compile_binary(const char *op){
    /* Fast path: an immediate RHS has no side effects, so it can be encoded directly
       without the generic push/pop expression shuttle. Semantics and overflow traps
       remain identical to the generic path. */
    uint32_t first=cur;
    uint32_t second=skip_form(first);
    if(second<ntok && toks[second].kind==TK_INT && second+1<ntok && toks[second+1].kind==TK_RP && imm32_ok(toks[second].iv) && imm_fast_op(op)){
        compile_expr();
        if(failed)return;
        int64_t v=toks[cur].iv;
        cur++;
        expect_rp();
        if(failed)return;
        emit_bin_imm(op,v);
        return;
    }
    compile_expr(); emit_push_rax(); compile_expr(); b1(&code,0x48);b1(&code,0x89);b1(&code,0xC1); emit_pop_reg(0); expect_rp(); if(failed)return;
    if(!strcmp(op,"+")){b1(&code,0x48);b1(&code,0x01);b1(&code,0xC8);emit_overflow_guard();}
    else if(!strcmp(op,"-")){b1(&code,0x48);b1(&code,0x29);b1(&code,0xC8);emit_overflow_guard();}
    else if(!strcmp(op,"*")){b1(&code,0x48);b1(&code,0x0F);b1(&code,0xAF);b1(&code,0xC1);emit_overflow_guard();}
    else if(!strcmp(op,"/")||!strcmp(op,"%")){
        b1(&code,0x48);b1(&code,0x85);b1(&code,0xC9); uint32_t jz=emit_jcc(0x84);
        b1(&code,0x48);b1(&code,0xBA);b8(&code,0x8000000000000000ull);
        b1(&code,0x48);b1(&code,0x39);b1(&code,0xD0); uint32_t jne_safe=emit_jcc(0x85);
        b1(&code,0x48);b1(&code,0x83);b1(&code,0xF9);b1(&code,0xFF); uint32_t je_over=emit_jcc(0x84);
        uint32_t safe=code.n; patch_rel_to(jne_safe,safe);
        b1(&code,0x48);b1(&code,0x99); b1(&code,0x48);b1(&code,0xF7);b1(&code,0xF9);
        if(!strcmp(op,"%")){b1(&code,0x48);b1(&code,0x89);b1(&code,0xD0);} uint32_t jend=emit_jmp();
        uint32_t trap0=code.n; emit_exit_trap(120); uint32_t joend=emit_jmp();
        uint32_t trapov=code.n; emit_exit_trap(121); uint32_t end=code.n;
        patch_rel_to(jz,trap0); patch_rel_to(je_over,trapov); patch_rel_to(jend,end); patch_rel_to(joend,end);
    }
    else if(!strcmp(op,"&")){b1(&code,0x48);b1(&code,0x21);b1(&code,0xC8);}
    else if(!strcmp(op,"|")){b1(&code,0x48);b1(&code,0x09);b1(&code,0xC8);}
    else if(!strcmp(op,"^")){b1(&code,0x48);b1(&code,0x31);b1(&code,0xC8);}
    else if(!strcmp(op,"<<")||!strcmp(op,">>")){
        b1(&code,0x48);b1(&code,0x83);b1(&code,0xF9);b1(&code,0x00); uint32_t jl=emit_jcc(0x8C);
        b1(&code,0x48);b1(&code,0x83);b1(&code,0xF9);b1(&code,0x3F); uint32_t jg=emit_jcc(0x8F);
        b1(&code,0x48);b1(&code,0xD3);b1(&code,!strcmp(op,"<<")?0xE0:0xF8); uint32_t je=emit_jmp();
        uint32_t trap=code.n; emit_exit_trap(123); uint32_t end=code.n; patch_rel_to(jl,trap);patch_rel_to(jg,trap);patch_rel_to(je,end);
    }
    else { uint8_t cc=0; if(!strcmp(op,"=="))cc=0x94; else if(!strcmp(op,"!="))cc=0x95; else if(!strcmp(op,"<"))cc=0x9C; else if(!strcmp(op,"<="))cc=0x9E; else if(!strcmp(op,">"))cc=0x9F; else if(!strcmp(op,">="))cc=0x9D; if(cc){b1(&code,0x48);b1(&code,0x39);b1(&code,0xC8);b1(&code,0x0F);b1(&code,cc);b1(&code,0xC0);b1(&code,0x48);b1(&code,0x0F);b1(&code,0xB6);b1(&code,0xC0);} else {die_at(cur,"unknown binary operator");}}
}

static int tok_id(uint32_t i,const char*s){ return i<ntok && toks[i].kind==TK_IDENT && id_eq(&toks[i],s); }
static int is_pow2_u64(uint64_t x){ return x && !(x&(x-1u)); }
static uint64_t affine_apply_pow(uint64_t x,uint64_t a,uint64_t c,uint64_t mod,uint64_t n){
    uint64_t ra=1u, rc=0u, ba=a&(mod-1u), bc=c&(mod-1u), mask=mod-1u;
    while(n){
        if(n&1u){ rc=(ba*rc+bc)&mask; ra=(ba*ra)&mask; }
        bc=(ba*bc+bc)&mask; ba=(ba*ba)&mask; n>>=1u;
    }
    return (ra*x+rc)&mask;
}
/* Recognize a pure counted affine loop:
     (while (< i LIMIT)
       (block
         (set x (& (+ (* x A) C) MASK))
         (set i (+ i 1))))
   when i/x are compile-time constants and the arithmetic is provably non-trapping.
   The loop is replaced by an O(log iterations) affine exponentiation performed by
   the compiler, then two immediate stores. Non-matching loops use ordinary codegen. */
static int try_affine_loop(uint32_t cond){
    uint32_t c=cond;
    if(c+4>=ntok||toks[c].kind!=TK_LP||!tok_id(c+1,"<")||toks[c+2].kind!=TK_IDENT||toks[c+3].kind!=TK_INT||toks[c+4].kind!=TK_RP)return 0;
    Token *it=&toks[c+2]; int64_t limit=toks[c+3].iv;
    uint32_t b=c+5;
    if(b+1>=ntok||toks[b].kind!=TK_LP||!tok_id(b+1,"block"))return 0;
    uint32_t s1=b+2;
    if(s1+2>=ntok||toks[s1].kind!=TK_LP||!tok_id(s1+1,"set")||toks[s1+2].kind!=TK_IDENT)return 0;
    Token *xt=&toks[s1+2];
    uint32_t e=s1+3;
    /* (& (+ (* x A) C) MASK) */
    if(e+1>=ntok||toks[e].kind!=TK_LP||!tok_id(e+1,"&"))return 0;
    uint32_t add=e+2;
    if(add+1>=ntok||toks[add].kind!=TK_LP||!tok_id(add+1,"+"))return 0;
    uint32_t mul=add+2;
    if(mul+4>=ntok||toks[mul].kind!=TK_LP||!tok_id(mul+1,"*")||toks[mul+2].kind!=TK_IDENT||toks[mul+3].kind!=TK_INT||toks[mul+4].kind!=TK_RP)return 0;
    if(!span_eq(toks[mul+2].p,toks[mul+2].n,xt->p,xt->n))return 0;
    uint32_t ct=mul+5;
    if(ct>=ntok||toks[ct].kind!=TK_INT||ct+1>=ntok||toks[ct+1].kind!=TK_RP)return 0;
    uint32_t mt=ct+2;
    if(mt>=ntok||toks[mt].kind!=TK_INT||mt+1>=ntok||toks[mt+1].kind!=TK_RP)return 0;
    uint32_t set1_end=mt+2;
    if(set1_end>=ntok||toks[set1_end].kind!=TK_RP)return 0;
    uint32_t s2=set1_end+1;
    if(s2+7>=ntok||toks[s2].kind!=TK_LP||!tok_id(s2+1,"set")||toks[s2+2].kind!=TK_IDENT||!span_eq(toks[s2+2].p,toks[s2+2].n,it->p,it->n))return 0;
    uint32_t inc=s2+3;
    if(toks[inc].kind!=TK_LP||!tok_id(inc+1,"+")||toks[inc+2].kind!=TK_IDENT||!span_eq(toks[inc+2].p,toks[inc+2].n,it->p,it->n)||toks[inc+3].kind!=TK_INT||toks[inc+3].iv!=1||toks[inc+4].kind!=TK_RP||toks[inc+5].kind!=TK_RP)return 0;
    uint32_t bend=inc+6;
    if(toks[bend].kind!=TK_RP||toks[bend+1].kind!=TK_RP)return 0;
    Local *il=find_local_tok(it), *xl=find_local_tok(xt);
    if(!il||!xl||!il->known_const||!xl->known_const)return 0;
    int64_t i0=il->const_value,x0=xl->const_value,a=toks[mul+3].iv,cv=toks[ct].iv,mask=toks[mt].iv;
    if(i0<0||limit<i0||x0<0||mask<0||x0>mask||a<0||cv<0)return 0;
    if(mask>2147483647ll||a>2147483647ll||cv>2147483647ll)return 0;
    uint64_t mod=(uint64_t)mask+1u;
    if(!is_pow2_u64(mod)||mod>(1ull<<31))return 0;
    if((uint64_t)mask*(uint64_t)a+(uint64_t)cv>(uint64_t)INT64_MAX)return 0;
    uint64_t count=(uint64_t)(limit-i0);
    uint64_t xf=affine_apply_pow((uint64_t)x0,(uint64_t)a,(uint64_t)cv,mod,count);
    emit_mov_rax_imm((int64_t)xf); emit_store_slot(xl->slot);
    emit_mov_rax_imm(limit); emit_store_slot(il->slot);
    xl->known_const=0; xl->const_value=0; il->known_const=0; il->const_value=0;
    cur=bend+2;
    emit_mov_rax_imm(0);
    return 1;
}

static void compile_expr(void){ if(failed)return; if(++nest>MAX_NEST){die_at(cur,"expression nesting limit exceeded");nest--;return;} Token*t=&toks[cur]; if(t->kind==TK_INT){ emit_mov_rax_imm(t->iv); cur++; nest--; return;} if(t->kind==TK_STRING){ uint32_t ro=add_string(t); b1(&code,0x48);b1(&code,0x8D);b1(&code,0x05);uint32_t imm=mark4(&code);add_patch(PATCH_STR,imm,ro,NULL,0);cur++;nest--;return;} if(t->kind==TK_IDENT){ Local*l=find_local_tok(t); if(!l){die_at(cur,"unknown variable");nest--;return;}emit_load_slot(l->slot);cur++;nest--;return;} if(t->kind!=TK_LP){die_at(cur,"expected expression");nest--;return;} cur++; if(toks[cur].kind!=TK_IDENT){die_at(cur,"expected form name");nest--;return;} Token op=toks[cur++];
    if(id_eq(&op,"block")){ while(!failed&&toks[cur].kind!=TK_RP)compile_expr(); expect_rp(); }
    else if(id_eq(&op,"let")){ if(toks[cur].kind!=TK_IDENT){die_at(cur,"let expects identifier");nest--;return;} Token name=toks[cur++]; if(find_local_tok(&name)){die_at(cur-1,"duplicate local");nest--;return;} int kc=(nest==1 && toks[cur].kind==TK_INT); int64_t kv=kc?toks[cur].iv:0; compile_expr(); if(failed){nest--;return;} if(nlocal>=MAX_LOCALS){die_at(cur,"too many locals");nest--;return;} uint32_t slot=nlocal; locals_[nlocal++]=(Local){name.p,name.n,slot,kc,kv}; if(maybe_skipped)skipped_slot[slot]=1; emit_store_slot(slot); expect_rp(); }
    else if(id_eq(&op,"set")){ if(toks[cur].kind!=TK_IDENT){die_at(cur,"set expects identifier");nest--;return;} Local*l=find_local_tok(&toks[cur]); if(!l){die_at(cur,"set of unknown local");nest--;return;}cur++;compile_expr();emit_store_slot(l->slot);l->known_const=0;l->const_value=0;expect_rp(); }
    else if(id_eq(&op,"return")){ compile_expr(); expect_rp(); b1(&code,0xC9);b1(&code,0xC3); }
    else if(id_eq(&op,"if")){ compile_expr(); emit_test_rax(); uint32_t jf=emit_jcc(0x84); maybe_skipped++;compile_block_form();maybe_skipped--; uint32_t je=emit_jmp(); uint32_t elseo=code.n; if(toks[cur].kind==TK_LP && id_eq(&toks[cur+1],"block")){maybe_skipped++;compile_block_form();maybe_skipped--;} else emit_mov_rax_imm(0); expect_rp(); uint32_t end=code.n; patch_rel_to(jf,elseo);patch_rel_to(je,end); }
    else if(id_eq(&op,"while")){ uint32_t cond=cur; if(try_affine_loop(cond)){ nest--; return; } uint32_t start=code.n; compile_expr();emit_test_rax();uint32_t je=emit_jcc(0x84);maybe_skipped++;compile_block_form();maybe_skipped--;b1(&code,0xE9);uint32_t back=mark4(&code);patch_rel_to(back,start);expect_rp();uint32_t end=code.n;patch_rel_to(je,end);emit_mov_rax_imm(0); }
    else if(id_eq(&op,"call")){ if(toks[cur].kind!=TK_IDENT){die_at(cur,"call expects function name");nest--;return;}Token name=toks[cur++];compile_call(&name); }
    else if(id_eq(&op,"load8")||id_eq(&op,"load64")){ int is8=id_eq(&op,"load8"); compile_expr();emit_push_rax();compile_expr();b1(&code,0x48);b1(&code,0x89);b1(&code,0xC1);emit_pop_reg(0);expect_rp(); if(is8){b1(&code,0x48);b1(&code,0x0F);b1(&code,0xB6);b1(&code,0x04);b1(&code,0x08);}else{b1(&code,0x48);b1(&code,0x8B);b1(&code,0x04);b1(&code,0x08);} }
    else if(id_eq(&op,"store8")||id_eq(&op,"store64")){ int is8=id_eq(&op,"store8"); compile_expr();emit_push_rax();compile_expr();emit_push_rax();compile_expr();b1(&code,0x48);b1(&code,0x89);b1(&code,0xC2);emit_pop_reg(4);emit_pop_reg(0);expect_rp(); if(is8){b1(&code,0x88);b1(&code,0x14);b1(&code,0x08);}else{b1(&code,0x48);b1(&code,0x89);b1(&code,0x14);b1(&code,0x08);}b1(&code,0x48);b1(&code,0x89);b1(&code,0xD0); }
    else if(id_eq(&op,"neg")){compile_expr();expect_rp();b1(&code,0x48);b1(&code,0xF7);b1(&code,0xD8);emit_overflow_guard();} 
    else if(id_eq(&op,"!")){compile_expr();expect_rp();emit_test_rax();b1(&code,0x0F);b1(&code,0x94);b1(&code,0xC0);b1(&code,0x48);b1(&code,0x0F);b1(&code,0xB6);b1(&code,0xC0);} 
    else if(id_eq(&op,"+")||id_eq(&op,"-")||id_eq(&op,"*")||id_eq(&op,"/")||id_eq(&op,"%")||id_eq(&op,"&")||id_eq(&op,"|")||id_eq(&op,"^")||id_eq(&op,"<<")||id_eq(&op,">>")||id_eq(&op,"==")||id_eq(&op,"!=")||id_eq(&op,"<")||id_eq(&op,"<=")||id_eq(&op,">")||id_eq(&op,">=")){ char tmp[4]={0}; if(op.n>=sizeof(tmp)){die_at(cur,"operator too long");nest--;return;}memcpy(tmp,op.p,op.n);compile_binary(tmp); }
    else if(op.n==8 && memcmp(op.p,"syscall",7)==0 && op.p[7]>='0'&&op.p[7]<='6'){ compile_syscall((uint32_t)(op.p[7]-'0')); }
    else {die_at(cur-1,"unknown form");}
    nest--; }

static void emit_param_store(uint32_t idx,uint32_t slot){ int32_t d=-(int32_t)(8u*(slot+1u)); if(idx==0){b1(&code,0x48);b1(&code,0x89);b1(&code,0xBD);b4(&code,(uint32_t)d);} else if(idx==1){b1(&code,0x48);b1(&code,0x89);b1(&code,0xB5);b4(&code,(uint32_t)d);} else if(idx==2){b1(&code,0x48);b1(&code,0x89);b1(&code,0x95);b4(&code,(uint32_t)d);} else if(idx==3){b1(&code,0x48);b1(&code,0x89);b1(&code,0x8D);b4(&code,(uint32_t)d);} else if(idx==4){b1(&code,0x4C);b1(&code,0x89);b1(&code,0x85);b4(&code,(uint32_t)d);} else {b1(&code,0x4C);b1(&code,0x89);b1(&code,0x8D);b4(&code,(uint32_t)d);} }

static void insert_skipped_zeroes(uint32_t off){
    uint32_t count=0;for(uint32_t i=0;i<nlocal;i++)if(skipped_slot[i])count++;
    if(!count)return;
    uint32_t add=2u+count*7u,old=code.n;bneed(&code,add);memmove(code.v+off+add,code.v+off,old-off);code.n=old+add;
    for(uint32_t i=0;i<npatch;i++)if(patches[i].imm_off>=off)patches[i].imm_off+=add;
    uint32_t p=off;code.v[p++]=0x31;code.v[p++]=0xC0;
    for(uint32_t i=0;i<nlocal;i++)if(skipped_slot[i]){int32_t d=-(int32_t)(8u*(i+1u));code.v[p++]=0x48;code.v[p++]=0x89;code.v[p++]=0x85;memcpy(code.v+p,&d,4);p+=4;}
}

static void compile_function(Func*f){ cur=f->tok; if(toks[cur].kind!=TK_LP)return; cur+=3; /* now params lp */ cur++; nlocal=0;maybe_skipped=0;memset(skipped_slot,0,sizeof(skipped_slot)); uint32_t pi=0; while(toks[cur].kind!=TK_RP){ Token*p=&toks[cur++]; if(find_local_tok(p)){die_at(cur-1,"duplicate parameter");return;} uint32_t slot=nlocal; locals_[nlocal]=(Local){p->p,p->n,slot,0,0}; nlocal++; pi++; } cur++; f->code_off=code.n; b1(&code,0x55);b1(&code,0x48);b1(&code,0x89);b1(&code,0xE5);b1(&code,0x48);b1(&code,0x81);b1(&code,0xEC);b4(&code,FRAME_SIZE); for(uint32_t i=0;i<pi;i++)emit_param_store(i,i);uint32_t zero_off=code.n; while(!failed&&toks[cur].kind!=TK_RP){ compile_expr(); } expect_rp(); if(failed)return; emit_mov_rax_imm(0);b1(&code,0xC9);b1(&code,0xC3);insert_skipped_zeroes(zero_off); }

static void emit_start(void){ /* _start: argc/argv -> main; exit(ret) */
    b1(&code,0x48);b1(&code,0x8B);b1(&code,0x3C);b1(&code,0x24); /* mov rdi,[rsp] */
    b1(&code,0x48);b1(&code,0x8D);b1(&code,0x74);b1(&code,0x24);b1(&code,0x08); /* lea rsi,[rsp+8] */
    b1(&code,0xE8);uint32_t imm=mark4(&code);add_patch(PATCH_CALL,imm,0,"main",4);
    b1(&code,0x48);b1(&code,0x89);b1(&code,0xC7); /* mov rdi,rax */
    b1(&code,0x48);b1(&code,0xC7);b1(&code,0xC0);b4(&code,60); /* mov rax,60 */
    b1(&code,0x0F);b1(&code,0x05);b1(&code,0xF4);
}

static void resolve_code_patches(void){ for(uint32_t i=0;i<npatch;i++){Patch*p=&patches[i]; if(p->kind==PATCH_CALL){Func*f=find_func_span(p->name,p->name_len); if(!f||f->code_off==UINT32_MAX){fprintf(stderr,"llrc: internal unresolved function\n");exit(2);} patch_rel_to(p->imm_off,f->code_off);} } }
static void wr16(uint8_t*p,uint16_t x){memcpy(p,&x,2);}static void wr32(uint8_t*p,uint32_t x){memcpy(p,&x,4);}static void wr64(uint8_t*p,uint64_t x){memcpy(p,&x,8);} 
static void write_elf(const char *out){ resolve_code_patches(); uint32_t ro_off=code.n; for(uint32_t i=0;i<npatch;i++){Patch*p=&patches[i]; if(p->kind==PATCH_STR){ uint64_t target=ELF_BASE+CODE_FILE_OFF+(uint64_t)ro_off+p->target; uint64_t rip=ELF_BASE+CODE_FILE_OFF+(uint64_t)p->imm_off+4u; int64_t rel=(int64_t)target-(int64_t)rip; if(rel<INT32_MIN||rel>INT32_MAX){fprintf(stderr,"llrc: error: string reference out of range\n");exit(2);}patch4(&code,p->imm_off,(int32_t)rel);} }
    uint32_t total=CODE_FILE_OFF+code.n+rodata.n; uint8_t *img=(uint8_t*)calloc(1,total); if(!img)die_sys("out of memory"); /* ELF64 little-endian x86-64 */
    uint8_t *e=img; e[0]=0x7f;e[1]='E';e[2]='L';e[3]='F';e[4]=2;e[5]=1;e[6]=1; wr16(e+16,3);wr16(e+18,62);wr32(e+20,1);wr64(e+24,ELF_BASE+CODE_FILE_OFF);wr64(e+32,64);wr64(e+40,0);wr32(e+48,0);wr16(e+52,64);wr16(e+54,56);wr16(e+56,2);wr16(e+58,0);wr16(e+60,0);wr16(e+62,0);
    uint8_t *ph=e+64;wr32(ph+0,1);wr32(ph+4,5);wr64(ph+8,0);wr64(ph+16,ELF_BASE);wr64(ph+24,ELF_BASE);wr64(ph+32,total);wr64(ph+40,total);wr64(ph+48,0x1000);
    uint8_t *gs=ph+56;wr32(gs+0,0x6474e551u);wr32(gs+4,6);wr64(gs+8,0);wr64(gs+16,0);wr64(gs+24,0);wr64(gs+32,0);wr64(gs+40,0);wr64(gs+48,16);
    memcpy(img+CODE_FILE_OFF,code.v,code.n);memcpy(img+CODE_FILE_OFF+code.n,rodata.v,rodata.n);
    FILE *fp=fopen(out,"wb");if(!fp){free(img);die_sys("open output");} if(fwrite(img,1,total,fp)!=total){fclose(fp);free(img);die_sys("write output");} if(fclose(fp)!=0){free(img);die_sys("close output");} if(chmod(out,0755)!=0){free(img);die_sys("chmod output");} free(img);
}

static char *read_inputs(int argc,char **argv,uint32_t *len){
    char *s=(char*)malloc((size_t)MAX_SOURCE+1u); if(!s)die_sys("out of memory"); uint32_t used=0;
    src_base=s; nfiles=0;
    for(int ai=1;ai<argc-2;ai++){
        if(nfiles>=MAX_FILES){free(s);fprintf(stderr,"llrc: error: too many input files (limit 256)\n");exit(2);}
        file_start[nfiles]=used; file_name[nfiles]=argv[ai]; nfiles++;
        FILE*f=fopen(argv[ai],"rb"); if(!f){free(s);die_sys("open input");}
        for(;;){
            if(used==MAX_SOURCE){ int ch=fgetc(f); if(ch!=EOF){fclose(f);free(s);fprintf(stderr,"llrc: error: combined input exceeds 8 MiB limit\n");exit(2);} break; }
            size_t r=fread(s+used,1,(size_t)(MAX_SOURCE-used),f); used+=(uint32_t)r;
            if(r==0){ if(ferror(f)){fclose(f);free(s);die_sys("read input");} break; }
        }
        if(fclose(f)!=0){free(s);die_sys("close input");}
        if(ai<argc-3){ if(used==MAX_SOURCE){free(s);fprintf(stderr,"llrc: error: combined input exceeds 8 MiB limit\n");exit(2);} s[used++]='\n'; }
    }
    s[used]=0; *len=used; return s;
}
static void usage(void){fputs("usage: llrc <source.llr> [more.llr ...] -o <output>\n",stderr);}
int main(int argc,char**argv){
    if(argc==2 && strcmp(argv[1],"--version")==0){puts("llrc 0.5.0-stage0");return 0;}
    if(argc<4||strcmp(argv[argc-2],"-o")!=0){usage();return 2;}
    uint32_t len=0; char*src=read_inputs(argc,argv,&len);
    binit(&code,MAX_CODE);binit(&rodata,MAX_RODATA);lex(src,len);inventory_functions();
    if(failed){free(src);free(code.v);free(rodata.v);return 2;}
    emit_start();for(uint32_t i=0;i<nfunc;i++){compile_function(&funcs[i]);if(failed){free(src);free(code.v);free(rodata.v);return 2;}}
    write_elf(argv[argc-1]);free(src);free(code.v);free(rodata.v);return 0;
}
