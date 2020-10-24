#pragma once

#define OPLIST(f) \
	f(OP_DONE), \
	f(OP_MUL_F), \
	f(OP_MUL_V), \
	f(OP_MUL_FV), \
	f(OP_MUL_VF), \
	f(OP_DIV_F), \
	f(OP_ADD_F), \
	f(OP_ADD_V), \
	f(OP_SUB_F), \
	f(OP_SUB_V), \
\
	f(OP_EQ_F), \
	f(OP_EQ_V), \
	f(OP_EQ_S), \
	f(OP_EQ_E), \
	f(OP_EQ_FNC), \
\
	f(OP_NE_F), \
	f(OP_NE_V), \
	f(OP_NE_S), \
	f(OP_NE_E), \
	f(OP_NE_FNC), \
\
	f(OP_LE_F), \
	f(OP_GE_F), \
	f(OP_LT_F), \
	f(OP_GT_F), \
\
	f(OP_LOAD_F), \
	f(OP_LOAD_V), \
	f(OP_LOAD_S), \
	f(OP_LOAD_ENT), \
	f(OP_LOAD_FLD), \
	f(OP_LOAD_FNC), \
\
	f(OP_ADDRESS), \
\
	f(OP_STORE_F), \
	f(OP_STORE_V), \
	f(OP_STORE_S), \
	f(OP_STORE_ENT), \
	f(OP_STORE_FLD), \
	f(OP_STORE_FNC), \
\
	f(OP_STOREP_F), \
	f(OP_STOREP_V), \
	f(OP_STOREP_S), \
	f(OP_STOREP_ENT), \
	f(OP_STOREP_FLD), \
	f(OP_STOREP_FNC), \
\
	f(OP_RETURN), \
	f(OP_NOT_F), \
	f(OP_NOT_V), \
	f(OP_NOT_S), \
	f(OP_NOT_ENT), \
	f(OP_NOT_FNC), \
	f(OP_IF_I), \
	f(OP_IFNOT_I), \
	f(OP_CALL0), \
	f(OP_CALL1), \
	f(OP_CALL2), \
	f(OP_CALL3), \
	f(OP_CALL4), \
	f(OP_CALL5), \
	f(OP_CALL6), \
	f(OP_CALL7), \
	f(OP_CALL8), \
	f(OP_STATE), \
	f(OP_GOTO), \
	f(OP_AND_F), \
	f(OP_OR_F), \
\
	f(OP_BITAND_F), \
	f(OP_BITOR_F), \
\
	f(OP_MULSTORE_F), \
	f(OP_MULSTORE_VF), \
	f(OP_MULSTOREP_F), \
	f(OP_MULSTOREP_VF), \
\
	f(OP_DIVSTORE_F), \
	f(OP_DIVSTOREP_F), \
\
	f(OP_ADDSTORE_F), \
	f(OP_ADDSTORE_V), \
	f(OP_ADDSTOREP_F), \
	f(OP_ADDSTOREP_V), \
\
	f(OP_SUBSTORE_F), \
	f(OP_SUBSTORE_V), \
	f(OP_SUBSTOREP_F), \
	f(OP_SUBSTOREP_V), \
\
	f(OP_FETCH_GBL_F), \
	f(OP_FETCH_GBL_V), \
	f(OP_FETCH_GBL_S), \
	f(OP_FETCH_GBL_E), \
	f(OP_FETCH_GBL_FNC), \
\
	f(OP_CSTATE), \
	f(OP_CWSTATE), \
\
	f(OP_THINKTIME), \
\
	f(OP_BITSETSTORE_F), \
	f(OP_BITSETSTOREP_F), \
	f(OP_BITCLRSTORE_F), \
	f(OP_BITCLRSTOREP_F), \
\
	f(OP_RAND0), \
	f(OP_RAND1), \
	f(OP_RAND2), \
	f(OP_RANDV0), \
	f(OP_RANDV1), \
	f(OP_RANDV2), \
\
	f(OP_SWITCH_F), \
	f(OP_SWITCH_V), \
	f(OP_SWITCH_S), \
	f(OP_SWITCH_E), \
	f(OP_SWITCH_FNC), \
\
	f(OP_CASE), \
	f(OP_CASERANGE), \
\
	f(OP_CALL1H),\
	f(OP_CALL2H),\
	f(OP_CALL3H),\
	f(OP_CALL4H),\
	f(OP_CALL5H),\
	f(OP_CALL6H),\
	f(OP_CALL7H),\
	f(OP_CALL8H),\
\
	f(OP_STORE_I), \
	f(OP_STORE_IF), \
	f(OP_STORE_FI), \
\
	f(OP_ADD_I), \
	f(OP_ADD_FI), \
	f(OP_ADD_IF), \
\
	f(OP_SUB_I), \
	f(OP_SUB_FI), \
	f(OP_SUB_IF), \
\
	f(OP_CONV_ITOF), \
	f(OP_CONV_FTOI), \
	f(OP_CP_ITOF), \
	f(OP_CP_FTOI), \
	f(OP_LOAD_I), \
	f(OP_STOREP_I), \
	f(OP_STOREP_IF), \
	f(OP_STOREP_FI), \
\
	f(OP_BITAND_I), \
	f(OP_BITOR_I), \
\
	f(OP_MUL_I), \
	f(OP_DIV_I), \
	f(OP_EQ_I), \
	f(OP_NE_I), \
\
	f(OP_IFNOT_S), \
	f(OP_IF_S), \
\
	f(OP_NOT_I), \
\
	f(OP_DIV_VF), \
\
	f(OP_BITXOR_I), \
	f(OP_RSHIFT_I), \
	f(OP_LSHIFT_I), \
\
	f(OP_GLOBALADDRESS), \
	f(OP_ADD_PIW), \
\
	f(OP_LOADA_F), \
	f(OP_LOADA_V), \
	f(OP_LOADA_S), \
	f(OP_LOADA_ENT), \
	f(OP_LOADA_FLD), \
	f(OP_LOADA_FNC), \
	f(OP_LOADA_I), \
\
	f(OP_STORE_P), \
	f(OP_LOAD_P), \
\
	f(OP_LOADP_F), \
	f(OP_LOADP_V), \
	f(OP_LOADP_S), \
	f(OP_LOADP_ENT), \
	f(OP_LOADP_FLD), \
	f(OP_LOADP_FNC), \
	f(OP_LOADP_I), \
\
	f(OP_LE_I), \
	f(OP_GE_I), \
	f(OP_LT_I), \
	f(OP_GT_I), \
\
	f(OP_LE_IF), \
	f(OP_GE_IF), \
	f(OP_LT_IF), \
	f(OP_GT_IF), \
\
	f(OP_LE_FI), \
	f(OP_GE_FI), \
	f(OP_LT_FI), \
	f(OP_GT_FI), \
\
	f(OP_EQ_IF), \
	f(OP_EQ_FI), \
\
	f(OP_ADD_SF), \
	f(OP_SUB_S), \
	f(OP_STOREP_C), \
	f(OP_LOADP_C), \
\
	f(OP_MUL_IF), \
	f(OP_MUL_FI), \
	f(OP_MUL_VI), \
	f(OP_MUL_IV), \
	f(OP_DIV_IF), \
	f(OP_DIV_FI), \
	f(OP_BITAND_IF), \
	f(OP_BITOR_IF), \
	f(OP_BITAND_FI), \
	f(OP_BITOR_FI), \
	f(OP_AND_I), \
	f(OP_OR_I), \
	f(OP_AND_IF), \
	f(OP_OR_IF), \
	f(OP_AND_FI), \
	f(OP_OR_FI), \
	f(OP_NE_IF), \
	f(OP_NE_FI), \
\
	f(OP_GSTOREP_I), \
	f(OP_GSTOREP_F), \
	f(OP_GSTOREP_ENT), \
	f(OP_GSTOREP_FLD), \
	f(OP_GSTOREP_S), \
	f(OP_GSTOREP_FNC),	\
	f(OP_GSTOREP_V), \
	f(OP_GADDRESS), \
	f(OP_GLOAD_I), \
	f(OP_GLOAD_F), \
	f(OP_GLOAD_FLD), \
	f(OP_GLOAD_ENT), \
	f(OP_GLOAD_S), \
	f(OP_GLOAD_FNC), \
\
	f(OP_BOUNDCHECK), \
	f(OP_UNUSED), \
	f(OP_PUSH), \
	f(OP_POP), \
\
	f(OP_SWITCH_I), \
	f(OP_GLOAD_V), \
	f(OP_IF_F), \
	f(OP_IFNOT_F), \
\
	f(OP_STOREF_V), \
	f(OP_STOREF_F), \
	f(OP_STOREF_S), \
	f(OP_STOREF_I), \
\
	f(OP_STOREP_B), \
	f(OP_LOADP_B), \
\
	f(OP_NUMOPS)

enum
{
#define U(n)	n
OPLIST(U)
#undef U
};

static const char *opcode_names[] =
{
#define U(n)	#n
OPLIST(U)
#undef U
};

#define OP_BREAKPOINT 0x10000000

#ifndef OPCODES_ONLY
typedef void(*qcvm_opcode_func_t) (qcvm_t *vm, const qcvm_operands_t operands, int *depth);

#include "vm_opcodes.c.h"
#endif