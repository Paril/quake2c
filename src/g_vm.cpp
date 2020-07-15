#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"

struct QCOffset
{
	uint32_t	offset;
	uint32_t	size;
};

struct QCHeader
{
	uint32_t	version;
	uint16_t	crc;
	uint16_t	skip;

	struct {
		QCOffset	statement;
		QCOffset	definition;
		QCOffset	field;
		QCOffset	function;
		QCOffset	string;
		QCOffset	globals;
	} sections;
};

QCVMStringList::QCVMStringList(struct QCVM &invm) :
	vm(invm)
{
}

string_t QCVMStringList::Allocate()
{
	if (free_indices.size())
	{
		string_t top = free_indices.top();
		free_indices.pop();
		return top;
	}

	return static_cast<string_t>(-static_cast<int32_t>(strings.size() + 1));
}

string_t QCVMStringList::StoreStatic(const char **resolver)
{
	if (constant_storage.contains(resolver))
		return constant_storage.at(resolver);

	string_t id = Allocate();
	constant_storage.emplace(resolver, id);
	strings.emplace(id, resolver);
	return id;
}

string_t QCVMStringList::StoreStatic(const std::string_view &view)
{
	string_t id;

	if (constant_storage.contains(view.data()))
		id = constant_storage.at(view.data());
	else
	{
		id = Allocate();
		constant_storage.emplace(view.data(), id);
	}

	strings.emplace(id, view);
	return id;
}

string_t QCVMStringList::StoreRefCounted(const std::string &str)
{
	string_t id = Allocate();
	
	PrintTraceExt(vm, "REFSTRING STORE %10s -> %i", str.data(), id);

	strings.emplace(id, (QCVMRefCountString) {
		std::move(str),
		0
	});
	return id;
}

void QCVMStringList::Unstore(const string_t &id)
{
	assert(strings.contains(id));
		
	const StringType &str = strings.at(id);
		
	if (std::holds_alternative<const char **>(str))
		constant_storage.erase(std::get<const char **>(str));
		
	assert(!std::holds_alternative<QCVMRefCountString>(str) || !std::get<QCVMRefCountString>(str).ref_count);
	
	PrintTraceExt(vm, "REFSTRING UNSTORE %i", id);
		
	strings.erase(id);
	free_indices.push(id);
}

size_t QCVMStringList::Length(const string_t &id) const
{
	assert(strings.contains(id));
		
	const StringType &str = strings.at(id);
		
	if (std::holds_alternative<std::string_view>(str))
		return std::get<std::string_view>(str).length();
	else if (std::holds_alternative<QCVMRefCountString>(str))
		return std::get<QCVMRefCountString>(str).str.length();
		
	return strlen(*std::get<const char **>(str));
}

const char *QCVMStringList::GetStatic(const string_t &id) const
{
	assert(strings.contains(id));
		
	const StringType &str = strings.at(id);

	assert(std::holds_alternative<std::string_view>(str) || std::holds_alternative<const char **>(str));

	if (std::holds_alternative<std::string_view>(str))
		return std::get<std::string_view>(str).data();

	return *std::get<const char **>(str);
}

std::string &QCVMStringList::GetRefCounted(const string_t &id)
{
	assert(strings.contains(id));
		
	StringType &str = strings.at(id);

	assert(std::holds_alternative<QCVMRefCountString>(str));

	return std::get<QCVMRefCountString>(str).str;
}

const char *QCVMStringList::Get(const string_t &id) const
{
	assert(strings.contains(id));

	if (!strings.contains(id))
		return "";

	const StringType &str = strings.at(id);
		
	if (std::holds_alternative<std::string_view>(str))
		return std::get<std::string_view>(str).data();
	else if (std::holds_alternative<const char **>(str))
		return *std::get<const char **>(str);

	return std::get<QCVMRefCountString>(str).str.c_str();
}

void QCVMStringList::AcquireRefCounted(const string_t &id)
{
	__attribute__((unused)) auto timer = vm.CreateTimer(StringAcquire);

	assert(strings.contains(id));

	StringType &str = strings.at(id);

	assert(std::holds_alternative<QCVMRefCountString>(str));

	auto &ref = std::get<QCVMRefCountString>(str);

	ref.ref_count++;
	
	PrintTraceExt(vm, "REFSTRING ACQUIRE %i (now %i)", id, ref.ref_count);
}

void QCVMStringList::ReleaseRefCounted(const string_t &id)
{
	__attribute__((unused)) auto timer = vm.CreateTimer(StringRelease);

	assert(strings.contains(id));

	StringType &str = strings.at(id);

	assert(std::holds_alternative<QCVMRefCountString>(str));

	auto &ref = std::get<QCVMRefCountString>(str);

	assert(ref.ref_count);
		
	ref.ref_count--;
	
	PrintTraceExt(vm, "REFSTRING RELEASE %i (now %i)", id, ref.ref_count);

	if (!ref.ref_count)
		Unstore(id);
}

// mark a memory address as containing a reference to the specified string.
// increases ref count by 1 and shoves it into the list.
void QCVMStringList::MarkRefCopy(const string_t &id, const void *ptr)
{
	__attribute__((unused)) auto timer = vm.CreateTimer(StringMark);

	if (ref_storage.contains(ptr))
	{
		CheckRefUnset(ptr, 1);

		// it's *possible* for a seemingly no-op to occur in some cases
		// (for instance, a call into function which copies PARM0 into locals+0, then
		// copies locals+0 back into PARM0 for calling a function). because PARM0
		// doesn't release its ref until its value changes, we treat this as a no-op.
		// if we released every time the value changes (even to the same value it already
		// had) this would effectively be the same behavior.
		string_t current_id;

		if (HasRef(ptr, current_id) && id == current_id)
			return;
	}

	// increase ref count
	AcquireRefCounted(id);

	// mark
	ref_storage.emplace(ptr, id);
	
	PrintTraceExt(vm, "REFSTRING MARK %i -> %x", id, ptr);
}

void QCVMStringList::CheckRefUnset(const void *ptr, const size_t &span)
{
	__attribute__((unused)) auto timer = vm.CreateTimer(StringCheckUnset);

	for (size_t i = 0; i < span; i++)
	{
		auto gptr = reinterpret_cast<const global_t *>(ptr) + i;

		if (!ref_storage.contains(gptr))
			continue;

		auto old = ref_storage.at(gptr);
		auto newstr = *reinterpret_cast<const string_t *>(gptr);

		// still here, so we probably just copied to ourselves or something
		if (newstr == old)
			continue;

		// not here! release and unmark
		ReleaseRefCounted(old);
		PrintTraceExt(vm, "REFSTRING UNSET %i -> %x", old, gptr);
		ref_storage.erase(gptr);
	}
}

bool QCVMStringList::HasRef(const void *ptr, string_t &id)
{
	__attribute__((unused)) auto timer = vm.CreateTimer(StringHasRef);

	if (ref_storage.contains(ptr))
	{
		id = ref_storage.at(ptr);
		return true;
	}

	return false;
}

void QCVMStringList::MarkIfHasRef(const void *src_ptr, const void *dst_ptr)
{
	__attribute__((unused)) auto timer = vm.CreateTimer(StringMarkIfHasRef);

	if (ref_storage.contains(src_ptr))
		MarkRefCopy(ref_storage.at(src_ptr), dst_ptr);
}

bool QCVMStringList::IsRefCounted(const string_t &id)
{
	return strings.contains(id) && std::holds_alternative<QCVMRefCountString>(strings.at(id));
}

QCVMBuiltinList::QCVMBuiltinList(QCVM &invm) :
	vm(invm)
{
}

#include <sstream>

std::string ParseFormat(const char *format, QCVM &vm, const uint8_t &start)
{
	enum class ParseToken
	{
		None,
		Specifier,
		Skip
	};

	std::stringstream buf;
	size_t i = 0;
	const size_t len = strlen(format);
	static char format_buffer[17];
	uint8_t param_index = start;

	while (true)
	{
		const char *next = strchr(format + i, '%');

		if (!next)
		{
			buf.write(format + i, (format + len) - (format + i));
			break;
		}

		buf.write(format + i, (next - format) - i);
		i = next - format;

		const char *specifier_start = next;
		auto state = ParseToken::None;

		while (state < ParseToken::Specifier)
		{
			next++;
			i++;

			// check specifier
			switch (*next)
			{
			case 'd':
			case 'i':
			case 'o':
			case 'x':
			case 'X':
			case 'f':
			case 'F':
			case 'e':
			case 'E':
			case 'g':
			case 'G':
			case 'a':
			case 'A':
			case 'c':
			case 's':
			case 'p':
				state = ParseToken::Specifier;
				continue;
			case '%':
				buf.put('%');
				state = ParseToken::Skip;
				continue;
			}
		}

		if (state == ParseToken::Specifier)
		{
			Q_strlcpy(format_buffer, specifier_start, min(sizeof(format_buffer), static_cast<size_t>((next - specifier_start) + 1 + 1)));

			switch (*next)
			{
			case 'd':
			case 'i':
			case 'o':
			case 'x':
			case 'X':
			case 'c':
			case 'p':
				buf << vas(format_buffer, vm.ArgvInt32(param_index++));
				break;
			case 'f':
			case 'F':
			case 'e':
			case 'E':
			case 'g':
			case 'G':
			case 'a':
			case 'A':
				buf << vas(format_buffer, vm.ArgvFloat(param_index++));
				break;
			case 's':
				buf << vas(format_buffer, vm.ArgvString(param_index++));
				break;
			}
		}

		i++;
	}

	return buf.str();
}

void QCVMBuiltinList::Register(const char *name, QCBuiltin builtin)
{
	func_t id = static_cast<func_t>(this->next_id);
	this->next_id--;
	this->builtins.emplace(id, builtin);

	for (auto &func : vm.functions)
	{
		if (func.id || func.name_index == string_t::STRING_EMPTY)
			continue;

		if (strcmp(vm.GetString(func.name_index), name) == 0)
		{
			func.id = static_cast<int32_t>(id);
			break;
		}
	}
}

QCVMFieldWrapList::QCVMFieldWrapList(QCVM &invm) :
	vm(invm)
{
}

void QCVMFieldWrapList::Register(const char *field_name, const size_t &field_offset, const size_t &client_offset, QCVMFieldWrapper setter)
{
	for (auto &f : vm.fields)
	{
		if (f.name_index == string_t::STRING_EMPTY)
			continue;
		else if (strcmp(vm.GetString(f.name_index), field_name))
			continue;

		wraps.emplace((static_cast<uint32_t>(f.global_index) + field_offset) * sizeof(global_t), (QCVMFieldWrap) {
			&f,
			client_offset,
			setter
		});
		return;
	}

	vm.Error("missing field to wrap");
}

QCVM::QCVM() :
	dynamic_strings(*this),
	builtins(*this),
	field_wraps(*this)
{
}

std::string QCVM::StackEntry(const QCStack &stack)
{
	if (!linenumbers.size())
		return "dunno:dunno";

	if (!stack.function)
		return "C code";

	const char *func = GetString(stack.function->name_index);

	if (!strlen(func))
		func = "dunno";

	return vas("%s:%i(@%u)", func, linenumbers[stack.statement - statements.data()], stack.statement - statements.data());
}

std::string QCVM::StackTrace()
{
	std::string str = StackEntry(*state.current);

	for (auto it = state.stack.rbegin(); it != state.stack.rend(); it++)
	{
		const auto &s = *it;

		if (!s.function)
			break;

		str += "\n" + StackEntry(s);
	}

	return str;
}

struct operand
{
	uint16_t	arg;

	inline operator global_t() const
	{
		return static_cast<global_t>(arg);
	}
};

using OPCodeFunc = void(*)(QCVM &vm, const std::array<operand, 3> &operands, int &depth);

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_MUL(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a * b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_DIV(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a / b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_ADD(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a + b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_SUB(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a - b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_EQ(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a == b);
}

template<>
inline void F_OP_EQ<string_t, string_t, vec_t>(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<string_t>(operands[0]);
	const auto &b = vm.GetGlobal<string_t>(operands[1]);

	if (a == b)
		vm.SetGlobal<vec_t>(operands[2], 1);
	else
		vm.SetGlobal<vec_t>(operands[2], !strcmp(vm.GetString(a), vm.GetString(b)));
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_NE(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a != b);
}

template<>
inline void F_OP_NE<string_t, string_t, vec_t>(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<string_t>(operands[0]);
	const auto &b = vm.GetGlobal<string_t>(operands[1]);

	if (a == b)
		vm.SetGlobal<vec_t>(operands[2], 0);
	else
		vm.SetGlobal<vec_t>(operands[2], !!strcmp(vm.GetString(a), vm.GetString(b)));
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_LE(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a <= b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_GE(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a >= b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_LT(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a < b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_GT(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a > b);
}

template<typename TType>
inline void F_OP_LOAD(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	auto &ent = *vm.EntToEntity(vm.GetGlobal<ent_t>(operands[0]), true);
	auto &field_offset = vm.GetGlobal<int32_t>(operands[1]);
	auto &field_value = *reinterpret_cast<TType *>(reinterpret_cast<int32_t*>(&ent) + field_offset);
	vm.SetGlobal(operands[2], field_value);

	vm.dynamic_strings.MarkIfHasRef<sizeof(TType) / sizeof(global_t)>(&field_value, vm.GetGlobalByIndex(operands[2]));
}

inline void F_OP_ADDRESS(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	auto &ent = *vm.EntToEntity(vm.GetGlobal<ent_t>(operands[0]), true);
	auto field = vm.GetGlobal<int32_t>(operands[1]);
	vm.SetGlobal(operands[2], vm.EntityFieldAddress(ent, field));
}

template<typename TType>
inline void F_OP_STORE(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	vm.CopyGlobal<TType>(operands[1], operands[0]);
}

template<typename TType, typename TResult>
inline void F_OP_STORE(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	vm.CopyGlobal<TType, TResult>(operands[1], operands[0]);
}

template<typename TType, typename TResult = TType>
inline void F_OP_STOREP(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &address = vm.GetGlobal<int32_t>(operands[1]);
	const auto &value = vm.GetGlobal<TType>(operands[0]);

	if (!vm.PointerValid(address, sizeof(TResult)))
		vm.Error("invalid address");

	constexpr size_t span = sizeof(TType) / sizeof(global_t);

	auto address_ptr = reinterpret_cast<TResult *>(address);
	*address_ptr = value;
	vm.dynamic_strings.CheckRefUnset(address_ptr, span);
				
	auto &ent = vm.AddressToEntity(address);
	const auto &field = vm.AddressToField(ent, address);

	for (size_t i = 0; i < span; i++)
		vm.field_wraps.WrapField(ent, field + (i * sizeof(global_t)), &value + i);

	vm.dynamic_strings.MarkIfHasRef<span>(&value, address_ptr);
}

template<typename TType, typename TResult>
inline void F_NOT(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TType>(operands[0]);

	vm.SetGlobal<TResult>(operands[2], !a);
}

template<>
inline void F_NOT<string_t, vec_t>(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<string_t>(operands[0]);

	vm.SetGlobal<vec_t>(operands[2], a == string_t::STRING_EMPTY || !*vm.GetString(a));
}

template<typename TType>
inline void F_IF(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	if (vm.GetGlobal<TType>(operands[0]))
	{
		auto &current = *(vm.state.current);
		current.statement += static_cast<int16_t>(current.statement->args[1]) - 1;
	}
}

template<>
inline void F_IF<string_t>(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &s = vm.GetGlobal<string_t>(operands[0]);

	if (s != string_t::STRING_EMPTY && *vm.GetString(s))
	{
		auto &current = *(vm.state.current);
		current.statement += static_cast<int16_t>(current.statement->args[1]) - 1;
	}
}

template<typename TType>
inline void F_IFNOT(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	if (!vm.GetGlobal<TType>(operands[0]))
	{
		auto &current = *(vm.state.current);
		current.statement += static_cast<int16_t>(current.statement->args[1]) - 1;
	}
}

template<>
inline void F_IFNOT<string_t>(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &s = vm.GetGlobal<string_t>(operands[0]);

	if (s == string_t::STRING_EMPTY || !*vm.GetString(s))
	{
		auto &current = *(vm.state.current);
		current.statement += static_cast<int16_t>(current.statement->args[1]) - 1;
	}
}

template<size_t argc, bool hexen>
inline void F_CALL(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	if constexpr (argc >= 2 && hexen)
		vm.CopyGlobal<std::array<global_t, 3>>(global_t::PARM1, operands[2]);

	if constexpr (argc >= 1 && hexen)
		vm.CopyGlobal<std::array<global_t, 3>>(global_t::PARM0, operands[1]);

	const int32_t &enter_func = vm.GetGlobal<int32_t>(operands[0]);

	vm.state.argc = argc;
	if (!enter_func)
		vm.Error("NULL function");

#ifdef ALLOW_PROFILING
	auto &current = *(vm.state.current);
	current.profile->fields[NumFuncCalls]++;
#endif

	QCFunction &call = vm.functions[enter_func];

	if (!call.id)
		vm.Error("Tried to call missing function %s", vm.GetString(call.name_index));

	if (call.id < 0)
	{
		// negative statements are built in functions
		vm.CallBuiltin(call);
		return;
	}

	depth++;
	vm.Enter(call);
}

inline void F_GOTO(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	auto &current = *(vm.state.current);
	current.statement += static_cast<int16_t>(current.statement->args[0]) - 1;
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_AND(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a && b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_OR(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a || b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_BITAND(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = static_cast<int32_t>(vm.GetGlobal<TLeft>(operands[0]));
	const auto &b = static_cast<int32_t>(vm.GetGlobal<TRight>(operands[1]));

	vm.SetGlobal<TResult>(operands[2], a & b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_BITOR(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = static_cast<int32_t>(vm.GetGlobal<TLeft>(operands[0]));
	const auto &b = static_cast<int32_t>(vm.GetGlobal<TRight>(operands[1]));

	vm.SetGlobal<TResult>(operands[2], a | b);
}

inline void F_OP_ITOF(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	vm.SetGlobal<vec_t>(operands[2], vm.GetGlobal<int32_t>(operands[0]));
}

inline void F_OP_FTOI(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	vm.SetGlobal<int32_t>(operands[2], vm.GetGlobal<vec_t>(operands[0]));
}

inline void F_OP_P_ITOF(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &address = vm.GetGlobal<int32_t>(operands[0]);

	if (!vm.PointerValid(address))
		vm.Error("invalid address");

	vm.SetGlobal<vec_t>(operands[2], *reinterpret_cast<int32_t *>(address));
}

inline void F_OP_P_FTOI(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &address = vm.GetGlobal<int32_t>(operands[0]);

	if (!vm.PointerValid(address))
		vm.Error("invalid address");

	vm.SetGlobal<int32_t>(operands[2], *reinterpret_cast<vec_t *>(address));
}

inline void F_OP_BITXOR(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<int32_t>(operands[0]);
	const auto &b = vm.GetGlobal<int32_t>(operands[1]);

	vm.SetGlobal<int32_t>(operands[2], a ^ b);
}

inline void F_OP_RSHIFT(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<int32_t>(operands[0]);
	const auto &b = vm.GetGlobal<int32_t>(operands[1]);

	vm.SetGlobal<int32_t>(operands[2], a >> b);
}

inline void F_OP_LSHIFT(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<int32_t>(operands[0]);
	const auto &b = vm.GetGlobal<int32_t>(operands[1]);

	vm.SetGlobal<int32_t>(operands[2], a << b);
}

inline void F_OP_GLOBALADDRESS(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	vm.SetGlobal(operands[2], reinterpret_cast<int32_t>(&vm.GetGlobal<int32_t>(operands[0]) + vm.GetGlobal<int32_t>(operands[1])));
}

inline void F_OP_ADD_PIW(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &a = vm.GetGlobal<int32_t>(operands[0]);
	const auto &b = vm.GetGlobal<int32_t>(operands[1]);
	const ptrdiff_t address = a + (b * sizeof(float));

	vm.SetGlobal(operands[2], address);
}

template<typename TType>
inline void F_OP_LOADA(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const ptrdiff_t address = operands[0].arg + vm.GetGlobal<int32_t>(operands[1]);
			
	if (!vm.PointerValid(reinterpret_cast<ptrdiff_t>(vm.global_data + address), sizeof(TType)))
		vm.Error("Invalid pointer %x", address);

	constexpr size_t span = sizeof(TType) / sizeof(global_t);
			
	auto &field_value = *reinterpret_cast<TType *>(vm.global_data + address);
	vm.SetGlobal(operands[2], field_value);

	vm.dynamic_strings.MarkIfHasRef<span>(&field_value, vm.GetGlobalByIndex(operands[2]));
}

template<typename TType>
inline void F_OP_LOADP(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const ptrdiff_t address = vm.GetGlobal<int32_t>(operands[0]) + (vm.GetGlobal<int32_t>(operands[1]) * 4);

	if (!vm.PointerValid(address, sizeof(TType)))
		vm.Error("Invalid pointer %x", address);

	constexpr size_t span = sizeof(TType) / sizeof(global_t);
			
	auto &field_value = *reinterpret_cast<TType *>(address);
	vm.SetGlobal(operands[2], field_value);

	vm.dynamic_strings.MarkIfHasRef<span>(&field_value, vm.GetGlobalByIndex(operands[2]));
}

inline void F_OP_BOUNDCHECK(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
#if _DEBUG
	const auto &a = vm.GetGlobal<uint32_t>(operands[0]);
	const auto &b = operands[1].arg;
	const auto &c = operands[2].arg;

	if (a < c || a >= b)
		vm.Error("bounds check failed");
#endif
}

template<typename TType, typename TMul>
inline void F_OP_MULSTOREP(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &address = vm.GetGlobal<int32_t>(operands[1]);

	if (!vm.PointerValid(address))
		vm.Error("bad pointer");

	TType *f = reinterpret_cast<TType *>(address);

	const auto &a = vm.GetGlobal<TMul>(operands[0]);

	vm.SetGlobal<TType>(operands[2], (*f) *= a);
}

template<typename TType, typename TMul>
inline void F_OP_DIVSTOREP(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &address = vm.GetGlobal<int32_t>(operands[1]);

	if (!vm.PointerValid(address))
		vm.Error("bad pointer");

	TType *f = reinterpret_cast<TType *>(address);

	const auto &a = vm.GetGlobal<TMul>(operands[0]);

	vm.SetGlobal<TType>(operands[2], (*f) /= a);
}

template<typename TType, typename TMul>
inline void F_OP_SUBSTOREP(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &address = vm.GetGlobal<int32_t>(operands[1]);

	if (!vm.PointerValid(address))
		vm.Error("bad pointer");

	TType *f = reinterpret_cast<TType *>(address);

	const auto &a = vm.GetGlobal<TMul>(operands[0]);

	vm.SetGlobal<TType>(operands[2], (*f) -= a);
}

template<typename TType, typename TMul>
inline void F_OP_ADDSTOREP(QCVM &vm, const std::array<operand, 3> &operands, int &depth)
{
	const auto &address = vm.GetGlobal<int32_t>(operands[1]);

	if (!vm.PointerValid(address))
		vm.Error("bad pointer");

	TType *f = reinterpret_cast<TType *>(address);

	const auto &a = vm.GetGlobal<TMul>(operands[0]);

	vm.SetGlobal<TType>(operands[2], (*f) += a);
}

static OPCodeFunc codeFuncs[] = {
	[OP_DONE] = [](auto vm, auto operands, auto depth) { vm.Leave(); depth--; },

	[OP_MUL_F] = F_OP_MUL<vec_t, vec_t, vec_t>,
	[OP_MUL_V] = F_OP_MUL<vec3_t, vec3_t, vec_t>,
	[OP_MUL_FV] = F_OP_MUL<vec_t, vec3_t, vec3_t>,
	[OP_MUL_VF] = F_OP_MUL<vec3_t, vec_t, vec3_t>,
	[OP_MUL_I] = F_OP_MUL<int32_t, int32_t, int32_t>,
	[OP_MUL_IF] = F_OP_MUL<int32_t, vec_t, vec_t>,
	[OP_MUL_FI] = F_OP_MUL<vec_t, int32_t, vec_t>,
	[OP_MUL_VI] = F_OP_MUL<vec3_t, int32_t, vec3_t>,
	[OP_MUL_IV] = F_OP_MUL<int32_t, vec3_t, vec3_t>,
	
	[OP_DIV_F] = F_OP_DIV<vec_t, vec_t, vec_t>,
	[OP_DIV_I] = F_OP_DIV<int32_t, int32_t, int32_t>,
	[OP_DIV_VF] = F_OP_DIV<vec3_t, vec_t, vec3_t>,
	[OP_DIV_IF] = F_OP_DIV<int32_t, vec_t, vec_t>,
	[OP_DIV_FI] = F_OP_DIV<vec_t, int32_t, vec_t>,
	
	[OP_ADD_F] = F_OP_ADD<vec_t, vec_t, vec_t>,
	[OP_ADD_V] = F_OP_ADD<vec3_t, vec3_t, vec3_t>,
	[OP_ADD_I] = F_OP_ADD<int32_t, int32_t, int32_t>,
	[OP_ADD_FI] = F_OP_ADD<vec_t, int32_t, vec_t>,
	[OP_ADD_IF] = F_OP_ADD<int32_t, vec_t, vec_t>,
	
	[OP_SUB_F] = F_OP_SUB<vec_t, vec_t, vec_t>,
	[OP_SUB_V] = F_OP_SUB<vec3_t, vec3_t, vec3_t>,
	[OP_SUB_I] = F_OP_SUB<int32_t, int32_t, int32_t>,
	[OP_SUB_FI] = F_OP_SUB<vec_t, int32_t, vec_t>,
	[OP_SUB_IF] = F_OP_SUB<int32_t, vec_t, vec_t>,
	
	[OP_EQ_F] = F_OP_EQ<vec_t, vec_t, vec_t>,
	[OP_EQ_V] = F_OP_EQ<vec3_t, vec3_t, vec_t>,
	[OP_EQ_S] = F_OP_EQ<string_t, string_t, vec_t>,
	[OP_EQ_E] = F_OP_EQ<ent_t, ent_t, vec_t>,
	[OP_EQ_FNC] = F_OP_EQ<func_t, func_t, vec_t>,
	[OP_EQ_I] = F_OP_EQ<int32_t, int32_t, int32_t>,
	[OP_EQ_IF] = F_OP_EQ<int32_t, vec_t, int32_t>,
	[OP_EQ_FI] = F_OP_EQ<vec_t, int32_t, int32_t>,

	[OP_NE_F] = F_OP_NE<vec_t, vec_t, vec_t>,
	[OP_NE_V] = F_OP_NE<vec3_t, vec3_t, vec_t>,
	[OP_NE_S] = F_OP_NE<string_t, string_t, vec_t>,
	[OP_NE_E] = F_OP_NE<ent_t, ent_t, vec_t>,
	[OP_NE_FNC] = F_OP_NE<func_t, func_t, vec_t>,
	[OP_NE_I] = F_OP_NE<int32_t, int32_t, int32_t>,
	[OP_NE_IF] = F_OP_NE<int32_t, vec_t, int32_t>,
	[OP_NE_FI] = F_OP_NE<vec_t, int32_t, int32_t>,
	
	[OP_LE_F] = F_OP_LE<vec_t, vec_t, vec_t>,
	[OP_LE_I] = F_OP_LE<int32_t, int32_t, int32_t>,
	[OP_LE_IF] = F_OP_LE<int32_t, vec_t, int32_t>,
	[OP_LE_FI] = F_OP_LE<vec_t, int32_t, int32_t>,
	
	[OP_GE_F] = F_OP_GE<vec_t, vec_t, vec_t>,
	[OP_GE_I] = F_OP_GE<int32_t, int32_t, int32_t>,
	[OP_GE_IF] = F_OP_GE<int32_t, vec_t, int32_t>,
	[OP_GE_FI] = F_OP_GE<vec_t, int32_t, int32_t>,
	
	[OP_LT_F] = F_OP_LT<vec_t, vec_t, vec_t>,
	[OP_LT_I] = F_OP_LT<int32_t, int32_t, int32_t>,
	[OP_LT_IF] = F_OP_LT<int32_t, vec_t, int32_t>,
	[OP_LT_FI] = F_OP_LT<vec_t, int32_t, int32_t>,
	
	[OP_GT_F] = F_OP_GT<vec_t, vec_t, vec_t>,
	[OP_GT_I] = F_OP_GT<int32_t, int32_t, int32_t>,
	[OP_GT_IF] = F_OP_GT<int32_t, vec_t, int32_t>,
	[OP_GT_FI] = F_OP_GT<vec_t, int32_t, int32_t>,
	
	[OP_LOAD_F] = F_OP_LOAD<int32_t>,
	[OP_LOAD_V] = F_OP_LOAD<vec3_t>,
	[OP_LOAD_S] = F_OP_LOAD<string_t>,
	[OP_LOAD_ENT] = F_OP_LOAD<ent_t>,
	[OP_LOAD_FLD] = F_OP_LOAD<int32_t>,
	[OP_LOAD_FNC] = F_OP_LOAD<func_t>,
	[OP_LOAD_I] = F_OP_LOAD<int32_t>,
	[OP_LOAD_P] = F_OP_LOAD<int32_t>,

	[OP_ADDRESS] = F_OP_ADDRESS,
	
	[OP_STORE_F] = F_OP_STORE<int32_t>,
	[OP_STORE_V] = F_OP_STORE<vec3_t>,
	[OP_STORE_S] = F_OP_STORE<string_t>,
	[OP_STORE_ENT] = F_OP_STORE<ent_t>,
	[OP_STORE_FLD] = F_OP_STORE<int32_t>,
	[OP_STORE_FNC] = F_OP_STORE<func_t>,
	[OP_STORE_I] = F_OP_STORE<int32_t>,
	[OP_STORE_IF] = F_OP_STORE<int32_t, vec_t>,
	[OP_STORE_FI] = F_OP_STORE<vec_t, int32_t>,
	[OP_STORE_P] = F_OP_STORE<int32_t>,
	
	[OP_STOREP_F] = F_OP_STOREP<vec_t>,
	[OP_STOREP_V] = F_OP_STOREP<vec3_t>,
	[OP_STOREP_S] = F_OP_STOREP<string_t>,
	[OP_STOREP_ENT] = F_OP_STOREP<ent_t>,
	[OP_STOREP_FLD] = F_OP_STOREP<int32_t>,
	[OP_STOREP_FNC] = F_OP_STOREP<func_t>,
	[OP_STOREP_I] = F_OP_STOREP<int32_t>,
	[OP_STOREP_IF] = F_OP_STOREP<int32_t, vec_t>,
	[OP_STOREP_FI] = F_OP_STOREP<vec_t, int32_t>,

	[OP_RETURN] = [](auto vm, auto operands, auto depth)
	{
		if (operands[0].arg)
			vm.template CopyGlobal<std::array<global_t, 3>>(global_t::RETURN, operands[0]);

		vm.Leave();
		depth--;
	},
		
	[OP_MULSTOREP_F] = F_OP_MULSTOREP<vec_t, vec_t>,
	[OP_MULSTOREP_VF] = F_OP_MULSTOREP<vec3_t, vec_t>,
		
	[OP_DIVSTOREP_F] = F_OP_DIVSTOREP<vec_t, vec_t>,
		
	[OP_ADDSTOREP_F] = F_OP_ADDSTOREP<vec_t, vec_t>,
	[OP_ADDSTOREP_V] = F_OP_ADDSTOREP<vec3_t, vec3_t>,
		
	[OP_SUBSTOREP_F] = F_OP_SUBSTOREP<vec_t, vec_t>,
	[OP_SUBSTOREP_V] = F_OP_SUBSTOREP<vec3_t, vec3_t>,
		
	[OP_NOT_F] = F_NOT<vec_t, vec_t>,
	[OP_NOT_V] = F_NOT<vec3_t, vec_t>,
	[OP_NOT_S] = F_NOT<string_t, vec_t>,
	[OP_NOT_FNC] = F_NOT</*func_t*/int32_t, vec_t>,
	[OP_NOT_ENT] = F_NOT</*ent_t*/int32_t, vec_t>,
	[OP_NOT_I] = F_NOT<int32_t, int32_t>,
		
	[OP_IF_I] = F_IF<int32_t>,
	[OP_IF_S] = F_IF<string_t>,
	[OP_IF_F] = F_IF<vec_t>,
		
	[OP_IFNOT_I] = F_IFNOT<int32_t>,
	[OP_IFNOT_S] = F_IFNOT<string_t>,
	[OP_IFNOT_F] = F_IFNOT<vec_t>,
		
	[OP_CALL0] = F_CALL<0, false>,
	[OP_CALL1] = F_CALL<1, false>,
	[OP_CALL2] = F_CALL<2, false>,
	[OP_CALL3] = F_CALL<3, false>,
	[OP_CALL4] = F_CALL<4, false>,
	[OP_CALL5] = F_CALL<5, false>,
	[OP_CALL6] = F_CALL<6, false>,
	[OP_CALL7] = F_CALL<7, false>,
	[OP_CALL8] = F_CALL<8, false>,
		
	[OP_CALL1H] = F_CALL<1, true>,
	[OP_CALL2H] = F_CALL<2, true>,
	[OP_CALL3H] = F_CALL<3, true>,
	[OP_CALL4H] = F_CALL<4, true>,
	[OP_CALL5H] = F_CALL<5, true>,
	[OP_CALL6H] = F_CALL<6, true>,
	[OP_CALL7H] = F_CALL<7, true>,
	[OP_CALL8H] = F_CALL<8, true>,

	[OP_GOTO] = F_GOTO,
		
	[OP_AND_F] = F_OP_AND<vec_t, vec_t, vec_t>,
	[OP_AND_I] = F_OP_AND<int32_t, int32_t, int32_t>,
	[OP_AND_IF] = F_OP_AND<int32_t, vec_t, int32_t>,
	[OP_AND_FI] = F_OP_AND<vec_t, int32_t, int32_t>,
		
	[OP_OR_F] = F_OP_OR<vec_t, vec_t, vec_t>,
	[OP_OR_I] = F_OP_OR<int32_t, int32_t, int32_t>,
	[OP_OR_IF] = F_OP_OR<int32_t, vec_t, int32_t>,
	[OP_OR_FI] = F_OP_OR<vec_t, int32_t, int32_t>,
		
	[OP_BITAND_F] = F_OP_BITAND<vec_t, vec_t, vec_t>,
	[OP_BITAND_I] = F_OP_BITAND<int32_t, int32_t, int32_t>,
	[OP_BITAND_IF] = F_OP_BITAND<int32_t, vec_t, int32_t>,
	[OP_BITAND_FI] = F_OP_BITAND<vec_t, int32_t, int32_t>,
		
	[OP_BITOR_F] = F_OP_BITOR<vec_t, vec_t, vec_t>,
	[OP_BITOR_I] = F_OP_BITOR<int32_t, int32_t, int32_t>,
	[OP_BITOR_IF] = F_OP_BITOR<int32_t, vec_t, int32_t>,
	[OP_BITOR_FI] = F_OP_BITOR<vec_t, int32_t, int32_t>,
		
	[OP_CONV_ITOF] = F_OP_ITOF,
	[OP_CONV_FTOI] = F_OP_FTOI,
	[OP_CP_ITOF] = F_OP_P_ITOF,
	[OP_CP_FTOI] = F_OP_P_FTOI,
		
	[OP_BITXOR_I] = F_OP_BITXOR,
	[OP_RSHIFT_I] = F_OP_RSHIFT,
	[OP_LSHIFT_I] = F_OP_LSHIFT,

	[OP_GLOBALADDRESS] = F_OP_GLOBALADDRESS,
	[OP_ADD_PIW] = F_OP_ADD_PIW,
		
	[OP_LOADA_F] = F_OP_LOADA<vec_t>,
	[OP_LOADA_V] = F_OP_LOADA<vec3_t>,
	[OP_LOADA_S] = F_OP_LOADA<string_t>,
	[OP_LOADA_ENT] = F_OP_LOADA<ent_t>,
	[OP_LOADA_FLD] = F_OP_LOADA<int32_t>,
	[OP_LOADA_FNC] = F_OP_LOADA<func_t>,
	[OP_LOADA_I] = F_OP_LOADA<int32_t>,
		
	[OP_LOADP_F] = F_OP_LOADP<vec_t>,
	[OP_LOADP_V] = F_OP_LOADP<vec3_t>,
	[OP_LOADP_S] = F_OP_LOADP<string_t>,
	[OP_LOADP_ENT] = F_OP_LOADP<ent_t>,
	[OP_LOADP_FLD] = F_OP_LOADP<int32_t>,
	[OP_LOADP_FNC] = F_OP_LOADP<func_t>,
	[OP_LOADP_I] = F_OP_LOADP<int32_t>,

	[OP_BOUNDCHECK] = F_OP_BOUNDCHECK
};

void QCVM::Execute(QCFunction &function)
{
	if (function.id < 0)
	{
		CallBuiltin(function);
		return;
	}

	int enter_depth = 1;

	Enter(function);

	while (1)
	{
		// get next statement
		auto &current = *state.current;
		const QCStatement &statement = *(++current.statement);
		__attribute__((unused)) auto timer = CreateOpcodeTimer(statement.opcode);

#ifdef ALLOW_PROFILING
		(*state.current).profile->fields[NumInstructions]++;
#endif

		const std::array<operand, 3> operands = {
			statement.args[0],
			statement.args[1],
			statement.args[2]
		};

		if (statement.opcode > std::extent_v<decltype(codeFuncs)>)
			Error("unsupported opcode %i", statement.opcode);

		auto func = codeFuncs[statement.opcode];

		if (!func)
			Error("unsupported opcode %i", statement.opcode);

		func(*this, operands, enter_depth);

		if (!enter_depth)
			return;		// all done
	}
}

QCVM qvm;
static const cvar_t *game_var;

void InitVM()
{
	gi.dprintf ("==== %s ====\n", __func__);

	game_var = gi.cvar("game", "", CVAR_NONE);

	if (!game_var->string)
		qvm.Error("bad game");

	std::filesystem::path progs_path(vas("%s/progs.dat", game_var->string).data());

	if (!std::filesystem::exists(progs_path))
		qvm.Error("no progs.dat");
	
	std::ifstream stream(progs_path, std::ios::binary);

	QCHeader header;

	stream.read(reinterpret_cast<char *>(&header), sizeof(header));

	qvm.string_data = static_cast<char *>(gi.TagMalloc(header.sections.string.size, TAG_GAME));
	qvm.string_size = header.sections.string.size;

	stream.seekg(header.sections.string.offset);
	stream.read(qvm.string_data, header.sections.string.size);
	
	// create immutable string map, for fast hash action
	for (size_t i = 0; i < qvm.string_size; i++)
	{
		const char *s = qvm.string_data + i;

		if (!*s)
			continue;

		auto view = std::string_view(s);
		i += view.length();

		if (qvm.string_hashes.contains(view))
			continue;

		qvm.string_hashes.emplace(std::move(view));
	}

	qvm.statements.resize(header.sections.statement.size);

	stream.seekg(header.sections.statement.offset);
	stream.read(reinterpret_cast<char *>(qvm.statements.data()), header.sections.statement.size * sizeof(QCStatement));

#if _DEBUG
	for (auto &s : qvm.statements)
		if (!codeFuncs[s.opcode])
			qvm.Error("opcode not implemented: %i\n", s.opcode);
#endif
	
	qvm.definitions.resize(header.sections.definition.size);

	stream.seekg(header.sections.definition.offset);
	stream.read(reinterpret_cast<char *>(qvm.definitions.data()), header.sections.definition.size * sizeof(QCDefinition));
	
	for (auto &definition : qvm.definitions)
		if (definition.name_index != string_t::STRING_EMPTY)
			qvm.definition_map.emplace(definition.name_index, &definition);

	qvm.fields.resize(header.sections.field.size);

	stream.seekg(header.sections.field.offset);
	stream.read(reinterpret_cast<char *>(qvm.fields.data()), header.sections.field.size * sizeof(QCDefinition));

	for (auto &field : qvm.fields)
		qvm.field_map.emplace(field.global_index, &field);

	qvm.functions.resize(header.sections.function.size);
#ifdef ALLOW_PROFILING
	qvm.profile_data.resize(header.sections.function.size);
#endif

	stream.seekg(header.sections.function.offset);
	stream.read(reinterpret_cast<char *>(qvm.functions.data()), header.sections.function.size * sizeof(QCFunction));

	qvm.global_data = reinterpret_cast<global_t *>(gi.TagMalloc(header.sections.globals.size * sizeof(global_t), TAG_GAME));
	qvm.global_size = header.sections.globals.size;

	stream.seekg(header.sections.globals.offset);
	stream.read(reinterpret_cast<char *>(qvm.global_data), qvm.global_size * sizeof(global_t));

	int32_t lowest_func = 0;

	for (auto &func : qvm.functions)
		if (func.id < 0)
			lowest_func = min(func.id, lowest_func);

	qvm.builtins.SetFirstID(lowest_func - 1);

	// Check for debugging info
	std::filesystem::path lno_path(vas("%s/progs.lno", game_var->string).data());

	if (std::filesystem::exists(lno_path))
	{
		constexpr int lnotype = 1179602508;
		constexpr int version = 1;

		int magic, ver, numglobaldefs, numglobals, numfielddefs, numstatements;

		std::ifstream lno_stream(lno_path, std::ios::binary);
		
		lno_stream.read(reinterpret_cast<char *>(&magic), sizeof(magic));
		lno_stream.read(reinterpret_cast<char *>(&ver), sizeof(ver));
		lno_stream.read(reinterpret_cast<char *>(&numglobaldefs), sizeof(numglobaldefs));
		lno_stream.read(reinterpret_cast<char *>(&numglobals), sizeof(numglobals));
		lno_stream.read(reinterpret_cast<char *>(&numfielddefs), sizeof(numfielddefs));
		lno_stream.read(reinterpret_cast<char *>(&numstatements), sizeof(numstatements));

		if (magic == lnotype && ver == version && numglobaldefs == header.sections.definition.size &&
			numglobals == header.sections.globals.size && numfielddefs == header.sections.field.size &&
			numstatements == header.sections.statement.size)
		{
			qvm.linenumbers.resize(header.sections.statement.size);
			lno_stream.read(reinterpret_cast<char *>(qvm.linenumbers.data()), sizeof(int) * header.sections.statement.size);
		}
		else
			gi.dprintf("Unsupported/outdated progs.lno file\n");
	}
}

void CheckVM()
{
	for (auto &func : qvm.functions)
		if (func.id == 0 && func.name_index != string_t::STRING_EMPTY)
			gi.dprintf("Missing builtin function: %s\n", qvm.GetString(func.name_index));
}

void ShutdownVM()
{
#ifdef ALLOW_PROFILING
	{
		std::filesystem::path progs_path(vas("%s/profile.csv", game_var->string).data());
		std::filebuf fb;
		fb.open(progs_path, std::ios::out);
		std::ostream stream(&fb);

		stream << "ID,Name,Total (ms),Self(ms),Funcs(ms)";
	
		for (auto pf : profile_type_names)
			stream << "," << pf;
	
		stream << "\n";

		for (size_t i = 0; i < qvm.profile_data.size(); i++)
		{
			auto &profile = qvm.profile_data[i];
			auto &ff = qvm.functions[i];
			const char *name = qvm.GetString(ff.name_index);
		
			auto total = std::chrono::duration<double, std::milli>(profile.total).count();
			double self = total;
			double func_call_time = std::chrono::duration<double, std::milli>(profile.call_into).count();
		
			if (func_call_time)
				self -= func_call_time;

			stream << i << "," << name << "," << total << "," << self << "," << func_call_time;
		
			for (profile_type_t f = static_cast<profile_type_t>(0); f < TotalProfileFields; f = static_cast<profile_type_t>(static_cast<size_t>(f) + 1))
				stream << "," << profile.fields[f];

			stream << "\n";
		}
	}

	{
		std::filesystem::path progs_path(vas("%s/timers.csv", game_var->string).data());
		std::filebuf fb;
		fb.open(progs_path, std::ios::out);
		std::ostream stream(&fb);

		stream << "Name,Count,Total (ms)\n";

		for (size_t i = 0; i < std::extent_v<decltype(qvm.timers)>; i++)
		{
			auto &timer = qvm.timers[i];

			auto total = std::chrono::duration<double, std::milli>(timer.time).count();

			stream << timer_type_names[i] << "," << timer.count << "," << total << "\n";
		}
	}

	{
		std::filesystem::path progs_path(vas("%s/opcodes.csv", game_var->string).data());
		std::filebuf fb;
		fb.open(progs_path, std::ios::out);
		std::ostream stream(&fb);

		stream << "ID,Count,Total (ms)\n";

		for (size_t i = 0; i < std::extent_v<decltype(qvm.opcode_timers)>; i++)
		{
			auto &timer = qvm.opcode_timers[i];

			auto total = std::chrono::duration<double, std::milli>(timer.time).count();

			stream << i << "," << timer.count << "," << total << "\n";
		}
	}
#endif
}