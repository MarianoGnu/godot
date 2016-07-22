#ifndef GV_FUNCTIONS_H
#define GV_FUNCTIONS_H

#include "core/variant.h"

class GVFunctions {
public:

	enum Function {
		MATH_SIN,
		MATH_COS,
		MATH_TAN,
		MATH_SINH,
		MATH_COSH,
		MATH_TANH,
		MATH_ASIN,
		MATH_ACOS,
		MATH_ATAN,
		MATH_ATAN2,
		MATH_SQRT,
		MATH_FMOD,
		MATH_FPOSMOD,
		MATH_FLOOR,
		MATH_CEIL,
		MATH_ROUND,
		MATH_ABS,
		MATH_SIGN,
		MATH_POW,
		MATH_LOG,
		MATH_EXP,
		MATH_ISNAN,
		MATH_ISINF,
		MATH_EASE,
		MATH_DECIMALS,
		MATH_STEPIFY,
		MATH_LERP,
		MATH_DECTIME,
		MATH_RANDOMIZE,
		MATH_RAND,
		MATH_RANDF,
		MATH_RANDOM,
		MATH_SEED,
		MATH_RANDSEED,
		MATH_DEG2RAD,
		MATH_RAD2DEG,
		MATH_LINEAR2DB,
		MATH_DB2LINEAR,
		LOGIC_MAX,
		LOGIC_MIN,
		LOGIC_CLAMP,
		LOGIC_NEAREST_PO2,
		OBJ_WEAKREF,
		FUNC_FUNCREF,
		TYPE_CONVERT,
		TYPE_OF,
		TYPE_EXISTS,
		TEXT_STR,
		TEXT_PRINT,
		TEXT_PRINT_TABBED,
		TEXT_PRINT_SPACED,
		TEXT_PRINTERR,
		TEXT_PRINTRAW,
		VAR_TO_STR,
		STR_TO_VAR,
		VAR_TO_BYTES,
		BYTES_TO_VAR,
		GEN_RANGE,
		RESOURCE_LOAD,
		INST2DICT,
		DICT2INST,
		HASH,
		COLOR8,
		PRINT_STACK,
		INSTANCE_FROM_ID,
		FUNC_MAX

	};

	static const char *get_func_name(Function p_func);
	static void call(Function p_func,const Variant **p_args,int p_arg_count,Variant &r_ret,Variant::CallError &r_error);
	static bool is_deterministic(Function p_func);
	static MethodInfo get_info(Function p_func);
};

#endif // GV_FUNCTIONS_H
