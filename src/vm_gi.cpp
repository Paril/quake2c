#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"
#include "vm_game.h"

static void QC_sound(qcvm_t *vm)
{
	edict_t *entity = qcvm_argv_entity(vm, 0);
	const sound_channel_t channel = qcvm_argv_int32(vm, 1);
	const int32_t soundindex = qcvm_argv_int32(vm, 2);
	const vec_t volume = qcvm_argv_float(vm, 3);
	const vec_t attenuation = qcvm_argv_float(vm, 4);
	const vec_t timeofs = qcvm_argv_float(vm, 5);

	gi.sound(entity, channel, soundindex, volume, attenuation, timeofs);
}

static void QC_positioned_sound(qcvm_t *vm)
{
	const vec3_t position = qcvm_argv_vector(vm, 0);
	edict_t *entity = qcvm_argv_entity(vm, 1);
	const sound_channel_t channel = qcvm_argv_int32(vm, 2);
	const int32_t soundindex = qcvm_argv_int32(vm, 3);
	const vec_t volume = qcvm_argv_float(vm, 4);
	const vec_t attenuation = qcvm_argv_float(vm, 5);
	const vec_t timeofs = qcvm_argv_float(vm, 6);

	gi.positioned_sound(&position, entity, channel, soundindex, volume, attenuation, timeofs);
}

static void QC_cvar_get_name(qcvm_t *vm)
{
	const cvar_t *cvar = (cvar_t *)qcvm_argv_int32(vm, 0);
	qcvm_return_string(vm, cvar->name);
}

static void QC_cvar_get_string(qcvm_t *vm)
{
	const cvar_t *cvar = (cvar_t *)qcvm_argv_int32(vm, 0);
	qcvm_return_string(vm, cvar->string);
}

static void QC_cvar_get_latched_string(qcvm_t *vm)
{
	const cvar_t *cvar = (cvar_t *)qcvm_argv_int32(vm, 0);
	qcvm_return_string(vm, cvar->latched_string);
}

static void QC_cvar_get_modified(qcvm_t *vm)
{
	const cvar_t *cvar = (cvar_t *)qcvm_argv_int32(vm, 0);
	qcvm_return_int32(vm, cvar->modified);
}
static void QC_cvar_get_flags(qcvm_t *vm)
{
	const cvar_t *cvar = (cvar_t *)qcvm_argv_int32(vm, 0);
	qcvm_return_int32(vm, cvar->flags);
}

static void QC_cvar_set_modified(qcvm_t *vm)
{
	cvar_t *cvar = (cvar_t *)qcvm_argv_int32(vm, 0);
	const qboolean value = (qboolean)qcvm_argv_int32(vm, 1);

	cvar->modified = value;
}

static void QC_cvar_get_floatVal(qcvm_t *vm)
{
	const cvar_t *cvar = (cvar_t *)qcvm_argv_int32(vm, 0);
	qcvm_return_float(vm, cvar->value);
}

static void QC_cvar_get_intVal(qcvm_t *vm)
{
	const cvar_t *cvar = (cvar_t *)qcvm_argv_int32(vm, 0);
	qcvm_return_int32(vm, (int32_t)cvar->value);
}

static void QC_cvar(qcvm_t *vm)
{
	const char *name = qcvm_argv_string(vm, 0);
	const char *value = qcvm_argv_string(vm, 1);
	const cvar_flags_t flags = qcvm_argv_int32(vm, 2);

	cvar_t *cvar = gi.cvar(name, value, flags);

	qcvm_return_int32(vm, (int32_t)cvar);
}

static void QC_cvar_set(qcvm_t *vm)
{
	const char *name = qcvm_argv_string(vm, 0);
	const char *value = qcvm_argv_string(vm, 1);
	gi.cvar_set(name, value);
}

static void QC_cvar_forceset(qcvm_t *vm)
{
	const char *name = qcvm_argv_string(vm, 0);
	const char *value = qcvm_argv_string(vm, 1);
	gi.cvar_forceset(name, value);
}

static void QC_configstring(qcvm_t *vm)
{
	const config_string_t id = qcvm_argv_int32(vm, 0);
	const char *str = qcvm_argv_string(vm, 1);

	gi.configstring(id, str);
}

static void QC_error(qcvm_t *vm)
{
	const string_t fmtid = qcvm_argv_string_id(vm, 0);
	qcvm_error(vm, ParseFormat(fmtid, vm, 1).data());
}

static void QC_modelindex(qcvm_t *vm)
{
	const char *str = qcvm_argv_string(vm, 0);
	qcvm_return_int32(vm, gi.modelindex(str));
}

static void QC_soundindex(qcvm_t *vm)
{
	const char *str = qcvm_argv_string(vm, 0);
	qcvm_return_int32(vm, gi.soundindex(str));
}

static void QC_imageindex(qcvm_t *vm)
{
	const char *str = qcvm_argv_string(vm, 0);
	qcvm_return_int32(vm, gi.imageindex(str));
}

static void QC_setmodel(qcvm_t *vm)
{
	edict_t *ent = qcvm_argv_entity(vm, 0);
	const char *str = qcvm_argv_string(vm, 1);

	gi.setmodel(ent, str);
}

typedef int QC_csurface_t;

static void QC_csurface_get_name(qcvm_t *vm)
{
	const csurface_t *surf = (csurface_t *)qcvm_argv_int32(vm, 0);

	if (!surf)
		qcvm_return_string(vm, nullptr);
	else
		qcvm_return_string(vm, surf->name);
}

static void QC_csurface_get_flags(qcvm_t *vm)
{
	const csurface_t *surf = (csurface_t *)qcvm_argv_int32(vm, 0);

	if (!surf)
		qcvm_return_int32(vm, 0);
	else
		qcvm_return_int32(vm, surf->flags);
}

static void QC_csurface_get_value(qcvm_t *vm)
{
	const csurface_t *surf = (csurface_t *)qcvm_argv_int32(vm, 0);

	if (!surf)
		qcvm_return_int32(vm, 0);
	else
		qcvm_return_int32(vm, surf->value);
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

static void QC_trace(qcvm_t *vm)
{
	QC_trace_t *trace = qcvm_get_global_ptr_typed(QC_trace_t, vm, GLOBAL_PARM0);
	const vec3_t start = qcvm_argv_vector(vm, 1);
	const vec3_t mins = qcvm_argv_vector(vm, 2);
	const vec3_t maxs = qcvm_argv_vector(vm, 3);
	const vec3_t end = qcvm_argv_vector(vm, 4);
	edict_t *ent = qcvm_argv_entity(vm, 5);
	const content_flags_t contents = qcvm_argv_int32(vm, 6);

	trace_t trace_result = gi.trace(&start, &mins, &maxs, &end, ent, contents);
	trace->allsolid = trace_result.allsolid;
	trace->startsolid = trace_result.startsolid;
	trace->fraction = trace_result.fraction;
	trace->endpos = trace_result.endpos;
	trace->plane = trace_result.plane;
	trace->surface = (QC_csurface_t)trace_result.surface;
	trace->contents = trace_result.contents;
	trace->ent = qcvm_entity_to_ent(trace_result.ent);
	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, trace, sizeof(*trace) / sizeof(global_t), false);
}

static void QC_pointcontents(qcvm_t *vm)
{
	const vec3_t pos = qcvm_argv_vector(vm, 0);
	qcvm_return_int32(vm, gi.pointcontents(&pos));
}

static void QC_inPVS(qcvm_t *vm)
{
	const vec3_t a = qcvm_argv_vector(vm, 0);
	const vec3_t b = qcvm_argv_vector(vm, 1);
	qcvm_return_int32(vm, gi.inPVS(&a, &b));
}

static void QC_inPHS(qcvm_t *vm)
{
	const vec3_t a = qcvm_argv_vector(vm, 0);
	const vec3_t b = qcvm_argv_vector(vm, 1);
	qcvm_return_int32(vm, gi.inPHS(&a, &b));
}

static void QC_SetAreaPortalState(qcvm_t *vm)
{
	const int32_t num = qcvm_argv_int32(vm, 0);
	const qboolean state = (qboolean)qcvm_argv_int32(vm, 1);

	gi.SetAreaPortalState(num, state);
}

static void QC_AreasConnected(qcvm_t *vm)
{
	const int32_t a = qcvm_argv_int32(vm, 0);
	const int32_t b = qcvm_argv_int32(vm, 1);
	qcvm_return_int32(vm, gi.AreasConnected(a, b));
}

static void QC_linkentity(qcvm_t *vm)
{
	edict_t *ent = qcvm_argv_entity(vm, 0);
	gi.linkentity(ent);
}

static void QC_unlinkentity(qcvm_t *vm)
{
	edict_t *ent = qcvm_argv_entity(vm, 0);
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
		gi.error("Out of bounds access");

	entity_set_link_t *link;
	
	for (link = set->head; index && link; index--, link = link->next) ;
	
#ifdef _DEBUG
	if (index || !link->entity)
		gi.error("Out of bounds access");
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

	gi.error("Remove entity not in list");
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

static void QC_entity_set_alloc(qcvm_t *vm)
{
	entity_set_t *set = entity_set_alloc((vm->state.argc && qcvm_argv_int32(vm, 0) > 1) ? qcvm_argv_int32(vm, 0) : 0);
	qcvm_return_int32(vm, (int32_t)set);
}

static void QC_entity_set_get(qcvm_t *vm)
{
	const entity_set_t *set = (entity_set_t *)qcvm_argv_int32(vm, 0);
	const int32_t index = qcvm_argv_int32(vm, 1);

	qcvm_return_entity(vm, entity_set_get(set, index));
}

static void QC_entity_set_add(qcvm_t *vm)
{
	entity_set_t *set = (entity_set_t *)qcvm_argv_int32(vm, 0);
	edict_t *ent = qcvm_argv_entity(vm, 1);
	entity_set_add(set, ent);
}

static void QC_entity_set_remove(qcvm_t *vm)
{
	entity_set_t *set = (entity_set_t *)qcvm_argv_int32(vm, 0);
	edict_t *ent = qcvm_argv_entity(vm, 1);
	entity_set_remove(set, ent);
}

static void QC_entity_set_length(qcvm_t *vm)
{
	entity_set_t *set = (entity_set_t *)qcvm_argv_int32(vm, 0);
	qcvm_return_int32(vm, (int32_t)set->count);
}

static void QC_entity_set_clear(qcvm_t *vm)
{
	entity_set_t *set = (entity_set_t *)qcvm_argv_int32(vm, 0);
	entity_set_clear(set);
}

static void QC_entity_set_free(qcvm_t *vm)
{
	entity_set_t *set = (entity_set_t *)qcvm_argv_int32(vm, 0);
	entity_set_free(set);
}

static void QC_BoxEdicts(qcvm_t *vm)
{
	entity_set_t *set = (entity_set_t *)qcvm_argv_int32(vm, 0);
	const vec3_t mins = qcvm_argv_vector(vm, 1);
	const vec3_t maxs = qcvm_argv_vector(vm, 2);
	const int32_t maxcount = qcvm_argv_int32(vm, 3);
	const box_edicts_area_t areatype = qcvm_argv_int32(vm, 4);
	static edict_t *raw_entities[MAX_EDICTS]; // FIXME: too big :(

	const int32_t count = gi.BoxEdicts(&mins, &maxs, raw_entities, min(32, maxcount), areatype);

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
static qcvm_t *pmove_vm;

static content_flags_t QC_pm_pointcontents(const vec3_t *position)
{
	QCFunction *func = qcvm_get_function(pmove_vm, QC_pm_pointcontents_func);
	qcvm_set_global_typed_ptr(vec3_t, pmove_vm, GLOBAL_PARM0, position);
	qcvm_execute(pmove_vm, func);
	return *qcvm_get_global_typed(content_flags_t, pmove_vm, GLOBAL_RETURN);
}

static func_t QC_pm_trace_func;

static trace_t QC_pm_trace(const vec3_t *start, const vec3_t *mins, const vec3_t *maxs, const vec3_t *end)
{
	QC_trace_t qc_tr;

	QCFunction *func = qcvm_get_function(pmove_vm, QC_pm_trace_func);
	qcvm_set_allowed_stack(pmove_vm, &qc_tr, sizeof(qc_tr));
	const int32_t address = (int32_t)&qc_tr;
	qcvm_set_global_typed_value(int32_t, pmove_vm, GLOBAL_PARM0, address);
	qcvm_set_global_typed_ptr(vec3_t, pmove_vm, GLOBAL_PARM1, start);
	qcvm_set_global_typed_ptr(vec3_t, pmove_vm, GLOBAL_PARM2, mins);
	qcvm_set_global_typed_ptr(vec3_t, pmove_vm, GLOBAL_PARM3, maxs);
	qcvm_set_global_typed_ptr(vec3_t, pmove_vm, GLOBAL_PARM4, end);
	qcvm_execute(pmove_vm, func);

	trace_t tr;
	tr.allsolid = (qboolean)qc_tr.allsolid;
	tr.startsolid = (qboolean)qc_tr.startsolid;
	tr.fraction = qc_tr.fraction;
	tr.endpos = qc_tr.endpos;
	tr.plane = qc_tr.plane;
	tr.surface = (csurface_t *)qc_tr.surface;
	tr.contents = (content_flags_t)qc_tr.contents;
	tr.ent = qcvm_ent_to_entity(qc_tr.ent, false);

	return tr;
}

static void QC_Pmove(qcvm_t *vm)
{
	pmove_vm = vm;

	QC_pmove_t *qc_pm = qcvm_get_global_ptr_typed(QC_pmove_t, vm, GLOBAL_PARM0);

	pmove_t pm;

	// set in parameters
	pm.s.pm_type = qc_pm->s.pm_type;

	pm.s.origin[0] = qc_pm->s.origin.x * coord2short;
	pm.s.origin[1] = qc_pm->s.origin.y * coord2short;
	pm.s.origin[2] = qc_pm->s.origin.z * coord2short;
	pm.s.velocity[0] = qc_pm->s.velocity.x * coord2short;
	pm.s.velocity[1] = qc_pm->s.velocity.y * coord2short;
	pm.s.velocity[2] = qc_pm->s.velocity.z * coord2short;
	pm.s.delta_angles[0] = qc_pm->s.delta_angles.x * angle2short;
	pm.s.delta_angles[1] = qc_pm->s.delta_angles.y * angle2short;
	pm.s.delta_angles[2] = qc_pm->s.delta_angles.z * angle2short;
	pm.cmd.angles[0] = qc_pm->cmd.angles.x * angle2short;
	pm.cmd.angles[1] = qc_pm->cmd.angles.y * angle2short;
	pm.cmd.angles[2] = qc_pm->cmd.angles.z * angle2short;

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

	qc_pm->s.origin.x = pm.s.origin[0] * short2coord;
	qc_pm->s.origin.y = pm.s.origin[1] * short2coord;
	qc_pm->s.origin.z = pm.s.origin[2] * short2coord;
	qc_pm->s.velocity.x = pm.s.velocity[0] * short2coord;
	qc_pm->s.velocity.y = pm.s.velocity[1] * short2coord;
	qc_pm->s.velocity.z = pm.s.velocity[2] * short2coord;
	qc_pm->s.delta_angles.x = pm.s.delta_angles[0] * short2angle;
	qc_pm->s.delta_angles.y = pm.s.delta_angles[1] * short2angle;
	qc_pm->s.delta_angles.z = pm.s.delta_angles[2] * short2angle;

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
		qc_pm->groundentity = qcvm_entity_to_ent(pm.groundentity);
	qc_pm->watertype = pm.watertype;
	qc_pm->waterlevel = pm.waterlevel;
	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, qc_pm, sizeof(*qc_pm) / sizeof(global_t), false);
}

static void QC_multicast(qcvm_t *vm)
{
	const vec3_t pos = qcvm_argv_vector(vm, 0);
	const multicast_t type = qcvm_argv_int32(vm, 1);

	gi.multicast(&pos, type);
}

static void QC_unicast(qcvm_t *vm)
{
	edict_t *ent = qcvm_argv_entity(vm, 0);
	const qboolean reliable = (qboolean)qcvm_argv_int32(vm, 1);

	gi.unicast(ent, reliable);
}

static void QC_WriteChar(qcvm_t *vm)
{
	const int32_t val = qcvm_argv_int32(vm, 0);

	gi.WriteChar(val);
}

static void QC_WriteByte(qcvm_t *vm)
{
	const int32_t val = qcvm_argv_int32(vm, 0);

	gi.WriteByte(val);
}

static void QC_WriteShort(qcvm_t *vm)
{
	const int32_t val = qcvm_argv_int32(vm, 0);

	gi.WriteShort(val);
}

static void QC_WriteLong(qcvm_t *vm)
{
	const int32_t val = qcvm_argv_int32(vm, 0);

	gi.WriteLong(val);
}

static void QC_WriteFloat(qcvm_t *vm)
{
	const vec_t val = qcvm_argv_float(vm, 0);

	gi.WriteFloat(val);
}

static void QC_WriteString(qcvm_t *vm)
{
	const char *val = qcvm_argv_string(vm, 0);

	gi.WriteString(val);
}

static void QC_WritePosition(qcvm_t *vm)
{
	const vec3_t val = qcvm_argv_vector(vm, 0);

	gi.WritePosition(&val);
}

static void QC_WriteDir(qcvm_t *vm)
{
	const vec3_t val = qcvm_argv_vector(vm, 0);

	gi.WriteDir(&val);
}

static void QC_WriteAngle(qcvm_t *vm)
{
	const vec_t val = qcvm_argv_float(vm, 0);

	gi.WriteAngle(val);
}

static void QC_argv(qcvm_t *vm)
{
	const int32_t n = qcvm_argv_int32(vm, 0);
	qcvm_return_string(vm, gi.argv(n));
}

static void QC_argc(qcvm_t *vm)
{
	qcvm_return_int32(vm, gi.argc());
}

static void QC_args(qcvm_t *vm)
{
	qcvm_return_string(vm, gi.args());
}

static void QC_AddCommandString(qcvm_t *vm)
{
	gi.AddCommandString(qcvm_argv_string(vm, 0));
}

static void QC_bprintf(qcvm_t *vm)
{
	const print_level_t level = qcvm_argv_int32(vm, 0);
	const string_t fmtid = qcvm_argv_string_id(vm, 1);
	gi.bprintf(level, "%s", ParseFormat(fmtid, vm, 2).data());
}

static void QC_dprintf(qcvm_t *vm)
{
	const string_t fmtid = qcvm_argv_string_id(vm, 0);
	gi.dprintf("%s", ParseFormat(fmtid, vm, 1).data());
}

static void QC_cprintf(qcvm_t *vm)
{
	edict_t *ent = qcvm_argv_entity(vm, 0);
	const print_level_t level = qcvm_argv_int32(vm, 1);
	const string_t fmtid = qcvm_argv_string_id(vm, 2);
	gi.cprintf(ent, level, "%s", ParseFormat(fmtid, vm, 3).data());
}

static void QC_centerprintf(qcvm_t *vm)
{
	edict_t *ent = qcvm_argv_entity(vm, 0);
	const string_t fmtid = qcvm_argv_string_id(vm, 1);
	gi.centerprintf(ent, "%s", ParseFormat(fmtid, vm, 2).data());
}

static void QC_DebugGraph(qcvm_t *vm)
{
	const vec_t a = qcvm_argv_float(vm, 0);
	const int32_t b = qcvm_argv_int32(vm, 1);
	gi.DebugGraph(a, b);
}

void InitGIBuiltins(qcvm_t *vm)
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