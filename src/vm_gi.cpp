#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"
#include "vm_game.h"

static void QC_sound(QCVM &vm)
{
	edict_t *entity = vm.ArgvEntity(0);
	const sound_channel_t channel = vm.ArgvInt32(1);
	const int32_t soundindex = vm.ArgvInt32(2);
	const vec_t volume = vm.ArgvFloat(3);
	const vec_t attenuation = vm.ArgvFloat(4);
	const vec_t timeofs = vm.ArgvFloat(5);

	gi.sound(entity, channel, soundindex, volume, attenuation, timeofs);
}

static void QC_positioned_sound(QCVM &vm)
{
	const vec3_t position = vm.ArgvVector(0);
	edict_t *entity = vm.ArgvEntity(1);
	const sound_channel_t channel = vm.ArgvInt32(2);
	const int32_t soundindex = vm.ArgvInt32(3);
	const vec_t volume = vm.ArgvFloat(4);
	const vec_t attenuation = vm.ArgvFloat(5);
	const vec_t timeofs = vm.ArgvFloat(6);

	gi.positioned_sound(position, entity, channel, soundindex, volume, attenuation, timeofs);
}

static void QC_cvar_get_name(QCVM &vm)
{
	const cvar_t *cvar = (cvar_t *)vm.ArgvInt32(0);
	vm.ReturnString(std::string(cvar->name));
}

static void QC_cvar_get_string(QCVM &vm)
{
	const cvar_t *cvar = (cvar_t *)vm.ArgvInt32(0);
	vm.ReturnString(std::string(cvar->string));
}

static void QC_cvar_get_latched_string(QCVM &vm)
{
	const cvar_t *cvar = (cvar_t *)vm.ArgvInt32(0);
	vm.ReturnString(std::string(cvar->latched_string));
}

static void QC_cvar_get_modified(QCVM &vm)
{
	const cvar_t *cvar = (cvar_t *)vm.ArgvInt32(0);
	vm.ReturnInt(cvar->modified);
}
static void QC_cvar_get_flags(QCVM &vm)
{
	const cvar_t *cvar = (cvar_t *)vm.ArgvInt32(0);
	vm.ReturnInt(cvar->flags);
}

static void QC_cvar_set_modified(QCVM &vm)
{
	cvar_t *cvar = (cvar_t *)vm.ArgvInt32(0);
	const qboolean value = (qboolean)vm.ArgvInt32(1);

	cvar->modified = value;
}

static void QC_cvar_get_floatVal(QCVM &vm)
{
	const cvar_t *cvar = (cvar_t *)vm.ArgvInt32(0);
	vm.ReturnFloat(cvar->value);
}

static void QC_cvar_get_intVal(QCVM &vm)
{
	const cvar_t *cvar = (cvar_t *)vm.ArgvInt32(0);
	vm.ReturnInt((int32_t)cvar->value);
}

static void QC_cvar(QCVM &vm)
{
	const char *name = vm.ArgvString(0);
	const char *value = vm.ArgvString(1);
	const cvar_flags_t flags = vm.ArgvInt32(2);

	cvar_t *cvar = gi.cvar(name, value, flags);

	vm.ReturnInt((int32_t)cvar);
}

static void QC_cvar_set(QCVM &vm)
{
	const char *name = vm.ArgvString(0);
	const char *value = vm.ArgvString(1);
	gi.cvar_set(name, value);
}

static void QC_cvar_forceset(QCVM &vm)
{
	const char *name = vm.ArgvString(0);
	const char *value = vm.ArgvString(1);
	gi.cvar_forceset(name, value);
}

static void QC_configstring(QCVM &vm)
{
	const config_string_t id = vm.ArgvInt32(0);
	const char *str = vm.ArgvString(1);

	gi.configstring(id, str);
}

[[noreturn]] static void QC_error(QCVM &vm)
{
	const string_t fmtid = vm.ArgvStringID(0);
	vm.Error(ParseFormat(fmtid, vm, 1).data());
}

static void QC_modelindex(QCVM &vm)
{
	const char *str = vm.ArgvString(0);
	vm.ReturnInt(gi.modelindex(str));
}

static void QC_soundindex(QCVM &vm)
{
	const char *str = vm.ArgvString(0);
	vm.ReturnInt(gi.soundindex(str));
}

static void QC_imageindex(QCVM &vm)
{
	const char *str = vm.ArgvString(0);
	vm.ReturnInt(gi.imageindex(str));
}

static void QC_setmodel(QCVM &vm)
{
	edict_t *ent = vm.ArgvEntity(0);
	const char *str = vm.ArgvString(1);

	gi.setmodel(ent, str);
}

typedef int QC_csurface_t;

static void QC_csurface_get_name(QCVM &vm)
{
	const csurface_t *surf = (csurface_t *)vm.ArgvInt32(0);

	if (!surf)
		vm.ReturnString(STRING_EMPTY);
	else
		vm.ReturnString(std::string(surf->name));
}

static void QC_csurface_get_flags(QCVM &vm)
{
	const csurface_t *surf = (csurface_t *)vm.ArgvInt32(0);

	if (!surf)
		vm.ReturnInt(0);
	else
		vm.ReturnInt(surf->flags);
}

static void QC_csurface_get_value(QCVM &vm)
{
	const csurface_t *surf = (csurface_t *)vm.ArgvInt32(0);

	if (!surf)
		vm.ReturnInt(0);
	else
		vm.ReturnInt(surf->value);
}

struct QC_trace_t
{
	int				allsolid;
	int				startsolid;
	float			fraction;
	vec3_t			endpos;
	cplane_t		plane;
	QC_csurface_t	surface;
	int				contents;
	ent_t			ent;
};

static void QC_trace(QCVM &vm)
{
	QC_trace_t *trace = vm.GetGlobalPtr<QC_trace_t>(GLOBAL_PARM0);
	const vec3_t start = vm.ArgvVector(1);
	const vec3_t mins = vm.ArgvVector(2);
	const vec3_t maxs = vm.ArgvVector(3);
	const vec3_t end = vm.ArgvVector(4);
	edict_t *ent = vm.ArgvEntity(5);
	const content_flags_t contents = vm.ArgvInt32(6);

	trace_t trace_result = gi.trace(start, mins, maxs, end, ent, contents);
	
	trace->allsolid = trace_result.allsolid;
	trace->startsolid = trace_result.startsolid;
	trace->fraction = trace_result.fraction;
	trace->endpos = trace_result.endpos;
	trace->plane = trace_result.plane;
	trace->surface = (QC_csurface_t)trace_result.surface;
	trace->contents = trace_result.contents;
	trace->ent = vm.EntityToEnt(trace_result.ent);
	vm.dynamic_strings.CheckRefUnset(trace, sizeof(*trace) / sizeof(global_t));
}

static void QC_pointcontents(QCVM &vm)
{
	const vec3_t pos = vm.ArgvVector(0);
	vm.ReturnInt(gi.pointcontents(pos));
}

static void QC_inPVS(QCVM &vm)
{
	const vec3_t a = vm.ArgvVector(0);
	const vec3_t b = vm.ArgvVector(1);
	vm.ReturnInt(gi.inPVS(a, b));
}

static void QC_inPHS(QCVM &vm)
{
	const vec3_t a = vm.ArgvVector(0);
	const vec3_t b = vm.ArgvVector(1);
	vm.ReturnInt(gi.inPHS(a, b));
}

static void QC_SetAreaPortalState(QCVM &vm)
{
	const int32_t num = vm.ArgvInt32(0);
	const qboolean state = (qboolean)vm.ArgvInt32(1);

	gi.SetAreaPortalState(num, state);
}

static void QC_AreasConnected(QCVM &vm)
{
	const int32_t a = vm.ArgvInt32(0);
	const int32_t b = vm.ArgvInt32(1);
	vm.ReturnInt(gi.AreasConnected(a, b));
}

static void QC_linkentity(QCVM &vm)
{
	edict_t *ent = vm.ArgvEntity(0);
	gi.linkentity(ent);
}

static void QC_unlinkentity(QCVM &vm)
{
	edict_t *ent = vm.ArgvEntity(0);
	gi.unlinkentity(ent);
}

typedef struct entity_set_link_s
{
	edict_t						*entity;
	struct entity_set_link_s	*next;
} entity_set_link_t;

typedef struct
{
	entity_set_link_t	*head;
	size_t				count, allocated;
} entity_set_t;

static entity_set_t *entity_set_alloc(const size_t reserved)
{
	entity_set_t *set = (entity_set_t *)gi.TagMalloc(sizeof(entity_set_t), TAG_GAME);

	if (!reserved)
		return set;

	set->allocated = reserved;

	entity_set_link_t **tail = &set->head;

	for (size_t i = 0; i < reserved; i++)
	{
		*tail = (entity_set_link_t *)gi.TagMalloc(sizeof(entity_set_link_t), TAG_GAME);
		tail = &(*tail)->next;
	}

	return set;
}

static edict_t *entity_set_get(const entity_set_t *set, size_t index)
{
	if (index >= set->count)
		qvm.Error("Out of bounds access");

	entity_set_link_t *link;
	
	for (link = set->head; index && link; index--, link = link->next) ;
	
#ifdef _DEBUG
	if (index || !link->entity)
		qvm.Error("Out of bounds access");
#endif

	return link->entity;
}

static void entity_set_add(entity_set_t *set, edict_t *ent)
{
	for (entity_set_link_t **link = &set->head; ; link = &(*link)->next)
	{
		if (!(*link))
		{
			*link = (entity_set_link_t *)gi.TagMalloc(sizeof(entity_set_link_t), TAG_GAME);
			set->allocated++;
		}

		if (!(*link)->entity)
		{
			(*link)->entity = ent;
			set->count++;
			return;
		}
		else if ((*link)->entity == ent)
			return;
	}
}

static void entity_set_remove(entity_set_t *set, edict_t *ent)
{
	for (entity_set_link_t **link = &set->head; (*link) && (*link)->entity; link = &(*link)->next)
	{
		if ((*link)->entity != ent)
			continue;

		(*link)->entity = nullptr;

		entity_set_link_t *removed_link = *link;
		*link = (*link)->next;

		while ((*link)->next) link = &(*link)->next;

		(*link)->next = removed_link;
		set->count--;
		set->allocated--;
		return;
	}

	qvm.Error("Remove entity not in list");
}

static void entity_set_clear(entity_set_t *set)
{
	set->count = 0;

	for (entity_set_link_t *link = set->head; link; link = link->next)
		link->entity = nullptr;
}

static void entity_set_free(entity_set_t *set)
{
	for (entity_set_link_t *link = set->head; link; )
	{
		entity_set_link_t *next = link->next;
		gi.TagFree(link);
		link = next;
	}

	gi.TagFree(set);
}

static void QC_entity_set_alloc(QCVM &vm)
{
	entity_set_t *set = entity_set_alloc((vm.state.argc && vm.ArgvInt32(0) > 1) ? vm.ArgvInt32(0) : 0);
	vm.ReturnInt((int32_t)set);
}

static void QC_entity_set_get(QCVM &vm)
{
	const entity_set_t *set = (entity_set_t *)vm.ArgvInt32(0);
	const int32_t index = vm.ArgvInt32(1);

	vm.ReturnEntity(entity_set_get(set, index));
}

static void QC_entity_set_add(QCVM &vm)
{
	entity_set_t *set = (entity_set_t *)vm.ArgvInt32(0);
	edict_t *ent = vm.ArgvEntity(1);
	entity_set_add(set, ent);
}

static void QC_entity_set_remove(QCVM &vm)
{
	entity_set_t *set = (entity_set_t *)vm.ArgvInt32(0);
	edict_t *ent = vm.ArgvEntity(1);
	entity_set_remove(set, ent);
}

static void QC_entity_set_length(QCVM &vm)
{
	entity_set_t *set = (entity_set_t *)vm.ArgvInt32(0);
	vm.ReturnInt((int32_t)set->count);
}

static void QC_entity_set_clear(QCVM &vm)
{
	entity_set_t *set = (entity_set_t *)vm.ArgvInt32(0);
	entity_set_clear(set);
}

static void QC_entity_set_free(QCVM &vm)
{
	entity_set_t *set = (entity_set_t *)vm.ArgvInt32(0);
	entity_set_free(set);
}

static void QC_BoxEdicts(QCVM &vm)
{
	entity_set_t *set = (entity_set_t *)vm.ArgvInt32(0);
	const vec3_t mins = vm.ArgvVector(1);
	const vec3_t maxs = vm.ArgvVector(2);
	const int32_t maxcount = vm.ArgvInt32(3);
	const box_edicts_area_t areatype = vm.ArgvInt32(4);
	static edict_t *raw_entities[MAX_EDICTS]; // FIXME: too big :(

	const int32_t count = gi.BoxEdicts(mins, maxs, raw_entities, min(32, maxcount), areatype);

	entity_set_clear(set);

	for (int32_t i = 0; i < count; i++)
		entity_set_add(set, raw_entities[i]);
}

struct QC_pmove_state_t
{
    pmtype_t pm_type;

    vec3_t origin;		// 12.3
    vec3_t velocity;	// 12.3
    int	pm_flags;		// ducked, jump_held, etc
    int	pm_time;		// each unit = 8 ms
    int	gravity;
    vec3_t delta_angles;	// add to command angles to get view direction
							// changed by spawns, rotating objects, and teleporters
};

struct QC_pmove_t
{
	// inout
	QC_pmove_state_t	s;
	
	// in
	QC_usercmd_t	cmd;
	bool			snapinitial;
	
	// out
	entity_set_t	*touchents;
	
	vec3_t	viewangles;
	float	viewheight;
	
	vec3_t	mins, maxs;
	
	ent_t	groundentity;
	int		watertype;
	int		waterlevel;
	
	// in (callbacks)
	func_t trace;
	func_t pointcontents;
};

static entity_set_t touchents_memory;

static func_t QC_pm_pointcontents_func;

static content_flags_t QC_pm_pointcontents(const vec3_t &position)
{
	QCFunction *func = qvm.FindFunction(QC_pm_pointcontents_func);
	qvm.SetGlobal(GLOBAL_PARM0, position);
	qvm.Execute(func);
	return qvm.GetGlobal<content_flags_t>(GLOBAL_RETURN);
}

static func_t QC_pm_trace_func;

static trace_t QC_pm_trace(const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end)
{
	QC_trace_t qc_tr;

	QCFunction *func = qvm.FindFunction(QC_pm_trace_func);
	qvm.SetAllowedStack(&qc_tr, sizeof(qc_tr));
	qvm.SetGlobal(GLOBAL_PARM0, (ptrdiff_t)&qc_tr);
	qvm.SetGlobal(GLOBAL_PARM1, start);
	qvm.SetGlobal(GLOBAL_PARM2, mins);
	qvm.SetGlobal(GLOBAL_PARM3, maxs);
	qvm.SetGlobal(GLOBAL_PARM4, end);
	qvm.Execute(func);

	trace_t tr;
	tr.allsolid = (qboolean)qc_tr.allsolid;
	tr.startsolid = (qboolean)qc_tr.startsolid;
	tr.fraction = qc_tr.fraction;
	tr.endpos = qc_tr.endpos;
	tr.plane = qc_tr.plane;
	tr.surface = (csurface_t *)qc_tr.surface;
	tr.contents = (content_flags_t)qc_tr.contents;
	tr.ent = qvm.EntToEntity(qc_tr.ent);

	return tr;
}

static void QC_Pmove(QCVM &vm)
{
	QC_pmove_t *qc_pm = vm.GetGlobalPtr<QC_pmove_t>(GLOBAL_PARM0);

	pmove_t pm;

	// set in parameters
	pm.s.pm_type = qc_pm->s.pm_type;
	for (int32_t i = 0; i < 3; i++)
	{
		pm.s.origin[i] = qc_pm->s.origin[i] * coord2short;
		pm.s.velocity[i] = qc_pm->s.velocity[i] * coord2short;
		pm.s.delta_angles[i] = qc_pm->s.delta_angles[i] * angle2short;
		pm.cmd.angles[i] = qc_pm->cmd.angles[i] * angle2short;
	}
	pm.s.pm_flags = (pmflags_t)qc_pm->s.pm_flags;
	pm.s.pm_time = qc_pm->s.pm_time;
	pm.s.gravity = qc_pm->s.gravity;
	pm.cmd.buttons = (button_bits_t)qc_pm->cmd.buttons;
	pm.cmd.forwardmove = qc_pm->cmd.forwardmove;
	pm.cmd.impulse = qc_pm->cmd.impulse;
	pm.cmd.lightlevel = qc_pm->cmd.lightlevel;
	pm.cmd.msec = qc_pm->cmd.msec;
	pm.cmd.sidemove = qc_pm->cmd.sidemove;
	pm.cmd.upmove = qc_pm->cmd.upmove;

	QC_pm_pointcontents_func = qc_pm->pointcontents;
	pm.pointcontents = QC_pm_pointcontents;

	QC_pm_trace_func = qc_pm->trace;
	pm.trace = QC_pm_trace;

	gi.Pmove(&pm);

	// copy out parameters
	qc_pm->s.pm_type = pm.s.pm_type;
	for (int32_t i = 0; i < 3; i++)
	{
		qc_pm->s.origin[i] = pm.s.origin[i] * short2coord;
		qc_pm->s.velocity[i] = pm.s.velocity[i] * short2coord;
		qc_pm->s.delta_angles[i] = pm.s.delta_angles[i] * short2angle;
	}
	qc_pm->s.pm_flags = pm.s.pm_flags;
	qc_pm->s.pm_time = pm.s.pm_time;
	qc_pm->s.gravity = pm.s.gravity;

	qc_pm->touchents = &touchents_memory;
	entity_set_clear(&touchents_memory);

	for (int32_t i = 0; i < pm.numtouch; i++)
		entity_set_add(&touchents_memory, pm.touchents[i]);

	qc_pm->viewangles = pm.viewangles;
	qc_pm->viewheight = pm.viewheight;
	
	qc_pm->mins = pm.mins;
	qc_pm->maxs = pm.maxs;

	if (!pm.groundentity)
		qc_pm->groundentity = ENT_INVALID;
	else
		qc_pm->groundentity = vm.EntityToEnt(pm.groundentity);
	qc_pm->watertype = pm.watertype;
	qc_pm->waterlevel = pm.waterlevel;
	vm.dynamic_strings.CheckRefUnset(qc_pm, sizeof(*qc_pm) / sizeof(global_t));
}

static void QC_multicast(QCVM &vm)
{
	const vec3_t pos = vm.ArgvVector(0);
	const multicast_t type = vm.ArgvInt32(1);

	gi.multicast(pos, type);
}

static void QC_unicast(QCVM &vm)
{
	edict_t *ent = vm.ArgvEntity(0);
	const qboolean reliable = (qboolean)vm.ArgvInt32(1);

	gi.unicast(ent, reliable);
}

static void QC_WriteChar(QCVM &vm)
{
	const int32_t val = vm.ArgvInt32(0);

	gi.WriteChar(val);
}

static void QC_WriteByte(QCVM &vm)
{
	const int32_t val = vm.ArgvInt32(0);

	gi.WriteByte(val);
}

static void QC_WriteShort(QCVM &vm)
{
	const int32_t val = vm.ArgvInt32(0);

	gi.WriteShort(val);
}

static void QC_WriteLong(QCVM &vm)
{
	const int32_t val = vm.ArgvInt32(0);

	gi.WriteLong(val);
}

static void QC_WriteFloat(QCVM &vm)
{
	const vec_t val = vm.ArgvFloat(0);

	gi.WriteFloat(val);
}

static void QC_WriteString(QCVM &vm)
{
	const char *val = vm.ArgvString(0);

	gi.WriteString(val);
}

static void QC_WritePosition(QCVM &vm)
{
	const vec3_t val = vm.ArgvVector(0);

	gi.WritePosition(val);
}

static void QC_WriteDir(QCVM &vm)
{
	const vec3_t val = vm.ArgvVector(0);

	gi.WriteDir(val);
}

static void QC_WriteAngle(QCVM &vm)
{
	const vec_t val = vm.ArgvFloat(0);

	gi.WriteAngle(val);
}

static void QC_argv(QCVM &vm)
{
	const int32_t n = vm.ArgvInt32(0);
	vm.ReturnString(std::string(gi.argv(n)));
}

static void QC_argc(QCVM &vm)
{
	vm.ReturnInt(gi.argc());
}

static void QC_args(QCVM &vm)
{
	vm.ReturnString(std::string(gi.args()));
}

static void QC_AddCommandString(QCVM &vm)
{
	gi.AddCommandString(vm.ArgvString(0));
}

static void QC_bprintf(QCVM &vm)
{
	const print_level_t level = vm.ArgvInt32(0);
	const string_t fmtid = vm.ArgvStringID(1);
	gi.bprintf(level, "%s", ParseFormat(fmtid, vm, 2).data());
}

static void QC_dprintf(QCVM &vm)
{
	const string_t fmtid = vm.ArgvStringID(0);
	gi.dprintf("%s", ParseFormat(fmtid, vm, 1).data());
}

static void QC_cprintf(QCVM &vm)
{
	edict_t *ent = vm.ArgvEntity(0);
	const print_level_t level = vm.ArgvInt32(1);
	const string_t fmtid = vm.ArgvStringID(2);
	gi.cprintf(ent, level, "%s", ParseFormat(fmtid, vm, 3).data());
}

static void QC_centerprintf(QCVM &vm)
{
	edict_t *ent = vm.ArgvEntity(0);
	const string_t fmtid = vm.ArgvStringID(1);
	gi.centerprintf(ent, "%s", ParseFormat(fmtid, vm, 2).data());
}

static void QC_DebugGraph(QCVM &vm)
{
	const vec_t a = vm.ArgvFloat(0);
	const int32_t b = vm.ArgvInt32(1);
	gi.DebugGraph(a, b);
}

void InitGIBuiltins(QCVM &vm)
{
	RegisterBuiltin(bprintf);
	RegisterBuiltin(dprintf);
	RegisterBuiltin(cprintf);
	RegisterBuiltin(centerprintf);
	RegisterBuiltin(sound);
	RegisterBuiltin(positioned_sound);
	RegisterBuiltin(cvar);
	RegisterBuiltin(cvar_set);
	RegisterBuiltin(cvar_forceset);

	RegisterBuiltin(configstring);
	
	RegisterBuiltin(error);

	RegisterBuiltin(modelindex);
	RegisterBuiltin(soundindex);
	RegisterBuiltin(imageindex);

	RegisterBuiltin(setmodel);
	
	RegisterBuiltin(trace);
	RegisterBuiltin(pointcontents);
	RegisterBuiltin(inPVS);
	RegisterBuiltin(inPHS);
	RegisterBuiltin(SetAreaPortalState);
	RegisterBuiltin(AreasConnected);
	
	RegisterBuiltin(linkentity);
	RegisterBuiltin(unlinkentity);
	RegisterBuiltin(BoxEdicts);
	RegisterBuiltin(Pmove);
	
	RegisterBuiltin(multicast);
	RegisterBuiltin(unicast);
	RegisterBuiltin(WriteChar);
	RegisterBuiltin(WriteByte);
	RegisterBuiltin(WriteShort);
	RegisterBuiltin(WriteLong);
	RegisterBuiltin(WriteFloat);
	RegisterBuiltin(WriteString);
	RegisterBuiltin(WritePosition);
	RegisterBuiltin(WriteDir);
	RegisterBuiltin(WriteAngle);
	
	RegisterBuiltin(argv);
	RegisterBuiltin(argc);
	RegisterBuiltin(args);

	RegisterBuiltin(AddCommandString);

	RegisterBuiltin(DebugGraph);
	
	RegisterBuiltin(entity_set_alloc);
	RegisterBuiltin(entity_set_get);
	RegisterBuiltin(entity_set_add);
	RegisterBuiltin(entity_set_remove);
	RegisterBuiltin(entity_set_length);
	RegisterBuiltin(entity_set_clear);
	RegisterBuiltin(entity_set_free);
	
	RegisterBuiltin(csurface_get_name);
	RegisterBuiltin(csurface_get_flags);
	RegisterBuiltin(csurface_get_value);
	
	RegisterBuiltin(cvar_get_name);
	RegisterBuiltin(cvar_get_string);
	RegisterBuiltin(cvar_get_latched_string);
	RegisterBuiltin(cvar_get_modified);
	RegisterBuiltin(cvar_set_modified);
	RegisterBuiltin(cvar_get_flags);
	RegisterBuiltin(cvar_get_floatVal);
	RegisterBuiltin(cvar_get_intVal);
}