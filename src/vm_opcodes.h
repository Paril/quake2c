#pragma once

enum
{
	OP_DONE,	//0
	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,
	OP_MUL_VF,
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_SUB_F,
	OP_SUB_V,
	
	OP_EQ_F,	//10
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FNC,
	
	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FNC,
	
	OP_LE_F,	//20
	OP_GE_F,
	OP_LT_F,
	OP_GT_F,

	OP_LOAD_F,
	OP_LOAD_V,
	OP_LOAD_S,
	OP_LOAD_ENT,
	OP_LOAD_FLD,
	OP_LOAD_FNC,

	OP_ADDRESS,	//30

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FLD,
	OP_STORE_FNC,

	OP_STOREP_F,
	OP_STOREP_V,
	OP_STOREP_S,
	OP_STOREP_ENT,	//40
	OP_STOREP_FLD,
	OP_STOREP_FNC,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FNC,
	OP_IF_I,
	OP_IFNOT_I,		//50
	OP_CALL0,		//careful... hexen2 and q1 have different calling conventions
	OP_CALL1,		//remap hexen2 calls to OP_CALL2H
	OP_CALL2,
	OP_CALL3,
	OP_CALL4,
	OP_CALL5,
	OP_CALL6,
	OP_CALL7,
	OP_CALL8,
	OP_STATE,		//60
	OP_GOTO,
	OP_AND_F,
	OP_OR_F,
	
	OP_BITAND_F,
	OP_BITOR_F,

	//these following ones are Hexen 2 constants.	
	OP_MULSTORE_F,	//66 redundant, for h2 compat
	OP_MULSTORE_VF,	//67 redundant, for h2 compat
	OP_MULSTOREP_F,	//68
	OP_MULSTOREP_VF,//69

	OP_DIVSTORE_F,	//70 redundant, for h2 compat
	OP_DIVSTOREP_F,	//71

	OP_ADDSTORE_F,	//72 redundant, for h2 compat
	OP_ADDSTORE_V,	//73 redundant, for h2 compat
	OP_ADDSTOREP_F,	//74
	OP_ADDSTOREP_V,	//75

	OP_SUBSTORE_F,	//76 redundant, for h2 compat
	OP_SUBSTORE_V,	//77 redundant, for h2 compat
	OP_SUBSTOREP_F,	//78
	OP_SUBSTOREP_V,	//79

	OP_FETCH_GBL_F,	//80 has built-in bounds check
	OP_FETCH_GBL_V,	//81 has built-in bounds check
	OP_FETCH_GBL_S,	//82 has built-in bounds check
	OP_FETCH_GBL_E,	//83 has built-in bounds check
	OP_FETCH_GBL_FNC,//84 has built-in bounds check

	OP_CSTATE,		//85
	OP_CWSTATE,		//86

	OP_THINKTIME,	//87 shortcut for OPA.nextthink=time+OPB

	OP_BITSETSTORE_F,	//88 redundant, for h2 compat
	OP_BITSETSTOREP_F,	//89
	OP_BITCLRSTORE_F,	//90
	OP_BITCLRSTOREP_F,	//91

	OP_RAND0,		//92	OPC = random()
	OP_RAND1,		//93	OPC = random()*OPA
	OP_RAND2,		//94	OPC = random()*(OPB-OPA)+OPA
	OP_RANDV0,		//95	//3d/box versions of the above.
	OP_RANDV1,		//96
	OP_RANDV2,		//97

	OP_SWITCH_F,	//98	switchref=OPA; PC += OPB   --- the jump allows the jump table (such as it is) to be inserted after the block.
	OP_SWITCH_V,	//99
	OP_SWITCH_S,	//100
	OP_SWITCH_E,	//101
	OP_SWITCH_FNC,	//102

	OP_CASE,		//103	if (OPA===switchref) PC += OPB
	OP_CASERANGE,	//104   if (OPA<=switchref&&switchref<=OPB) PC += OPC

	//the rest are added
	//mostly they are various different ways of adding two vars with conversions.

	//hexen2 calling convention (-TH2 requires us to remap OP_CALLX to these on load, -TFTE just uses these directly.)
	OP_CALL1H,	//OFS_PARM0=OPB
	OP_CALL2H,	//OFS_PARM0,1=OPB,OPC
	OP_CALL3H,	//no extra args
	OP_CALL4H,
	OP_CALL5H,
	OP_CALL6H,		//110
	OP_CALL7H,
	OP_CALL8H,


	OP_STORE_I,
	OP_STORE_IF,			//OPB.f = (float)OPA.i (makes more sense when written as a->b)
	OP_STORE_FI,			//OPB.i = (int)OPA.f
	
	OP_ADD_I,
	OP_ADD_FI,				//OPC.f = OPA.f + OPB.i
	OP_ADD_IF,				//OPC.f = OPA.i + OPB.f	-- redundant...
  
	OP_SUB_I,				//OPC.i = OPA.i - OPB.i
	OP_SUB_FI,		//120	//OPC.f = OPA.f - OPB.i
	OP_SUB_IF,				//OPC.f = OPA.i - OPB.f

	OP_CONV_ITOF,			//OPC.f=(float)OPA.i
	OP_CONV_FTOI,			//OPC.i=(int)OPA.f
	OP_CP_ITOF,				//OPC.f=(float)(*OPA).i
	OP_CP_FTOI,				//OPC.i=(int)(*OPA).f
	OP_LOAD_I,
	OP_STOREP_I,
	OP_STOREP_IF,
	OP_STOREP_FI,

	OP_BITAND_I,	//130
	OP_BITOR_I,

	OP_MUL_I,
	OP_DIV_I,
	OP_EQ_I,
	OP_NE_I,

	OP_IFNOT_S,	//compares string empty, rather than just null.
	OP_IF_S,

	OP_NOT_I,

	OP_DIV_VF,

	OP_BITXOR_I,		//140
	OP_RSHIFT_I,
	OP_LSHIFT_I,

	OP_GLOBALADDRESS,	//C.p = &A + B.i*4
	OP_ADD_PIW,			//C.p = A.p + B.i*4

	OP_LOADA_F,
	OP_LOADA_V,	
	OP_LOADA_S,
	OP_LOADA_ENT,
	OP_LOADA_FLD,
	OP_LOADA_FNC,	//150
	OP_LOADA_I,

	OP_STORE_P,	//152... erm.. wait...
	OP_LOAD_P,

	OP_LOADP_F,
	OP_LOADP_V,	
	OP_LOADP_S,
	OP_LOADP_ENT,
	OP_LOADP_FLD,
	OP_LOADP_FNC,
	OP_LOADP_I,		//160

	OP_LE_I,
	OP_GE_I,
	OP_LT_I,
	OP_GT_I,

	OP_LE_IF,
	OP_GE_IF,
	OP_LT_IF,
	OP_GT_IF,

	OP_LE_FI,
	OP_GE_FI,		//170
	OP_LT_FI,
	OP_GT_FI,

	OP_EQ_IF,
	OP_EQ_FI,

	//-------------------------------------
	//string manipulation.
	OP_ADD_SF,	//(char*)c = (char*)a + (float)b    add_fi->i
	OP_SUB_S,	//(float)c = (char*)a - (char*)b    sub_ii->f
	OP_STOREP_C,//(float)c = *(char*)b = (float)a
	OP_LOADP_C,	//(float)c = *(char*)
	//-------------------------------------


	OP_MUL_IF,
	OP_MUL_FI,		//180
	OP_MUL_VI,
	OP_MUL_IV,
	OP_DIV_IF,
	OP_DIV_FI,
	OP_BITAND_IF,
	OP_BITOR_IF,
	OP_BITAND_FI,
	OP_BITOR_FI,
	OP_AND_I,
	OP_OR_I,		//190
	OP_AND_IF,
	OP_OR_IF,
	OP_AND_FI,
	OP_OR_FI,
	OP_NE_IF,
	OP_NE_FI,

//erm... FTEQCC doesn't make use of these (doesn't model separate pointer types). These are for DP.
	OP_GSTOREP_I,
	OP_GSTOREP_F,
	OP_GSTOREP_ENT,
	OP_GSTOREP_FLD,		//200
	OP_GSTOREP_S,
	OP_GSTOREP_FNC,		
	OP_GSTOREP_V,
	OP_GADDRESS,
	OP_GLOAD_I,
	OP_GLOAD_F,
	OP_GLOAD_FLD,
	OP_GLOAD_ENT,
	OP_GLOAD_S,
	OP_GLOAD_FNC,		//210

//back to ones that we do use.
	OP_BOUNDCHECK,
	OP_UNUSED,	//used to be OP_STOREP_P, which is now emulated with OP_STOREP_I, fteqcc nor fte generated it
	OP_PUSH,	//push 4octets onto the local-stack (which is ALWAYS poped on function return). Returns a pointer.
	OP_POP,		//pop those ones that were pushed (don't over do it). Needs assembler.

	OP_SWITCH_I,//hmm.
	OP_GLOAD_V,
//r3349+
	OP_IF_F,		//compares as an actual float, instead of treating -0 as positive.
	OP_IFNOT_F,

//r5697+
	OP_STOREF_V,	//3 elements...
	OP_STOREF_F,	//1 fpu element...
	OP_STOREF_S,	//1 string reference
	OP_STOREF_I,	//1 non-string reference/int

	OP_NUMOPS,

	OP_BREAKPOINT = 0x10000000
};

typedef uint32_t opcode_t;

typedef struct
{
	global_t	a, b, c;
} operands_t;

typedef void(*qcvm_opcode_func_t) (qcvm_t *vm, const operands_t operands, int *depth);

extern qcvm_opcode_func_t codeFuncs[];