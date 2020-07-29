#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"
#include "vm_game.h"

struct QCOffset
{
	uint32_t	offset;
	uint32_t	size;
};

enum progs_version_t : uint32_t
{
	PROGS_Q1	= 6,
	PROGS_FTE	= 7,

	PROG_SECONDARYVERSION16 = ((('1'<<0)|('F'<<8)|('T'<<16)|('E'<<24))^(('P'<<0)|('R'<<8)|('O'<<16)|('G'<<24))),
	PROG_SECONDARYVERSION32 = ((('1'<<0)|('F'<<8)|('T'<<16)|('E'<<24))^(('3'<<0)|('2'<<8)|('B'<<16)|(' '<<24)))
};

struct QCHeader
{
	progs_version_t	version;
	uint16_t		crc;
	uint16_t		skip;

	struct {
		QCOffset	statement;
		QCOffset	definition;
		QCOffset	field;
		QCOffset	function;
		QCOffset	string;
		QCOffset	globals;
	} sections;

	uint32_t		entityfields;

	uint32_t		ofs_files;	//non list format. no comp
	uint32_t		ofs_linenums;	//numstatements big	//comp 64
	QCOffset		bodylessfuncs;
	QCOffset		types;

	uint32_t		blockscompressed;

	progs_version_t	secondary_version;
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

	return (string_t)(-(int32_t)(strings.size() + 1));
}

string_t QCVMStringList::StoreRefCounted(std::string &&str)
{
	string_t id = Allocate();

	strings.emplace(id, (QCVMRefCountString) {
		std::move(str),
		0
	});
	return id;
}

void QCVMStringList::Unstore(const string_t &id, const bool &free_index)
{
	assert(strings.contains(id));

	assert(!strings.at(id).ref_count);
		
	strings.erase(id);

	if (free_index)
		free_indices.push(id);
}

size_t QCVMStringList::Length(const string_t &id) const
{
	assert(strings.contains(id));
		
	const auto &str = strings.at(id);
		
	return str.str.length();
}

std::string &QCVMStringList::GetRefCounted(const string_t &id)
{
	assert(strings.contains(id));

	return strings.at(id).str;
}

const char *QCVMStringList::Get(const string_t &id) const
{
	assert(strings.contains(id));

	return strings.at(id).str.c_str();
}

void QCVMStringList::AcquireRefCounted(const string_t &id)
{
	__attribute__((unused)) auto timer = vm.CreateTimer(StringAcquire);

	assert(strings.contains(id));

	strings.at(id).ref_count++;
}

void QCVMStringList::ReleaseRefCounted(const string_t &id)
{
	__attribute__((unused)) auto timer = vm.CreateTimer(StringRelease);

	assert(strings.contains(id));

	QCVMRefCountString *str = &strings.at(id);

	assert(str->ref_count);
		
	str->ref_count--;

	if (!str->ref_count)
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
}

void QCVMStringList::CheckRefUnset(const void *ptr, const size_t &span, const bool &assume_changed)
{
	__attribute__((unused)) auto timer = vm.CreateTimer(StringCheckUnset);

	for (size_t i = 0; i < span; i++)
	{
		const global_t *gptr = (const global_t *)ptr + i;

		if (!ref_storage.contains(gptr))
			continue;

		const string_t old = ref_storage.at(gptr);

		if (!assume_changed)
		{
			auto newstr = *(const string_t *)gptr;

			// still here, so we probably just copied to ourselves or something
			if (newstr == old)
				continue;
		}

		// not here! release and unmark
		ReleaseRefCounted(old);

		ref_storage.erase(gptr);
	}
}

bool QCVMStringList::HasRef(const void *ptr)
{
	__attribute__((unused)) auto timer = vm.CreateTimer(StringHasRef);
	return ref_storage.contains(ptr);
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

bool QCVMStringList::IsRefCounted(const string_t &id)
{
	return strings.contains(id);
}

void QCVMStringList::MarkIfHasRef(const void *src_ptr, const void *dst_ptr, const size_t &span)
{
	__attribute__((unused)) auto timer = vm.CreateTimer(StringMarkIfHasRef);

	for (size_t i = 0; i < span; i++)
	{
		const global_t *src_gptr = (const global_t *)src_ptr + i;
		const global_t *dst_gptr = (const global_t *)dst_ptr + i;

		if (ref_storage.contains(src_gptr))
			MarkRefCopy(ref_storage.at(src_gptr), dst_gptr);
	}
}

QCVMRefCountBackup QCVMStringList::PopRef(const void *ptr)
{
	const string_t id = ref_storage.at(ptr);

	QCVMRefCountBackup popped_ref { ptr, id };

	ref_storage.erase(ptr);

	return popped_ref;
}

void QCVMStringList::PushRef(const QCVMRefCountBackup &backup)
{
	// somebody stole our ptr >:(
	if (ref_storage.contains(backup.ptr))
	{
		ReleaseRefCounted(ref_storage.at(backup.ptr));
		ref_storage.erase(backup.ptr);
	}

	// simple restore
	if (strings.contains(backup.id))
	{
		ref_storage.emplace(backup.ptr, backup.id);
		return;
	}

	vm.Error("what");
}

void QCVMStringList::WriteState(FILE *fp)
{
	size_t len;

	for (const auto &s : strings)
	{
		len = s.second.str.length();
		fwrite(&len, sizeof(len), 1, fp);
		fwrite(s.second.str.data(), sizeof(char), len, fp);
	}

	len = 0;
	fwrite(&len, sizeof(len), 1, fp);
}

void QCVMStringList::ReadState(FILE *fp)
{
	std::string s;

	while (true)
	{
		size_t len;

		fread(&len, sizeof(len), 1, fp);

		if (!len)
			break;

		s.resize(len);

		fread(s.data(), sizeof(char), len, fp);

		// does not acquire, since entity/game state does that itself
		vm.StoreOrFind(std::move(s));
	}
}

QCVMBuiltinList::QCVMBuiltinList(QCVM &invm) :
	vm(invm)
{
}

#include <sstream>

std::string ParseFormat(const string_t &formatid, QCVM &vm, const uint8_t &start)
{
	enum ParseToken
	{
		PT_NONE,
		PT_SPECIFIER,
		PT_SKIP
	};

	std::stringstream buf;
	size_t i = 0;
	const size_t len = vm.StringLength(formatid);
	static char format_buffer[17];
	uint8_t param_index = start;
	const char *format = vm.GetString(formatid);

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
		ParseToken state = ParseToken::PT_NONE;

		while (state < ParseToken::PT_SPECIFIER)
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
				state = ParseToken::PT_SPECIFIER;
				continue;
			case '%':
				buf.put('%');
				state = ParseToken::PT_SKIP;
				continue;
			}
		}

		if (state == ParseToken::PT_SPECIFIER)
		{
			Q_strlcpy(format_buffer, specifier_start, min(sizeof(format_buffer), (size_t)((next - specifier_start) + 1 + 1)));

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
	func_t id = (func_t)this->next_id;
	this->next_id--;
	this->builtins.emplace(id, builtin);

	for (auto &func : vm.functions)
	{
		if (func.id || func.name_index == STRING_EMPTY)
			continue;

		if (strcmp(vm.GetString(func.name_index), name) == 0)
		{
			func.id = (int32_t)id;
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
		if (f.name_index == STRING_EMPTY)
			continue;
		else if (strcmp(vm.GetString(f.name_index), field_name))
			continue;

		wraps.emplace((f.global_index + field_offset) * sizeof(global_t), (QCVMFieldWrap) {
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

	if (!*func)
		func = "dunno";

	const char *file = GetString(stack.function->file_index);

	if (!*file)
		file = "dunno.qc";

	return vas("%s (%s:%i @ %u)", file, func, LineNumberFor(stack.statement), stack.statement - statements.data());
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

#ifdef ALLOW_DEBUGGING
void QCVM::SetBreakpoint(const int &mode, const std::string &file, const int &line)
{
	string_t id;

	if (!FindString(file, id))
	{
		gi.dprintf("Can't toggle breakpoint: can't find file %s in table\n", file.data());
		return;
	}

	for (auto &function : functions)
	{
		if (function.id <= 0 || function.file_index != id)
			continue;

		for (auto statement = &statements[function.id]; statement->opcode != OP_DONE; statement++)
		{
			if (LineNumberFor(statement) == line)
			{
				// got it
				if (mode)
					statement->opcode |= OP_BREAKPOINT;
				else
					statement->opcode &= ~OP_BREAKPOINT;
				
				gi.dprintf("Breakpoint set @ %s:%i\n", file.data(), line);
				return;
			}
		}
	}

	gi.dprintf("Can't toggle breakpoint: can't find %s:%i\n", file.data(), line);
}
#endif

using operands = std::array<global_t, 3>;
using OPCodeFunc = void(*)(QCVM &vm, const operands &operands, int &depth);

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_MUL(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a * b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_DIV(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a / b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_ADD(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a + b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_SUB(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a - b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_EQ(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a == b);
}

inline void F_OP_EQ_S(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<string_t>(operands[0]);
	const auto &b = vm.GetGlobal<string_t>(operands[1]);

	if (a == b)
		vm.SetGlobal<vec_t>(operands[2], 1);
	else
		vm.SetGlobal<vec_t>(operands[2], !strcmp(vm.GetString(a), vm.GetString(b)));
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_NE(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a != b);
}

inline void F_OP_NE_S(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<string_t>(operands[0]);
	const auto &b = vm.GetGlobal<string_t>(operands[1]);

	if (a == b)
		vm.SetGlobal<vec_t>(operands[2], 0);
	else
		vm.SetGlobal<vec_t>(operands[2], !!strcmp(vm.GetString(a), vm.GetString(b)));
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_LE(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a <= b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_GE(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a >= b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_LT(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a < b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_GT(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a > b);
}

template<typename TType>
inline void F_OP_LOAD(QCVM &vm, const operands &operands, int &depth)
{
	edict_t *ent = vm.EntToEntity(vm.GetGlobal<ent_t>(operands[0]), true);
	int32_t field_offset = vm.GetGlobal<int32_t>(operands[1]);
	auto &field_value = *(TType *)((int32_t*)ent + field_offset);
	vm.SetGlobal(operands[2], field_value);

	vm.dynamic_strings.MarkIfHasRef<sizeof(TType) / sizeof(global_t)>(&field_value, vm.GetGlobalByIndex(operands[2]));
}

inline void F_OP_ADDRESS(QCVM &vm, const operands &operands, int &depth)
{
	edict_t *ent = vm.EntToEntity(vm.GetGlobal<ent_t>(operands[0]), true);
	int32_t field = vm.GetGlobal<int32_t>(operands[1]);
	vm.SetGlobal(operands[2], vm.EntityFieldAddress(ent, field));
}

template<typename TType>
inline void F_OP_STORE(QCVM &vm, const operands &operands, int &depth)
{
	vm.CopyGlobal<TType>(operands[1], operands[0]);
}

template<typename TType, typename TResult>
inline void F_OP_STORE(QCVM &vm, const operands &operands, int &depth)
{
	vm.CopyGlobal<TResult, TType>(operands[1], operands[0]);
}

template<typename TType, typename TResult = TType>
inline void F_OP_STOREP(QCVM &vm, const operands &operands, int &depth)
{
	size_t address = vm.GetGlobal<int32_t>(operands[1]) + (vm.GetGlobal<ptrdiff_t>(operands[2]) * sizeof(global_t));
	const auto &value = vm.GetGlobal<TType>(operands[0]);

	if (!vm.PointerValid(address, false, sizeof(TResult)))
		vm.Error("invalid address");

	constexpr size_t span = sizeof(TType) / sizeof(global_t);

	auto address_ptr = (TResult *)address;
	*address_ptr = value;
	vm.dynamic_strings.CheckRefUnset(address_ptr, span);

	edict_t *ent = vm.AddressToEntity(address);
	ptrdiff_t field = vm.AddressToField(ent, address);

	for (size_t i = 0; i < span; i++)
		vm.field_wraps.WrapField(ent, field + (i * sizeof(global_t)), &value + i);

	vm.dynamic_strings.MarkIfHasRef<span>(&value, address_ptr);
}

template<typename TType, typename TResult>
inline void F_NOT(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TType>(operands[0]);

	vm.SetGlobal<TResult>(operands[2], !a);
}

inline void F_NOT_S(QCVM &vm, const operands &operands, int &depth)
{
	const string_t a = vm.GetGlobal<string_t>(operands[0]);

	vm.SetGlobal<vec_t>(operands[2], a == STRING_EMPTY || !*vm.GetString(a));
}

template<typename TType>
inline void F_IF(QCVM &vm, const operands &operands, int &depth)
{
	if (vm.GetGlobal<TType>(operands[0]))
	{
		QCStack *current = vm.state.current;
		current->statement += (int16_t)current->statement->args[1] - 1;
	}
}

inline void F_IF_S(QCVM &vm, const operands &operands, int &depth)
{
	const string_t s = vm.GetGlobal<string_t>(operands[0]);

	if (s != STRING_EMPTY && *vm.GetString(s))
	{
		QCStack *current = vm.state.current;
		current->statement += (int16_t)current->statement->args[1] - 1;
	}
}

template<typename TType>
inline void F_IFNOT(QCVM &vm, const operands &operands, int &depth)
{
	if (!vm.GetGlobal<TType>(operands[0]))
	{
		QCStack *current = vm.state.current;
		current->statement += (int16_t)current->statement->args[1] - 1;
	}
}

inline void F_IFNOT_S(QCVM &vm, const operands &operands, int &depth)
{
	const string_t s = vm.GetGlobal<string_t>(operands[0]);

	if (s == STRING_EMPTY || !*vm.GetString(s))
	{
		QCStack *current = vm.state.current;
		current->statement += (int16_t)current->statement->args[1] - 1;
	}
}

template<size_t argc, bool hexen>
inline void F_CALL(QCVM &vm, const operands &operands, int &depth)
{
	if constexpr (argc >= 2 && hexen)
		vm.CopyGlobal<std::array<global_t, 3>>(GLOBAL_PARM1, operands[2]);

	if constexpr (argc >= 1 && hexen)
		vm.CopyGlobal<std::array<global_t, 3>>(GLOBAL_PARM0, operands[1]);

	const int32_t &enter_func = vm.GetGlobal<int32_t>(operands[0]);

	vm.state.argc = argc;
	if (enter_func <= 0 || enter_func >= vm.functions.size())
		vm.Error("NULL function");

#ifdef ALLOW_PROFILING
	QCStack *current = vm.state.current;
	current->profile->fields[NumFuncCalls]++;
#endif

	QCFunction *call = &vm.functions[enter_func];

	if (!call->id)
		vm.Error("Tried to call missing function %s", vm.GetString(call->name_index));

	if (call->id < 0)
	{
		// negative statements are built in functions
		vm.CallBuiltin(call);
		return;
	}

	depth++;
	vm.Enter(call);
}

inline void F_GOTO(QCVM &vm, const operands &operands, int &depth)
{
	QCStack *current = vm.state.current;
	current->statement += (int16_t)current->statement->args[0] - 1;
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_AND(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a && b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_OR(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a || b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_BITAND(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = (int32_t)vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = (int32_t)vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a & b);
}

template<typename TLeft, typename TRight, typename TResult>
inline void F_OP_BITOR(QCVM &vm, const operands &operands, int &depth)
{
	const auto &a = (int32_t)vm.GetGlobal<TLeft>(operands[0]);
	const auto &b = (int32_t)vm.GetGlobal<TRight>(operands[1]);

	vm.SetGlobal<TResult>(operands[2], a | b);
}

inline void F_OP_ITOF(QCVM &vm, const operands &operands, int &depth)
{
	vm.SetGlobal<vec_t>(operands[2], vm.GetGlobal<int32_t>(operands[0]));
}

inline void F_OP_FTOI(QCVM &vm, const operands &operands, int &depth)
{
	vm.SetGlobal<int32_t>(operands[2], vm.GetGlobal<vec_t>(operands[0]));
}

inline void F_OP_P_ITOF(QCVM &vm, const operands &operands, int &depth)
{
	const size_t address = vm.GetGlobal<int32_t>(operands[0]);

	if (!vm.PointerValid(address))
		vm.Error("invalid address");

	vm.SetGlobal<vec_t>(operands[2], *(int32_t *)address);
}

inline void F_OP_P_FTOI(QCVM &vm, const operands &operands, int &depth)
{
	const size_t address = vm.GetGlobal<int32_t>(operands[0]);

	if (!vm.PointerValid(address))
		vm.Error("invalid address");

	vm.SetGlobal<int32_t>(operands[2], *(vec_t *)address);
}

inline void F_OP_BITXOR(QCVM &vm, const operands &operands, int &depth)
{
	const int32_t a = vm.GetGlobal<int32_t>(operands[0]);
	const int32_t b = vm.GetGlobal<int32_t>(operands[1]);

	vm.SetGlobal<int32_t>(operands[2], a ^ b);
}

inline void F_OP_RSHIFT(QCVM &vm, const operands &operands, int &depth)
{
	const int32_t a = vm.GetGlobal<int32_t>(operands[0]);
	const int32_t b = vm.GetGlobal<int32_t>(operands[1]);

	vm.SetGlobal<int32_t>(operands[2], a >> b);
}

inline void F_OP_LSHIFT(QCVM &vm, const operands &operands, int &depth)
{
	const int32_t a = vm.GetGlobal<int32_t>(operands[0]);
	const int32_t b = vm.GetGlobal<int32_t>(operands[1]);

	vm.SetGlobal<int32_t>(operands[2], a << b);
}

inline void F_OP_GLOBALADDRESS(QCVM &vm, const operands &operands, int &depth)
{
	const global_t *base = &vm.GetGlobal<global_t>(operands[0]);
	const ptrdiff_t offset = vm.GetGlobal<int32_t>(operands[1]);
	const size_t address = (size_t)(base + offset);

	if (!vm.PointerValid(address))
		vm.Error("bad pointer");

	vm.SetGlobal(operands[2], (int32_t)address);
}

inline void F_OP_ADD_PIW(QCVM &vm, const operands &operands, int &depth)
{
	const int32_t a = vm.GetGlobal<int32_t>(operands[0]);
	const int32_t b = vm.GetGlobal<int32_t>(operands[1]);

	vm.SetGlobal(operands[2], (int32_t)(a + (b * sizeof(float))));
}

template<typename TType>
inline void F_OP_LOADA(QCVM &vm, const operands &operands, int &depth)
{
	const ptrdiff_t address = (ptrdiff_t)operands[0] + vm.GetGlobal<int32_t>(operands[1]);
			
	if (!vm.PointerValid((ptrdiff_t)(vm.global_data + address), false, sizeof(TType)))
		vm.Error("Invalid pointer %x", address);
			
	auto &field_value = *(TType *)(vm.global_data + address);
	vm.SetGlobal(operands[2], field_value);
	
	constexpr size_t span = sizeof(TType) / sizeof(global_t);
	vm.dynamic_strings.MarkIfHasRef<span>(&field_value, vm.GetGlobalByIndex(operands[2]));
}

template<typename TType>
inline void F_OP_LOADP(QCVM &vm, const operands &operands, int &depth)
{
	const ptrdiff_t address = vm.GetGlobal<int32_t>(operands[0]) + (vm.GetGlobal<int32_t>(operands[1]) * sizeof(global_t));

	if (!vm.PointerValid(address, false, sizeof(TType)))
		vm.Error("Invalid pointer %x", address);
			
	auto &field_value = *(TType *)(address);
	vm.SetGlobal(operands[2], field_value);
	
	constexpr size_t span = sizeof(TType) / sizeof(global_t);
	vm.dynamic_strings.MarkIfHasRef<span>(&field_value, vm.GetGlobalByIndex(operands[2]));
}

inline void F_OP_LOADP_C(QCVM &vm, const operands &operands, int &depth)
{
	const string_t strid = vm.GetGlobal<string_t>(operands[0]);
	const size_t offset = vm.GetGlobal<int32_t>(operands[1]);

	if (offset > vm.StringLength(strid))
		vm.SetGlobal<int32_t>(operands[2], 0);
	else
	{
		const char *str = vm.GetString(strid);
		vm.SetGlobal<int32_t>(operands[2], str[offset]);
	}
}

inline void F_OP_BOUNDCHECK(QCVM &vm, const operands &operands, int &depth)
{
#if _DEBUG
	const uint32_t a = vm.GetGlobal<uint32_t>(operands[0]);
	const uint32_t b = (uint32_t)operands[1];
	const uint32_t c = (uint32_t)operands[2];

	if (a < c || a >= b)
		vm.Error("bounds check failed");
#endif
}

template<typename TType, typename TMul>
inline void F_OP_MULSTOREP(QCVM &vm, const operands &operands, int &depth)
{
	const size_t address = vm.GetGlobal<int32_t>(operands[1]);

	if (!vm.PointerValid(address))
		vm.Error("bad pointer");

	TType *f = (TType *)address;

	const auto &a = vm.GetGlobal<TMul>(operands[0]);

	vm.SetGlobal<TType>(operands[2], (*f) *= a);
}

template<typename TType, typename TMul>
inline void F_OP_DIVSTOREP(QCVM &vm, const operands &operands, int &depth)
{
	const size_t address = vm.GetGlobal<int32_t>(operands[1]);

	if (!vm.PointerValid(address))
		vm.Error("bad pointer");

	TType *f = (TType *)address;

	const auto &a = vm.GetGlobal<TMul>(operands[0]);

	vm.SetGlobal<TType>(operands[2], (*f) /= a);
}

template<typename TType, typename TMul>
inline void F_OP_SUBSTOREP(QCVM &vm, const operands &operands, int &depth)
{
	const size_t address = vm.GetGlobal<int32_t>(operands[1]);

	if (!vm.PointerValid(address))
		vm.Error("bad pointer");

	TType *f = (TType *)address;

	const auto &a = vm.GetGlobal<TMul>(operands[0]);

	vm.SetGlobal<TType>(operands[2], (*f) -= a);
}

template<typename TType, typename TMul>
inline void F_OP_ADDSTOREP(QCVM &vm, const operands &operands, int &depth)
{
	const size_t address = vm.GetGlobal<int32_t>(operands[1]);

	if (!vm.PointerValid(address))
		vm.Error("bad pointer");

	TType *f = (TType *)address;

	const auto &a = vm.GetGlobal<TMul>(operands[0]);

	vm.SetGlobal<TType>(operands[2], (*f) += a);
}

inline void F_OP_RAND0(QCVM &vm, const operands &operands, int &depth)
{
	vm.SetGlobal<vec_t>(operands[2], frand());
}

inline void F_OP_RAND1(QCVM &vm, const operands &operands, int &depth)
{
	vm.SetGlobal<vec_t>(operands[2], frand(vm.GetGlobal<vec_t>(operands[0])));
}

inline void F_OP_RAND2(QCVM &vm, const operands &operands, int &depth)
{
	vm.SetGlobal<vec_t>(operands[2], frand(vm.GetGlobal<vec_t>(operands[0]), vm.GetGlobal<vec_t>(operands[1])));
}

inline void F_OP_RANDV0(QCVM &vm, const operands &operands, int &depth)
{
	vm.SetGlobal<vec3_t>(operands[2], { frand(), frand(), frand() });
}

inline void F_OP_RANDV1(QCVM &vm, const operands &operands, int &depth)
{
	const vec3_t a = vm.GetGlobal<vec3_t>(operands[0]);
	vm.SetGlobal<vec3_t>(operands[2], { frand(a[0]), frand(a[1]), frand(a[2]) });
}

inline void F_OP_RANDV2(QCVM &vm, const operands &operands, int &depth)
{
	const vec3_t a = vm.GetGlobal<vec3_t>(operands[0]);
	const vec3_t b = vm.GetGlobal<vec3_t>(operands[1]);
	vm.SetGlobal<vec3_t>(operands[2], { frand(a[0], b[0]), frand(a[1], b[1]), frand(a[2], b[2]) });
}

template<typename TType>
inline void F_OP_STOREF(QCVM &vm, const operands &operands, int &depth)
{
	edict_t *ent = vm.EntToEntity(vm.GetGlobal<ent_t>(operands[0]), true);
	const ptrdiff_t field_offset = vm.GetGlobal<int32_t>(operands[1]);
	const size_t address = (size_t)((int32_t*)ent + field_offset);
	TType *field_value = (TType *)address;
	const TType *value = (TType *)vm.GetGlobalByIndex(operands[2]);

	*field_value = *value;

	vm.dynamic_strings.MarkIfHasRef<sizeof(TType) / sizeof(global_t)>(value, field_value);
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
	[OP_EQ_S] = F_OP_EQ_S,
	[OP_EQ_E] = F_OP_EQ<ent_t, ent_t, vec_t>,
	[OP_EQ_FNC] = F_OP_EQ<func_t, func_t, vec_t>,
	[OP_EQ_I] = F_OP_EQ<int32_t, int32_t, int32_t>,
	[OP_EQ_IF] = F_OP_EQ<int32_t, vec_t, int32_t>,
	[OP_EQ_FI] = F_OP_EQ<vec_t, int32_t, int32_t>,

	[OP_NE_F] = F_OP_NE<vec_t, vec_t, vec_t>,
	[OP_NE_V] = F_OP_NE<vec3_t, vec3_t, vec_t>,
	[OP_NE_S] = F_OP_NE_S,
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
		if (operands[0] != GLOBAL_NULL)
			vm.template CopyGlobal<std::array<global_t, 3>>(GLOBAL_RETURN, operands[0]);

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
	[OP_NOT_S] = F_NOT_S,
	[OP_NOT_FNC] = F_NOT</*func_t*/int32_t, vec_t>,
	[OP_NOT_ENT] = F_NOT</*ent_t*/int32_t, vec_t>,
	[OP_NOT_I] = F_NOT<int32_t, int32_t>,
		
	[OP_IF_I] = F_IF<int32_t>,
	[OP_IF_S] = F_IF_S,
	[OP_IF_F] = F_IF<vec_t>,
		
	[OP_IFNOT_I] = F_IFNOT<int32_t>,
	[OP_IFNOT_S] = F_IFNOT_S,
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
	[OP_LOADP_C] = F_OP_LOADP_C,

	[OP_BOUNDCHECK] = F_OP_BOUNDCHECK,
		
	[OP_RAND0] = F_OP_RAND0,
	[OP_RAND1] = F_OP_RAND1,
	[OP_RAND2] = F_OP_RAND2,

	[OP_RANDV0] = F_OP_RANDV0,
	[OP_RANDV1] = F_OP_RANDV1,
	[OP_RANDV2] = F_OP_RANDV2,
		
	[OP_STOREF_F] = F_OP_STOREF<vec_t>,
	[OP_STOREF_S] = F_OP_STOREF<string_t>,
	[OP_STOREF_I] = F_OP_STOREF<int32_t>,
	[OP_STOREF_V] = F_OP_STOREF<vec3_t>,
};

#ifdef ALLOW_DEBUGGING
void QCVM::BreakOnCurrentStatement()
{
	SendDebuggerCommand(vas("qcstep \"%s\":%i\n", GetString(state.current->function->file_index), LineNumberFor(state.current->statement)));
	debug.state = DEBUG_BROKE;
	debug.step_function = state.current->function;
	debug.step_statement = state.current->statement;
	debug.step_depth = state.stack.size();
	WaitForDebuggerCommands();
}
#endif

void QCVM::Execute(QCFunction *function)
{
	if (function->id < 0)
	{
		CallBuiltin(function);
		return;
	}

	int32_t enter_depth = 1;

	Enter(function);

	while (1)
	{
		// get next statement
		QCStack *current = state.current;
		const QCStatement *statement = ++current->statement;
		__attribute__((unused)) auto timer = CreateOpcodeTimer(statement->opcode & ~OP_BREAKPOINT);

#ifdef ALLOW_PROFILING
		state.current->profile->fields[NumInstructions]++;
#endif

#ifdef ALLOW_DEBUGGING
		if (statement->opcode & OP_BREAKPOINT)
			BreakOnCurrentStatement();
		else
		{
			// figure out if we need to break here.
			// step into is easiest: next QC execution that is not on the same function+line combo
			if (debug.state == DEBUG_STEP_INTO)
			{
				if (debug.step_function != current->function || LineNumberFor(debug.step_statement) != LineNumberFor(current->statement))
					BreakOnCurrentStatement();
			}
			// I lied, step out is the easiest
			else if (debug.state == DEBUG_STEP_OUT)
			{
				if (debug.step_depth > state.stack.size())
					BreakOnCurrentStatement();
			}
			// step over: either step out, or the next step that is in the same function + stack depth + not on same line
			else if (debug.state == DEBUG_STEP_OVER)
			{
				if (debug.step_depth > state.stack.size() || (debug.step_depth == state.stack.size() && debug.step_function == current->function && LineNumberFor(debug.step_statement) != LineNumberFor(current->statement)))
					BreakOnCurrentStatement();
			}
		}

		const opcode_t code = statement->opcode & ~OP_BREAKPOINT;
#else
		const opcode_t code = statement->opcode;
#endif

		if (code > std::extent_v<decltype(codeFuncs)>)
			Error("unsupported opcode %i", code);

		OPCodeFunc func = codeFuncs[code];

		if (!func)
			Error("unsupported opcode %i", code);

		func(*this, statement->args, enter_depth);

		if (!enter_depth)
			return;		// all done
	}
}

const uint32_t QCVM_VERSION	= 1;

void QCVM::WriteState(FILE *fp)
{
	fwrite(&QCVM_VERSION, sizeof(QCVM_VERSION), 1, fp);

	// write dynamic strings
	dynamic_strings.WriteState(fp);
}

void QCVM::ReadState(FILE *fp)
{
	uint32_t ver;

	fread(&ver, sizeof(ver), 1, fp);

	if (ver != QCVM_VERSION)
		Error("bad VM version");

	// read dynamic strings
	dynamic_strings.ReadState(fp);
}

QCVM qvm;
const cvar_t *game_var;

static void VMLoadStatements(FILE *fp, QCStatement *dst, QCHeader &header)
{
	// simple, rustic
	if (header.version == PROGS_FTE && header.secondary_version == PROG_SECONDARYVERSION32)
	{
		fread(dst, sizeof(QCStatement), header.sections.statement.size, fp);
		return;
	}

	struct QCStatement16
	{
		uint16_t				opcode;
		std::array<uint16_t, 3>	args;
	};

	QCStatement16 *statements = (QCStatement16 *)gi.TagMalloc(sizeof(QCStatement16) * header.sections.statement.size, TAG_GAME);
	fread(statements, sizeof(QCStatement16), header.sections.statement.size, fp);

	for (size_t i = 0; i < header.sections.statement.size; i++, dst++)
	{
		QCStatement16 *src = statements + i;

		dst->opcode = (opcode_t)src->opcode;
		dst->args = {
			(global_t)src->args[0],
			(global_t)src->args[1],
			(global_t)src->args[2]
		};
	}

	gi.TagFree(statements);
}

static void VMLoadDefinitions(FILE *fp, QCDefinition *dst, QCHeader &header, const size_t &size)
{
	// simple, rustic
	if (header.version == PROGS_FTE && header.secondary_version == PROG_SECONDARYVERSION32)
	{
		fread(dst, sizeof(QCDefinition), size, fp);
		return;
	}

	struct QCDefinition16
	{
		uint16_t	id;
		uint16_t	global_index;
		string_t	name_index;
	};

	QCDefinition16 *defs = (QCDefinition16 *)gi.TagMalloc(sizeof(QCDefinition16) * size, TAG_GAME);
	fread(defs, sizeof(QCDefinition16), size, fp);

	for (size_t i = 0; i < size; i++, dst++)
	{
		QCDefinition16 *src = defs + i;

		dst->id = (deftype_t)src->id;
		dst->global_index = (global_t)src->global_index;
		dst->name_index = src->name_index;
	}

	gi.TagFree(defs);
}

void InitVM()
{
	gi.dprintf ("==== %s ====\n", __func__);

	game_var = gi.cvar("game", "", CVAR_NONE);

	if (!game_var->string)
		qvm.Error("bad game");

	FILE *fp = fopen(vas("%s/progs.dat", game_var->string).data(), "rb");

	if (!fp)
		qvm.Error("no progs.dat");

	QCHeader header;

	fread(&header, sizeof(header), 1, fp);

	if (header.version != PROGS_Q1 && header.version != PROGS_FTE)
		qvm.Error("bad version (only version 6 & 7 progs are supported)");

	qvm.string_size = header.sections.string.size;
	qvm.string_data = (char *)gi.TagMalloc(sizeof(*qvm.string_data) * header.sections.string.size, TAG_GAME);
	qvm.string_lengths = (size_t *)gi.TagMalloc(sizeof(*qvm.string_lengths) * header.sections.string.size, TAG_GAME);

	fseek(fp, header.sections.string.offset, SEEK_SET);
	fread(qvm.string_data, sizeof(char), header.sections.string.size, fp);
	
	// create immutable string map, for fast hash action
	for (size_t i = 0; i < qvm.string_size; i++)
	{
		const char *s = qvm.string_data + i;

		if (!*s)
			continue;

		std::string_view view(s);

		for (size_t x = 0; x < view.length(); x++)
		{
			size_t len = view.length() - x;
			qvm.string_lengths[i + x] = len;
			qvm.string_hashes.emplace(std::string_view(s + x, len));
		}

		i += view.length();
	}

	qvm.statements.resize(header.sections.statement.size);

	fseek(fp, header.sections.statement.offset, SEEK_SET);
	VMLoadStatements(fp, qvm.statements.data(), header);

	for (auto &s : qvm.statements)
		if (!codeFuncs[s.opcode])
			qvm.Error("opcode not implemented: %i\n", s.opcode);
	
	qvm.definitions.resize(header.sections.definition.size);

	fseek(fp, header.sections.definition.offset, SEEK_SET);
	VMLoadDefinitions(fp, qvm.definitions.data(), header, header.sections.definition.size);
	
	for (auto &definition : qvm.definitions)
	{
		if (definition.name_index != STRING_EMPTY)
			qvm.definition_map_by_name.emplace(qvm.string_data + definition.name_index, &definition);

		qvm.definition_map_by_id.emplace(definition.global_index, &definition);
		qvm.string_hashes.emplace(qvm.string_data + definition.name_index);
	}

	qvm.fields.resize(header.sections.field.size);

	fseek(fp, header.sections.field.offset, SEEK_SET);
	VMLoadDefinitions(fp, qvm.fields.data(), header, header.sections.field.size);

	for (auto &field : qvm.fields)
	{
		qvm.field_map.emplace(field.global_index, &field);
		qvm.field_map_by_name.emplace(qvm.string_data + field.name_index, &field);

		qvm.string_hashes.emplace(qvm.string_data + field.name_index);
	}

	qvm.functions.resize(header.sections.function.size);
#ifdef ALLOW_PROFILING
	qvm.profile_data.resize(header.sections.function.size);
#endif

	fseek(fp, header.sections.function.offset, SEEK_SET);
	fread(qvm.functions.data(), sizeof(QCFunction), header.sections.function.size, fp);

	qvm.global_data = (global_t *)gi.TagMalloc(header.sections.globals.size * sizeof(global_t), TAG_GAME);
	qvm.global_size = header.sections.globals.size;

	fseek(fp, header.sections.globals.offset, SEEK_SET);
	fread(qvm.global_data, sizeof(global_t), qvm.global_size, fp);

	int32_t lowest_func = 0;

	for (auto &func : qvm.functions)
		if (func.id < 0)
			lowest_func = min(func.id, lowest_func);

	qvm.builtins.SetFirstID(lowest_func - 1);

	fclose(fp);

	// Check for debugging info
	fp = fopen(vas("%s/progs.lno", game_var->string).data(), "rb");

	if (fp)
	{
		constexpr int lnotype = 1179602508;
		constexpr int version = 1;

		struct {
			int magic, ver, numglobaldefs, numglobals, numfielddefs, numstatements;
		} lno_header;
		
		fread(&lno_header, sizeof(lno_header), 1, fp);

		if (lno_header.magic == lnotype && lno_header.ver == version && lno_header.numglobaldefs == header.sections.definition.size &&
			lno_header.numglobals == header.sections.globals.size && lno_header.numfielddefs == header.sections.field.size &&
			lno_header.numstatements == header.sections.statement.size)
		{
			qvm.linenumbers.resize(header.sections.statement.size);
			fread(qvm.linenumbers.data(), sizeof(int), header.sections.statement.size, fp);
			gi.dprintf("progs.lno line numbers loaded\n");
		}
		else
			gi.dprintf("Unsupported/outdated progs.lno file\n");

		fclose(fp);
	}
}

void CheckVM()
{
	for (auto &func : qvm.functions)
		if (func.id == 0 && func.name_index != STRING_EMPTY)
			gi.dprintf("Missing builtin function: %s\n", qvm.GetString(func.name_index));
}

void ShutdownVM()
{
#ifdef ALLOW_PROFILING
	{
		FILE *fp = fopen(vas("%s/profile.csv", game_var->string).data(), "wb");

		fprintf(fp, "ID,Name,Total (ms),Self(ms),Funcs(ms)");
	
		for (auto pf : profile_type_names)
			fprintf(fp, ",%s", pf);
	
		fprintf(fp, "\n");

		for (size_t i = 0; i < qvm.profile_data.size(); i++)
		{
			const QCProfile *profile = qvm.profile_data.data() + i;
			const QCFunction *ff = qvm.functions.data() + i;
			const char *name = qvm.GetString(ff->name_index);
		
			const double total = profile->total / 1000000.0;
			double self = total;
			const double func_call_time = profile->call_into / 1000000.0;
		
			if (func_call_time)
				self -= func_call_time;

			fprintf(fp, "%i,%s,%f,%f,%f", i, name, total, self, func_call_time);
		
			for (profile_type_t f = 0; f < TotalProfileFields; f++)
				fprintf(fp, ",%i", profile->fields[f]);

			fprintf(fp, "\n");
		}

		fclose(fp);
	}

	{
		FILE *fp = fopen(vas("%s/timers.csv", game_var->string).data(), "wb");

		fprintf(fp, "Name,Count,Total (ms)\n");

		for (size_t i = 0; i < std::extent_v<decltype(qvm.timers)>; i++)
		{
			const profile_timer_t *timer = qvm.timers + i;
			const double total = timer->time / 1000000.0;

			fprintf(fp, "%s,%i,%f\n", timer_type_names[i], timer->count, total);
		}

		fclose(fp);
	}

	{
		FILE *fp = fopen(vas("%s/opcodes.csv", game_var->string).data(), "wb");

		fprintf(fp, "ID,Count,Total (ms)\n");

		for (size_t i = 0; i < std::extent_v<decltype(qvm.opcode_timers)>; i++)
		{
			const profile_timer_t *timer = qvm.opcode_timers + i;
			const double total = timer->time / 1000000.0;

			fprintf(fp, "%i,%i,%f\n", i, timer->count, total);
		}

		fclose(fp);
	}
#endif
}