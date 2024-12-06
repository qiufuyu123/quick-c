#include "parser.h"
#include "vm.h"
#include <stdio.h>

#ifdef QCARCH_X86

void emit_load(module_t*v,char r,u64 m){
    emit(v, r>=REG_R8?0x49:0x48);
    r &= 0b00000111;
    emit(v, 0xb8+r); 
    emit_data(v, 8, &m);
}

void emit_regimm(module_t *v, char isadd,char r,u32 offset){
    
    emit(v, r>=REG_R8?0x49:0x48);
    if(r == REG_AX){
        if(isadd)
            emit(v, 0x05);
        else
            emit(v, 0x2d);
    }else {
        emit(v, 0x81);
        char base = isadd?0b11000000:0b11100000;
        if(r>=REG_R8){
            r-=REG_R8;
        }
        if(!isadd)
            r |= 0b00001000;
        emit(v, base | r);
    }
    
    emit_data(v, 4, &offset);
}

void emit_sub_regimm(module_t *v,char r, u32 offset){
    emit_regimm(v, 0, r, offset);
}
void emit_add_regimm(module_t *v,char r, u32 offset){
    emit_regimm(v, 1, r, offset);
}

void emit_op_reg(module_t *v,char op,char r,char isff){
    if(r>=REG_R8){
        emit(v, 0x41);
        r-=REG_R8;
    }
    if(isff)
        emit(v, 0xff);
    emit(v, op+r);
}

void emit_call_reg(module_t *v,char r){
    emit_op_reg(v, 0xd0, r,1);
}

void emit_push_reg(module_t *v,char r){
    emit_op_reg(v, 0x50, r,0);
    v->pushpop_stack+=8;
}

void emit_pop_reg(module_t *v,char r){
    emit_op_reg(v, 0x58, r,0);
    if(v->pushpop_stack == 0){
        printf("Unbalanced push/pop inst!\n");
        exit(-1);
    }
    v->pushpop_stack-=8;

}


static void emit_rm(module_t*v,char dst,char src,char mode,char opc,char operand_wide){
    char op = mode;
    char prefix = 0;
    if((operand_wide == TP_U16) || (operand_wide == TP_I16))
        emit(v,0x66);
    if((operand_wide == TP_U64 )|| (operand_wide == TP_I64))
        prefix = 0x48;
    else if((dst >= REG_R8)||(src >= REG_R8))
        prefix = 0x40;
    
    op |= ((0b00000111 & src) <<3);
    op |= (0b00000111 & dst);
    if(prefix){
        if(src & 0b00001000){
            prefix |= 0b00000100;
        }
        if(dst & 0b00001000){
            prefix |= 0b00000001;
        }
    }
    if(prefix)
        emit(v, prefix);
    emit(v, opc);
    emit(v, op);    
}

void emit_mov_r2r(module_t*v,char dst,char src){ 
    if(dst == src)
        return;
    emit_rm(v, dst, src, 0b11000000, 0x89,TP_U64);
}

void emit_mov_addr2r(module_t*v,char dst,char src,char wide_type){
    // if()
    emit_rm(v, src, dst, 0, (wide_type == TP_I8)||(wide_type == TP_U8)?0x8a:0x8b,
    wide_type);
    
}

void emit_mov_r2addr(module_t*v,char dst,char src,char wide_type){
    emit_rm(v, dst, src, 0, (wide_type == TP_I8)||(wide_type == TP_U8)?0x88:0x89,
    wide_type);
    
}

void emit_addr2r(module_t*v,char dst,char src){
    emit_rm(v, dst, src, 0b11000000, 0x01,TP_U64);
}

void emit_minusr2r(module_t*v,char dst,char src){
    emit_rm(v, dst, src, 0b11000000, 0x29,TP_U64);
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


u64 emit_offset_stack(module_t *v){
    u64 stack_needed = BYTE_ALIGN(v->pushpop_stack,16);
    // printf("stack should be:%x, but now it is:%x\n",stack_needed,v->pushpop_stack);
    u64 offset = stack_needed - v->pushpop_stack;
    if(offset)
        emit_offsetrsp(v, offset, 1);
    return offset;
}
void emit_restore_stack(module_t *v,u64 offset){
    if(offset)
        emit_offsetrsp(v, offset, 0);
}

void emit_loadglo(module_t *v, u64 base_addr,char r,bool is_undef){
    u64*ptr =(u64*)emit_label_load(v,r);
    *ptr = base_addr;
    u32 val = (u64)ptr - (u64)v->jit_compiled;
    module_add_reloc(v, is_undef?val|EXTERN_MASK:val);
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

u64 emit_label_load(module_t* v,char r){
    emit_load(v,r,0);
    // emit(v, 0x48);emit(v,isrbx?0xbb:0xb8);
    u64 ret = (u64)v->jit_compiled+ v->jit_cur-8;
    // u64 def = 0;
    // emit_data(v, 8, &def);
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
    v->pushpop_stack+=8;
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
    v->pushpop_stack-=8;
}

void emit_gsbase(module_t *v, char r, char is_read){
    emit(v, 0xf3);
    emit(v, r>=REG_R8?0x49:0x48);  
    emit(v, 0x0f);
    emit(v, 0xae);
    if(is_read){
        emit(v, r>=REG_R8?r-REG_R8+0xc8:r-REG_AX+0xc8);
    } else {
        emit(v, r>=REG_R8?r-REG_R8+0xd8:r-REG_AX+0xd8);        
    }
}

void emit_unary(module_t *v,char r, char type){
    if(type == UOP_NOT){
        emit(v, r>=REG_R8?0x49:0x48);
        emit(v, 0xf7);
        emit(v, r>=REG_R8?r-REG_R8+0xd0:r-REG_AX+0xd0);
    }else if(type == UOP_NEG){
        emit(v, r>=REG_R8?0x49:0x48);
        emit(v, 0xf7);
        emit(v, r>=REG_R8?r-REG_R8+0xd8:r-REG_AX+0xd8);
    }else if(type == UOP_INC){
        emit(v, r>=REG_R8?0x49:0x48);
        emit(v, 0xff);
        emit(v, r>=REG_R8?r-REG_R8+0xc0:r-REG_AX+0xc0);
    }else if(type == UOP_DEC){
        emit(v, r>=REG_R8?0x49:0x48);
        emit(v, 0xff);
        emit(v, r>=REG_R8?r-REG_R8+0xc8:r-REG_AX+0xc8);
    }
}
void emit_binary(module_t *v,char dst,char src, char type,char opwide){
    
    emit_rm(v, dst, src, 0b11000000, type == BOP_CMP?((opwide ==TP_I8 || opwide == TP_U8)?0x38:0x39):
                                            type == BOP_OR?0x09:
                                            type == BOP_AND?0x21:
                                            0x31,opwide);
}
// void emit_xor_reg(module_t *v,c)
#endif