#include "parser.h"
#include "vm.h"

#ifdef QCARCH_X86

void emit_load(module_t*v,char r,u64 m){
    emit(v, 0x48);
    emit(v, 0xb8+r); 
    emit_data(v, 8, &m);
}


static void emit_offset_rbx_call(module_t *v,int offset, char w, char is_add){
    emit(v, 0x48);
    emit(v, w?0x81:0x83);
    emit(v, is_add?0xc3:0xeb);
    emit_data(v, w?4:1, &offset);
}

void emit_sub_rbx(module_t *v,int offset){
    emit_offset_rbx_call(v, offset, offset>127, 0);
}

void emit_add_rbx(module_t *v,int offset){
    emit_offset_rbx_call(v, offset, offset>127, 1);
}

static void emit_rm(module_t*v,char dst,char src,char mode,char opc){
    char op = mode,prefix=0b01001000;
    op |= ((0b00000111 & src) <<3);
    op |= (0b00000111 & dst);
    if(src & 0b00001000){
        prefix |= 0b00000100;
    }
    if(dst & 0b00001000){
        prefix |= 0b00000001;
    }
    emit(v, prefix);
    emit(v, opc);
    emit(v, op);    
}

void emit_mov_r2r(module_t*v,char dst,char src){ 
    emit_rm(v, dst, src, 0b11000000, 0x89);
}

void emit_mov_addr2r(module_t*v,char dst,char src){
    emit_rm(v, src, dst, 0, 0x8b);
}

void emit_mov_r2addr(module_t*v,char dst,char src){
    emit_rm(v, dst, src, 0, 0x89);
}

void emit_addr2r(module_t*v,char dst,char src){
    emit_rm(v, dst, src, 0b11000000, 0x01);
}

void emit_minusr2r(module_t*v,char dst,char src){
    emit_rm(v, dst, src, 0b11000000, 0x29);
}

void emit_mulrbx(module_t*v){
    emit(v, 0x48);
    emit(v, 0xf7);
    emit(v, 0xe3);
}

void emit_divrbx(module_t*v){
    emit_load(v, REG_DX, 0);
    emit(v, 0x48);
    emit(v, 0xf7);
    emit(v, 0xf3);
}


void emit_pushrbx(module_t *v){
    emit(v, 0x53); 
}
void emit_pushrax(module_t*v){
    emit(v, 0x50);
}
void emit_poprax(module_t*v){
    emit(v,0x58);
}
void emit_poprbx(module_t*v){
    emit(v, 0x5b);
}

void emit_saversp(module_t *v){
    emit(v, 0x54);
}

void emit_restorersp(module_t *v){
    emit(v,0x5c);
}

void emit_loadglo(module_t *v, u64 base_addr,bool isrbx,bool is_undef){
    u64*ptr =(u64*)emit_label_load(v,isrbx);
    *ptr = base_addr;
    u32 val = (u64)ptr - (u64)v->jit_compiled;
    module_add_reloc(v, is_undef?val|EXTERN_MASK:val);
}

void emit_param_4(module_t *v,u64 a,u64 b,u64 c,u64 d){
    emit_load(v, REG_CX, a);emit_load(v, REG_DX, b);
    emit_load(v, REG_R8, c);emit_load(v, REG_R9, d);
}

void emit_reg2rbp(module_t*v,char src,i32 offset){
    offset = -offset;
    switch (src) {
        case TP_U8:case TP_I8:
            emit(v, 0x88);
            break;
        case TP_U16:case TP_I16:
            emit(v, 0x66);emit(v, 0x89);
            break;
        case TP_U32:case TP_I32:
            emit(v, 0x89);
            break;
        case TP_U64:case TP_I64:
            emit(v, 0x48);emit(v, 0x89);
            break;
    }
    if(offset<128 && offset>=-127){
        emit(v, 0x45);
        emit(v,(u8)offset);
    }else {
        emit(v,0x85);
        emit_data(v, 4, &offset);
    }
    
}

void emit_rbpload(module_t *v,char w,u32 offset){
    offset = -offset;
    if(w < TP_I64){
        emit(v, 0x48);emit(v, 0x31);emit(v, 0xc0); // xor rax,rax
    }
     switch (w) {
        case TP_U8:case TP_I8:
            emit(v, 0x8a);
            break;
        case TP_U16:case TP_I16:
            emit(v, 0x66);emit(v, 0x8b);
            break;
        case TP_U32:case TP_I32:
            emit(v, 0x8b);
            break;
        case TP_U64:case TP_I64:
            emit(v, 0x48);emit(v, 0x8b);
            break;
    }
    if(offset<128){
        emit(v, 0x45);
        emit(v,(u8)offset);
    }else {
        emit(v,0x85);
        emit_data(v, 4, &offset);
    }
    
}

void emit_storelocaddr(module_t *v,u32 dst,u32 src){
    emit_mov_r2r(v, REG_AX, REG_BP);
    emit_load(v, REG_BX, src);
    emit_minusr2r(v, REG_AX, REG_BX);
    emit_reg2rbp(v, TP_U64, dst);
}

void emit_rsp2bx(module_t*v,u32 offset){
    emit(v, 0x48);emit(v, 0x8b);
    if(offset<128){
        emit(v, 0x5c);
        emit(v, 0x24);
        emit(v,(u8)offset);
    }else {
        emit(v,0x9c);
        emit(v, 0x24);
        emit_data(v, 4, &offset);
    }
}

u64* emit_offsetrsp(module_t*v,u32 offset,bool sub){
    emit(v, 0x48);
    if(offset<128){
        emit(v, 0x83);
    }else {
        emit(v, 0x81);
    }
    sub?emit(v, 0xec):emit(v,0xc4);
    u64* r = jit_top(v);
    if(offset<128){
        emit(v,offset);
    }else {
        emit_data(v, 4, &offset);
    }
    return r;
}

u64* emit_jmp_flg(module_t*v){
    emit_load(v, REG_AX, 0);
    u64 ret = (u64)jit_top(v)-8;
    emit(v, 0xff);emit(v,0xe0);
    return (u64*)ret;
}

i32* emit_reljmp_flg(module_t *v){
    emit(v, 0xe9); // jmp near
    emit(v,0);
    emit(v,0);
    emit(v,0);
    emit(v,0);
    u64 ret = (u64)jit_top(v)-4;
    return (i32*)ret;
}

u64 *jit_restore_off(module_t *v,int off){
    return (u64*)((u64)v->jit_compiled+off);
}

void emit_call(module_t *v,u64 addr){
    emit_load(v, REG_AX, addr);
    emit(v, 0xff);emit(v,0xd0); // callq [rax]
}

int emit_call_enter(module_t* v,int p_cnt){
    int sz = p_cnt*8 + 8; // an extra 8 for ebp stack
    sz = BYTE_ALIGN(sz, 16);
    emit_load(v, REG_AX, sz);
    emit_minusr2r(v, REG_SP, REG_AX);
    return sz;
}

void emit_sub_esp(module_t* v,int sz){
    if(sz <= 0x80){
        emit(v, 0x83);
        emit(v, 0xec);
        emit(v, sz);
    }else {
        emit(v, 0x81);
        emit(v, 0xec);
        emit_data(v, 4, &sz);
    }
}

void emit_add_esp(module_t* v,int sz){
    if(sz < 0x80){
        emit(v, 0x83);
        emit(v, 0xc4);
        emit(v, sz);
    }else {
        emit(v, 0x81);
        emit(v, 0xc4);
        emit_data(v, 4, &sz);
    }
}

void emit_call_leave(module_t* v,int sz){
    emit_load(v, REG_AX, sz);
    emit_addr2r(v, REG_SP, REG_AX);
}

u64 emit_label_load(module_t* v,bool isrbx){
    emit(v, 0x48);emit(v,isrbx?0xbb:0xb8);
    u64 ret = (u64)v->jit_compiled+ v->jit_cur;
    u64 def = 0;
    emit_data(v, 8, &def);
    return ret;
}

void backup_caller_reg(module_t *v,int no){
    emit(v, no == 1?0x57:
            no == 2?0x56:
            no == 3?0x52:
            no == 4?0x51:0x41);
    if(no == 5){
        emit(v, 0x50); // r8
    }else if(no == 6){
        emit(v, 0x51); //r9
    }
}

void restore_caller_reg(module_t *v,int no){
    emit(v, no == 1?0x5f:
            no == 2?0x5e:
            no == 3?0x5a:
            no == 4?0x59:0x41);
    if(no == 5){
        emit(v, 0x58); // r8
    }else if(no == 6){
        emit(v, 0x59); //r9
    }
}
#endif