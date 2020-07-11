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
		return constant_storage[resolver];

	string_t id = Allocate();
	constant_storage[resolver] = id;
	strings[id] = resolver;
	return id;
}

string_t QCVMStringList::StoreStatic(const std::string_view &view)
{
	string_t id;

	if (constant_storage.contains(view.data()))
		id = constant_storage[view.data()];
	else
	{
		id = Allocate();
		constant_storage[view.data()] = id;
	}

	strings[id] = view;
	return id;
}

string_t QCVMStringList::StoreDynamic(const std::string &&str)
{
	string_t id = Allocate();
	strings[id] = str;
	return id;
}

string_t QCVMStringList::StoreRefCounted(const std::string &&str)
{
	string_t id = Allocate();
	strings[id] = (QCVMRefCountString) {
		str,
		1
	};

	return id;
}

void QCVMStringList::Unstore(const string_t &id)
{
	assert(strings.contains(id));
		
	const StringType &str = strings.at(id);
		
	if (std::holds_alternative<std::string_view>(str))
		constant_storage.erase(std::get<std::string_view>(str).data());
	else if (std::holds_alternative<const char **>(str))
		constant_storage.erase(std::get<const char **>(str));
		
	assert(!std::holds_alternative<QCVMRefCountString>(str) || !std::get<QCVMRefCountString>(str).ref_count);
		
	strings.erase(id);
	free_indices.push(id);
}

size_t QCVMStringList::Length(const string_t &id) const
{
	assert(strings.contains(id));
		
	const StringType &str = strings.at(id);
		
	if (std::holds_alternative<std::string>(str))
		return std::get<std::string>(str).length();
	else if (std::holds_alternative<std::string_view>(str))
		return std::get<std::string_view>(str).length();
	else if (std::holds_alternative<QCVMRefCountString>(str))
		return std::get<QCVMRefCountString>(str).str.length();
		
	return strlen(*std::get<const char **>(str));
}

std::string &QCVMStringList::GetDynamic(const string_t &id)
{
	assert(strings.contains(id));
		
	StringType &str = strings.at(id);

	assert(std::holds_alternative<std::string>(str) || std::holds_alternative<QCVMRefCountString>(str));
		
	if (std::holds_alternative<QCVMRefCountString>(str))
		return std::get<QCVMRefCountString>(str).str;

	return std::get<std::string>(str);
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
	else if (std::holds_alternative<QCVMRefCountString>(str))
		return std::get<QCVMRefCountString>(str).str.c_str();

	return std::get<std::string>(str).c_str();
}

void QCVMStringList::AcquireRefCounted(const string_t &id)
{
	assert(strings.contains(id));

	StringType &str = strings.at(id);

	assert(std::holds_alternative<QCVMRefCountString>(str));

	auto &ref = std::get<QCVMRefCountString>(str);

	ref.ref_count++;
}

void QCVMStringList::ReleaseRefCounted(const string_t &id)
{
	assert(strings.contains(id));

	StringType &str = strings.at(id);

	assert(std::holds_alternative<QCVMRefCountString>(str));

	auto &ref = std::get<QCVMRefCountString>(str);

	assert(ref.ref_count);
		
	ref.ref_count--;

	if (!ref.ref_count)
		Unstore(id);
}

QCVMBuiltinList::QCVMBuiltinList(QCVM &invm) :
	vm(invm)
{
}

void QCVMBuiltinList::SetFirstID(const int32_t &id)
{
	next_id = id;
}

bool QCVMBuiltinList::IsRegistered(const func_t &func)
{
	return builtins.contains(func);
}

QCBuiltin QCVMBuiltinList::Get(const func_t &func)
{
	return builtins.at(func);
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

			vm.Error("invalid format code detected: %x", *next);
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
	this->builtins[id] = builtin;

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
	builtins(*this),
	field_wraps(*this)
{
}

std::string QCVM::StackEntry(const QCStack &stack)
{
	if (!linenumbers.size())
		return "dunno:dunno";

	const char *func = GetString(stack.function->name_index);

	if (!strlen(func))
		func = "dunno";

	return vas("%s:%i(@%u)", func, linenumbers[stack.statement - statements.data()], stack.statement - statements.data());
}

std::string QCVM::StackTrace()
{
	std::string str = StackEntry(state.current);

	for (auto it = state.stack.rbegin(); it != state.stack.rend(); it++)
	{
		const auto &s = *it;

		if (!s.function)
			break;

		str += "\n" + StackEntry(s);
	}

	return str;
}

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
		const QCStatement &statement = *(++state.current.statement);

#ifdef STACK_TRACING
		stack_pos = StackTrace();
#endif

#ifdef ALLOW_PROFILING
		state.current.profile->fields[NumInstructions]++;
#endif

		struct operand
		{
			uint16_t	arg;

			inline operator global_t() const
			{
				return static_cast<global_t>(arg);
			}
		};

		const std::array<operand, 3> operands = {
			statement.args[0],
			statement.args[1],
			statement.args[2]
		};

		if (enable_tracing && (state.current.statement - statements.data()) == 122641)
			__debugbreak();

		switch (statement.opcode)
		{
		case OP_ADD_F: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal(operands[2], a + b);

			if (enable_tracing)
				PrintTrace("ADD_F %s + %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_ADD_V: {
			vec3_t out;
			const auto &a = GetGlobal<vec3_t>(operands[0]);
			const auto &b = GetGlobal<vec3_t>(operands[1]);

			for (size_t i = 0; i < out.size(); i++)
				out.at(i) = a.at(i) + b.at(i);

			SetGlobal(operands[2], out);
				
			if (enable_tracing)
				PrintTrace("ADD_V %s + %s = %s", TraceGlobal(operands[0], TYPE_VECTOR).data(), TraceGlobal(operands[1], TYPE_VECTOR).data(), TraceGlobal(operands[2], TYPE_VECTOR).data());
			break; }

		case OP_SUB_F: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);
				
			SetGlobal(operands[2], a - b);
				
			if (enable_tracing)
				PrintTrace("SUB_F %s - %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_SUB_V: {
			vec3_t out;
			const auto &a = GetGlobal<vec3_t>(operands[0]);
			const auto &b = GetGlobal<vec3_t>(operands[1]);

			for (size_t i = 0; i < out.size(); i++)
				out.at(i) = a.at(i) - b.at(i);

			SetGlobal(operands[2], out);
				
			if (enable_tracing)
				PrintTrace("SUB_V %s - %s = %s", TraceGlobal(operands[0], TYPE_VECTOR).data(), TraceGlobal(operands[1], TYPE_VECTOR).data(), TraceGlobal(operands[2], TYPE_VECTOR).data());
			break; }

		case OP_MUL_F: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal(operands[2], a * b);
				
			if (enable_tracing)
				PrintTrace("MUL_F %s * %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_MUL_V: {
			const auto &a = GetGlobal<vec3_t>(operands[0]);
			const auto &b = GetGlobal<vec3_t>(operands[1]);

			SetGlobal(operands[2], DotProduct(a, b));
				
			if (enable_tracing)
				PrintTrace("MUL_V %s * %s = %s", TraceGlobal(operands[0], TYPE_VECTOR).data(), TraceGlobal(operands[1], TYPE_VECTOR).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }

		case OP_MUL_FV: {
			vec3_t out;
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec3_t>(operands[1]);

			for (size_t i = 0; i < out.size(); i++)
				out.at(i) = b.at(i) * a;

			SetGlobal(operands[2], out);
				
			if (enable_tracing)
				PrintTrace("MUL_FV %s * %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_VECTOR).data(), TraceGlobal(operands[2], TYPE_VECTOR).data());
			break; }
		case OP_MUL_VF: {
			vec3_t out;
			const auto &a = GetGlobal<vec3_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			for (size_t i = 0; i < out.size(); i++)
				out.at(i) = a.at(i) * b;

			SetGlobal(operands[2], out);
				
			if (enable_tracing)
				PrintTrace("MUL_VF %s * %s = %s", TraceGlobal(operands[0], TYPE_VECTOR).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_VECTOR).data());
			break; }

		case OP_DIV_F: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal(operands[2], a / b);
				
			if (enable_tracing)
				PrintTrace("DIV_F %s / %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }

		case OP_BITAND: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], static_cast<int32_t>(a) & static_cast<int32_t>(b));
				
			if (enable_tracing)
				PrintTrace("BITAND %s & %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_BITOR: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], static_cast<int32_t>(a) | static_cast<int32_t>(b));
				
			if (enable_tracing)
				PrintTrace("BITOR %s | %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
				
		case OP_GE: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], a >= b);
				
			if (enable_tracing)
				PrintTrace("GE %s >= %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_LE: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], a <= b);
				
			if (enable_tracing)
				PrintTrace("LE %s <= %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_GT: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], a > b);
				
			if (enable_tracing)
				PrintTrace("GT %s > %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_LT: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], a < b);
				
			if (enable_tracing)
				PrintTrace("LT %s < %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_AND: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], a && b);
				
			if (enable_tracing)
				PrintTrace("AND %s && %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_OR: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], a || b);
				
			if (enable_tracing)
				PrintTrace("OR %s || %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		
		case OP_NOT_F: {
			const auto &a = GetGlobal<vec_t>(operands[0]);

			SetGlobal<vec_t>(operands[2], !a);

			if (enable_tracing)
				PrintTrace("NOT_F !%s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_NOT_V: {
			const auto &a = GetGlobal<vec3_t>(operands[0]);

			SetGlobal<vec_t>(operands[2], a == vec3_origin);
				
			if (enable_tracing)
				PrintTrace("NOT_V !%s = %s", TraceGlobal(operands[0], TYPE_VECTOR).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_NOT_S: {
			const auto &a = GetGlobal<string_t>(operands[0]);

			SetGlobal<vec_t>(operands[2], a == string_t::STRING_EMPTY || !*GetString(a));
				
			if (enable_tracing)
				PrintTrace("NOT_S !%s = %s", TraceGlobal(operands[0], TYPE_STRING).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_NOT_ENT:
		case OP_NOT_FNC: {
			const auto &a = GetGlobal<int32_t>(operands[0]);

			SetGlobal<vec_t>(operands[2], a == 0);
				
			if (enable_tracing)
				PrintTrace("NOT !%s = %s", TraceGlobal(operands[0], OpcodeType(statement.opcode)).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }

		case OP_EQ_F: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], a == b);
				
			if (enable_tracing)
				PrintTrace("EQ_F %s == %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_EQ_V: {
			const auto &a = GetGlobal<vec3_t>(operands[0]);
			const auto &b = GetGlobal<vec3_t>(operands[1]);
				
			SetGlobal<vec_t>(operands[2], a == b);
				
			if (enable_tracing)
				PrintTrace("EQ_V %s == %s = %s", TraceGlobal(operands[0], TYPE_VECTOR).data(), TraceGlobal(operands[1], TYPE_VECTOR).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_EQ_S: {
			const auto &l = GetString(GetGlobal<string_t>(operands[0]));
			const auto &r = GetString(GetGlobal<string_t>(operands[1]));

			SetGlobal<vec_t>(operands[2], !strcmp(l, r));

			if (enable_tracing)
				PrintTrace("EQ_S %s == %s = %s", TraceGlobal(operands[0], TYPE_STRING).data(), TraceGlobal(operands[1], TYPE_STRING).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_EQ_E:
		case OP_EQ_FNC: {
			const auto &a = GetGlobal<int32_t>(operands[0]);
			const auto &b = GetGlobal<int32_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], a == b);

			if (enable_tracing)
				PrintTrace("EQ %s == %s = %s", TraceGlobal(operands[0], OpcodeType(statement.opcode)).data(), TraceGlobal(operands[1], OpcodeType(statement.opcode)).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }

		case OP_NE_F: {
			const auto &a = GetGlobal<vec_t>(operands[0]);
			const auto &b = GetGlobal<vec_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], a != b);
				
			if (enable_tracing)
				PrintTrace("NE_F %s != %s = %s", TraceGlobal(operands[0], TYPE_FLOAT).data(), TraceGlobal(operands[1], TYPE_FLOAT).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_NE_V: {
			const auto &a = GetGlobal<vec3_t>(operands[0]);
			const auto &b = GetGlobal<vec3_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], a != b);
				
			if (enable_tracing)
				PrintTrace("NE_V %s != %s = %s", TraceGlobal(operands[0], TYPE_VECTOR).data(), TraceGlobal(operands[1], TYPE_VECTOR).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_NE_S: {
			const auto &l = GetString(GetGlobal<string_t>(operands[0]));
			const auto &r = GetString(GetGlobal<string_t>(operands[1]));

			SetGlobal<vec_t>(operands[2], strcmp(l, r));

			if (enable_tracing)
				PrintTrace("NE_S %s == %s = %s", TraceGlobal(operands[0], TYPE_STRING).data(), TraceGlobal(operands[1], TYPE_STRING).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }
		case OP_NE_E:
		case OP_NE_FNC: {
			const auto &a = GetGlobal<int32_t>(operands[0]);
			const auto &b = GetGlobal<int32_t>(operands[1]);

			SetGlobal<vec_t>(operands[2], a != b);

			if (enable_tracing)
				PrintTrace("NE %s != %s = %s", TraceGlobal(operands[0], OpcodeType(statement.opcode)).data(), TraceGlobal(operands[1], OpcodeType(statement.opcode)).data(), TraceGlobal(operands[2], TYPE_FLOAT).data());
			break; }

		case OP_STORE_F:
		case OP_STORE_ENT:
		case OP_STORE_FLD:		// integers
		case OP_STORE_S:
		case OP_STORE_FNC: {		// pointers
			const int32_t &value = GetGlobal<int32_t>(operands[0]);
			SetGlobal<int32_t>(operands[1], value);

			if (operands[1] >= global_t::PARM0 && operands[1] < global_t::QC_OFS)
				params_from[operands[1]] = operands[0];
				
			if (enable_tracing)
				PrintTrace("STORE %s -> %s", TraceGlobal(operands[0], OpcodeType(statement.opcode)).data(), TraceGlobal(operands[1], OpcodeType(statement.opcode)).data());
			break; }
		case OP_STORE_V: {
			const vec3_t &value = GetGlobal<vec3_t>(operands[0]);
			SetGlobal<vec3_t>(operands[1], value);

			if (operands[1] >= global_t::PARM0 && operands[1] < global_t::QC_OFS)
				params_from[operands[1]] = operands[0];
				
			if (enable_tracing)
				PrintTrace("STORE %s -> %s", TraceGlobal(operands[0], OpcodeType(statement.opcode)).data(), TraceGlobal(operands[1], OpcodeType(statement.opcode)).data());
			break; }


		// Get address of entity field.
		case OP_ADDRESS: {
			auto &ent = *EntToEntity(GetGlobal<ent_t>(operands[0]), true);
			auto field = GetGlobal<int32_t>(operands[1]);
			SetGlobal<int32_t>(operands[2], EntityFieldAddress(ent, field));
			if (enable_tracing)
				PrintTrace("ADDRESS : %s %s -> %s", TraceGlobal(operands[0], TYPE_ENTITY).data(), TraceGlobal(operands[1], TYPE_FIELD).data(), TraceGlobal(operands[2], TYPE_POINTER).data());
			break; }


		// Store arg into operand
		case OP_STOREP_F:
		case OP_STOREP_ENT:
		case OP_STOREP_FLD:		// integers
		case OP_STOREP_S:
		case OP_STOREP_FNC: {		// pointers
			const auto &address = GetGlobal<int32_t>(operands[1]);
			const auto &value = GetGlobal<int32_t>(operands[0]);

			AddressToEntityField<uint32_t>(address) = value;
				
			auto &ent = AddressToEntity(address);
			const auto &field = AddressToField(ent, address);
			field_wraps.WrapField(ent, field, &value);
				
			if (enable_tracing)
				PrintTrace("STOREP %s -> %s %s", TraceGlobal(operands[0], OpcodeType(statement.opcode)).data(), TraceEntity(&ent).data(), TraceField(field).data());
			break; }
		case OP_STOREP_V: {
			const auto &address = GetGlobal<int32_t>(operands[1]);
			const auto &value = GetGlobal<vec3_t>(operands[0]);

			AddressToEntityField<vec3_t>(address) = value;

			auto &ent = AddressToEntity(address);
			const auto &field = AddressToField(ent, address);
			field_wraps.WrapField(ent, field, &value[0]);
			field_wraps.WrapField(ent, field + 4, &value[1]);
			field_wraps.WrapField(ent, field + 8, &value[2]);
				
			if (enable_tracing)
				PrintTrace("STOREP %s -> %s %s", TraceGlobal(operands[0], OpcodeType(statement.opcode)).data(), TraceEntity(&ent).data(), TraceField(field).data());
			break; }
				

		// Load operand into return
		case OP_LOAD_F:
		case OP_LOAD_FLD:
		case OP_LOAD_ENT:
		case OP_LOAD_S:
		case OP_LOAD_FNC: {
			auto &ent = *EntToEntity(GetGlobal<ent_t>(operands[0]), true);

			if (&ent < globals.edicts)
				__debugbreak();

			auto &field_offset = GetGlobal<int32_t>(operands[1]);
			auto &field_value = *reinterpret_cast<int32_t *>(reinterpret_cast<int32_t*>(&ent) + field_offset);
			SetGlobal<int32_t>(operands[2], field_value);
			if (enable_tracing)
				PrintTrace("LOAD : %s %s -> %s", TraceGlobal(operands[0], TYPE_ENTITY).data(), TraceGlobal(operands[1], TYPE_FIELD).data(), TraceGlobal(operands[2], OpcodeType(statement.opcode)).data());
			break; }
		case OP_LOAD_V: {
			auto &ent = *EntToEntity(GetGlobal<ent_t>(operands[0]), true);
			auto &field_offset = GetGlobal<int32_t>(operands[1]);
			auto &field_value = *reinterpret_cast<vec3_t *>(reinterpret_cast<int32_t*>(&ent) + field_offset);
			SetGlobal<vec3_t>(operands[2], field_value);
			if (enable_tracing)
				PrintTrace("LOAD : %s %s -> %s", TraceGlobal(operands[0], TYPE_ENTITY).data(), TraceGlobal(operands[1], TYPE_FIELD).data(), TraceGlobal(operands[2], OpcodeType(statement.opcode)).data());
			break; }


		case OP_IFNOT:
			if (!GetGlobal<int32_t>(operands[0]))
			{
				state.current.statement += static_cast<int16_t>(statement.args[1]) - 1;
#ifdef ALLOW_PROFILING
				state.current.profile->fields[NumConditionalJumps]++;
#endif
			}
			if (enable_tracing)
				PrintTrace("IFNOT %s GOTO + %i (%s)", TraceGlobal(operands[0]).data(), static_cast<int16_t>(statement.args[1]), !GetGlobal<int32_t>(operands[0]) ? "passed" : "failed");
			break;

		case OP_IF:
			if (GetGlobal<int32_t>(operands[0]))
			{
				state.current.statement += static_cast<int16_t>(statement.args[1]) - 1;
#ifdef ALLOW_PROFILING
				state.current.profile->fields[NumConditionalJumps]++;
#endif
			}
			if (enable_tracing)
				PrintTrace("IF %s GOTO + %i (%s)", TraceGlobal(operands[0]).data(), static_cast<int16_t>(statement.args[1]), GetGlobal<int32_t>(operands[0]) ? "passed" : "failed");
			break;

		case OP_GOTO:
			state.current.statement += static_cast<int16_t>(statement.args[0]) - 1;

#ifdef ALLOW_PROFILING
			state.current.profile->fields[NumUnconditionalJumps]++;
#endif

			if (enable_tracing)
				PrintTrace("GOTO + %i", static_cast<int16_t>(statement.args[0]));
			break;

		case OP_CALL0:
		case OP_CALL1:
		case OP_CALL2:
		case OP_CALL3:
		case OP_CALL4:
		case OP_CALL5:
		case OP_CALL6:
		case OP_CALL7:
		case OP_CALL8: {
				const int32_t &enter_func = GetGlobal<int32_t>(operands[0]);

				state.argc = static_cast<uint8_t>(statement.opcode - OP_CALL0);
				if (!enter_func)
					Error("NULL function");

#ifdef ALLOW_PROFILING
				state.current.profile->fields[NumFuncCalls]++;
#endif

				QCFunction &call = functions[enter_func];
					
				if (enable_tracing)
					PrintTrace("CALL(%u) %s", state.argc, TraceFunction(enter_func).data());

				if (!call.id)
					Error("Tried to call missing function %s", GetString(call.name_index));

				if (call.id < 0)
				{
					// negative statements are built in functions
					CallBuiltin(call);
					break;
				}

				enter_depth++;
				Enter(call);
				break;
			}

		case OP_DONE:
		case OP_RETURN:
#ifdef ALLOW_PROFILING
			state.current.profile->fields[NumConditionalJumps]++;
#endif

			if (operands[0].arg)
			{
				for (size_t i = 0; i < 3; i++)
					SetGlobal<int32_t>(GlobalOffset(global_t::RETURN, i), GetGlobal<int32_t>(GlobalOffset(operands[0], i)));
					
				if (enable_tracing)
					PrintTrace("RETURN %s", TraceGlobal(operands[0]).data());
			}
	
			Leave();

			enter_depth--;

			if (!enter_depth)
				return;		// all done

			break;

		case OP_STATE:
			Error("STATE is not a valid OP in Q2QC");
			
		case OP_GLOBALADDRESS:
			SetGlobal<int32_t>(operands[2], reinterpret_cast<int32_t>(&GetGlobal<int32_t>(operands[0]) + GetGlobal<int32_t>(operands[1])));
			break;

		default:
			Error("Unknown opcode %i", statement.opcode);
		}
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

	qvm.statements.resize(header.sections.statement.size);

	stream.seekg(header.sections.statement.offset);
	stream.read(reinterpret_cast<char *>(qvm.statements.data()), header.sections.statement.size * sizeof(QCStatement));
	
	qvm.definitions.resize(header.sections.definition.size);

	stream.seekg(header.sections.definition.offset);
	stream.read(reinterpret_cast<char *>(qvm.definitions.data()), header.sections.definition.size * sizeof(QCDefinition));

	qvm.fields.resize(header.sections.field.size);

	stream.seekg(header.sections.field.offset);
	stream.read(reinterpret_cast<char *>(qvm.fields.data()), header.sections.field.size * sizeof(QCDefinition));

	qvm.functions.resize(header.sections.function.size);
#ifdef ALLOW_PROFILING
	qvm.profile_data.resize(header.sections.function.size);
#endif

	stream.seekg(header.sections.function.offset);
	stream.read(reinterpret_cast<char *>(qvm.functions.data()), header.sections.function.size * sizeof(QCFunction));

	qvm.global_data = reinterpret_cast<global_t *>(gi.TagMalloc(header.sections.globals.size * sizeof(global_t), TAG_GAME));

	stream.seekg(header.sections.globals.offset);
	stream.read(reinterpret_cast<char *>(qvm.global_data), header.sections.globals.size * sizeof(global_t));

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
#endif
}