#include "shared/shared.h"
#include "g_vm.h"
#include "vm_gi.h"

#include "game.h"
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
	const cvar_t *cvar = qcvm_argv_handle(cvar_t, vm, 0);
	qcvm_return_string(vm, cvar->name);
}

static void QC_cvar_get_string(qcvm_t *vm)
{
	const cvar_t *cvar = qcvm_argv_handle(cvar_t, vm, 0);
	qcvm_return_string(vm, cvar->string);
}

static void QC_cvar_get_latched_string(qcvm_t *vm)
{
	const cvar_t *cvar = qcvm_argv_handle(cvar_t, vm, 0);
	qcvm_return_string(vm, cvar->latched_string);
}

static void QC_cvar_get_modified(qcvm_t *vm)
{
	const cvar_t *cvar = qcvm_argv_handle(cvar_t, vm, 0);
	qcvm_return_int32(vm, cvar->modified);
}
static void QC_cvar_get_flags(qcvm_t *vm)
{
	const cvar_t *cvar = qcvm_argv_handle(cvar_t, vm, 0);
	qcvm_return_int32(vm, cvar->flags);
}

static void QC_cvar_set_modified(qcvm_t *vm)
{
	cvar_t *cvar = qcvm_argv_handle(cvar_t, vm, 0);
	const qboolean value = (qboolean)(qcvm_argv_int32(vm, 1));

	cvar->modified = value;
}

static void QC_cvar_get_floatVal(qcvm_t *vm)
{
	const cvar_t *cvar = qcvm_argv_handle(cvar_t, vm, 0);
	qcvm_return_float(vm, cvar->value);
}

static void QC_cvar_get_intVal(qcvm_t *vm)
{
	const cvar_t *cvar = qcvm_argv_handle(cvar_t, vm, 0);
	qcvm_return_int32(vm, (int32_t)cvar->value);
}

static void QC_cvar(qcvm_t *vm)
{
	const char *name = qcvm_argv_string(vm, 0);
	const char *value = qcvm_argv_string(vm, 1);
	const cvar_flags_t flags = qcvm_argv_int32(vm, 2);

	cvar_t *cvar = gi.cvar(name, value, flags);
	int32_t handle = qcvm_handle_alloc(vm, cvar, NULL);
	qcvm_return_int32(vm, handle);
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
	const qcvm_string_t fmtid = qcvm_argv_string_id(vm, 0);
	qcvm_error(vm, qcvm_parse_format(fmtid, vm, 1));
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

typedef struct
{
	qcvm_string_t	name;
	surface_flags_t	flags;
	int				value;
} QC_csurface_t;

typedef struct
{
	int				allsolid;
	int				startsolid;
	float			fraction;
	vec3_t			endpos;
	vec3_t			normal;
	QC_csurface_t	surface;
	int				contents;
	qcvm_ent_t		ent;
} QC_trace_t;

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
	trace->normal = trace_result.plane.normal;
	trace->surface.flags = trace_result.surface->flags;
	trace->surface.value = trace_result.surface->value;
	trace->surface.name = qcvm_store_or_find_string(vm, trace_result.surface->name, strlen(trace_result.surface->name), true);

	if (qcvm_string_list_is_ref_counted(&vm->dynamic_strings, trace->surface.name))
		qcvm_string_list_mark_ref_copy(&vm->dynamic_strings, trace->surface.name, &trace->surface.name);

	trace->contents = trace_result.contents;
	trace->ent = qcvm_entity_to_ent(vm, trace_result.ent);
	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, trace, sizeof(*trace) / sizeof(qcvm_global_t), false);
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
	const qboolean state = (qboolean)(qcvm_argv_int32(vm, 1));

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

static void entity_set_free(qcvm_t *vm, void *ptr)
{
	entity_set_t *set = (entity_set_t *)ptr;

	for (entity_set_link_t *link = set->head; link; )
	{
		entity_set_link_t *next = link->next;
		gi.TagFree(link);
		link = next;
	}

	gi.TagFree(set);
}

static qcvm_handle_descriptor_t entity_set_descriptor =
{
	.free = entity_set_free
};

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

		(*link)->entity = NULL;

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
		link->entity = NULL;
}

static void QC_entity_set_alloc(qcvm_t *vm)
{
	entity_set_t *set = entity_set_alloc((vm->state.argc && qcvm_argv_int32(vm, 0) > 1) ? (size_t)qcvm_argv_int32(vm, 0) : 0);
	int32_t handle = qcvm_handle_alloc(vm, set, &entity_set_descriptor);
	qcvm_return_int32(vm, handle);
}

static void QC_entity_set_get(qcvm_t *vm)
{
	const entity_set_t *set = qcvm_argv_handle(entity_set_t, vm, 0);
	const int32_t index = qcvm_argv_int32(vm, 1);

	qcvm_return_entity(vm, entity_set_get(set, index));
}

static void QC_entity_set_add(qcvm_t *vm)
{
	entity_set_t *set = qcvm_argv_handle(entity_set_t, vm, 0);
	edict_t *ent = qcvm_argv_entity(vm, 1);
	entity_set_add(set, ent);
}

static void QC_entity_set_remove(qcvm_t *vm)
{
	entity_set_t *set = qcvm_argv_handle(entity_set_t, vm, 0);
	edict_t *ent = qcvm_argv_entity(vm, 1);
	entity_set_remove(set, ent);
}

static void QC_entity_set_length(qcvm_t *vm)
{
	entity_set_t *set = qcvm_argv_handle(entity_set_t, vm, 0);
	qcvm_return_int32(vm, (int32_t)set->count);
}

static void QC_entity_set_clear(qcvm_t *vm)
{
	entity_set_t *set = qcvm_argv_handle(entity_set_t, vm, 0);
	entity_set_clear(set);
}

static void QC_BoxEdicts(qcvm_t *vm)
{
	entity_set_t *set = qcvm_argv_handle(entity_set_t, vm, 0);
	const vec3_t mins = qcvm_argv_vector(vm, 1);
	const vec3_t maxs = qcvm_argv_vector(vm, 2);
	const int32_t maxcount = qcvm_argv_int32(vm, 3);
	const box_edicts_area_t areatype = qcvm_argv_int32(vm, 4);
	edict_t *raw_entities[maxcount];

	const int32_t count = gi.BoxEdicts(&mins, &maxs, raw_entities, maxcount, areatype);

	entity_set_clear(set);

	for (int32_t i = 0; i < count; i++)
		entity_set_add(set, raw_entities[i]);
}

typedef struct
{
    pmtype_t pm_type;

    vec3_t origin;		// 12.3
    vec3_t velocity;	// 12.3
    int	pm_flags;		// ducked, jump_held, etc
    int	pm_time;		// each unit = 8 ms
    int	gravity;
    vec3_t delta_angles;	// add to command angles to get view direction
							// changed by spawns, rotating objects, and teleporters
} QC_pmove_state_t;

typedef qcvm_handle_id_t QC_entity_set_t;

typedef struct
{
	// inout
	QC_pmove_state_t	s;
	
	// in
	QC_usercmd_t	cmd;
	bool			snapinitial;
	
	// out
	QC_entity_set_t	touchents;
	
	vec3_t	viewangles;
	float	viewheight;
	
	vec3_t	mins, maxs;
	
	qcvm_ent_t	groundentity;
	int		watertype;
	int		waterlevel;
	
	// in (callbacks)
	qcvm_func_t trace;
	qcvm_func_t pointcontents;
} QC_pmove_t;

static entity_set_t touchents_memory;
static int32_t touchents_handle = 0;

static qcvm_func_t QC_pm_pointcontents_func;
static qcvm_t *pmove_vm;

static content_flags_t QC_pm_pointcontents(const vec3_t *position)
{
	qcvm_function_t *func = qcvm_get_function(pmove_vm, QC_pm_pointcontents_func);
	qcvm_set_global_typed_ptr(vec3_t, pmove_vm, GLOBAL_PARM0, position);
	qcvm_execute(pmove_vm, func);
	return *qcvm_get_global_typed(content_flags_t, pmove_vm, GLOBAL_RETURN);
}

static qcvm_func_t QC_pm_trace_func;

static trace_t QC_pm_trace(const vec3_t *start, const vec3_t *mins, const vec3_t *maxs, const vec3_t *end)
{
	QC_trace_t qc_tr;

	qcvm_function_t *func = qcvm_get_function(pmove_vm, QC_pm_trace_func);
	qcvm_set_allowed_stack(pmove_vm, &qc_tr, sizeof(qc_tr));
	const qcvm_pointer_t pointer = qcvm_make_pointer(pmove_vm, QCVM_POINTER_STACK, &qc_tr);
	qcvm_set_global_typed_value(qcvm_pointer_t, pmove_vm, GLOBAL_PARM0, pointer);
	qcvm_set_global_typed_ptr(vec3_t, pmove_vm, GLOBAL_PARM1, start);
	qcvm_set_global_typed_ptr(vec3_t, pmove_vm, GLOBAL_PARM2, mins);
	qcvm_set_global_typed_ptr(vec3_t, pmove_vm, GLOBAL_PARM3, maxs);
	qcvm_set_global_typed_ptr(vec3_t, pmove_vm, GLOBAL_PARM4, end);
	qcvm_execute(pmove_vm, func);

	static csurface_t qc_surface;
	trace_t tr;
	tr.allsolid = (qboolean)qc_tr.allsolid;
	tr.startsolid = (qboolean)qc_tr.startsolid;
	tr.fraction = qc_tr.fraction;
	tr.endpos = qc_tr.endpos;
	tr.plane = (cplane_t) { qc_tr.normal, 0, 0, 0, { 0, 0 } };
	qc_surface.flags = qc_tr.surface.flags;
	qc_surface.value = qc_tr.surface.value;

	const char *str = qcvm_get_string(pmove_vm, qc_tr.surface.name);

	if (str)
		Q_strlcpy(qc_surface.name, str, sizeof(qc_surface.name));
	else
		qc_surface.name[0] = 0;

	tr.surface = &qc_surface;
	tr.contents = (content_flags_t)qc_tr.contents;
	tr.ent = qcvm_ent_to_entity(pmove_vm, qc_tr.ent, false);

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

	if (!touchents_handle)
		touchents_handle = qcvm_handle_alloc(vm, &touchents_memory, &entity_set_descriptor);

	qc_pm->touchents = touchents_handle;
	entity_set_clear(&touchents_memory);

	for (int32_t i = 0; i < pm.numtouch; i++)
		entity_set_add(&touchents_memory, pm.touchents[i]);

	qc_pm->viewangles = pm.viewangles;
	qc_pm->viewheight = pm.viewheight;
	
	qc_pm->mins = pm.mins;
	qc_pm->maxs = pm.maxs;

	qc_pm->groundentity = qcvm_entity_to_ent(vm, pm.groundentity);
	qc_pm->watertype = pm.watertype;
	qc_pm->waterlevel = pm.waterlevel;
	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, qc_pm, sizeof(*qc_pm) / sizeof(qcvm_global_t), false);
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
	const qboolean reliable = (qboolean)(qcvm_argv_int32(vm, 1));

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
	const qcvm_string_t fmtid = qcvm_argv_string_id(vm, 1);
	gi.bprintf(level, "%s", qcvm_parse_format(fmtid, vm, 2));
}

static void QC_dprintf(qcvm_t *vm)
{
	const qcvm_string_t fmtid = qcvm_argv_string_id(vm, 0);
	gi.dprintf("%s", qcvm_parse_format(fmtid, vm, 1));
}

static void QC_cprintf(qcvm_t *vm)
{
	edict_t *ent = qcvm_argv_entity(vm, 0);
	const print_level_t level = qcvm_argv_int32(vm, 1);
	const qcvm_string_t fmtid = qcvm_argv_string_id(vm, 2);
	gi.cprintf(ent, level, "%s", qcvm_parse_format(fmtid, vm, 3));
}

static void QC_centerprintf(qcvm_t *vm)
{
	edict_t *ent = qcvm_argv_entity(vm, 0);
	const qcvm_string_t fmtid = qcvm_argv_string_id(vm, 1);
	gi.centerprintf(ent, "%s", qcvm_parse_format(fmtid, vm, 2));
}

static void QC_DebugGraph(qcvm_t *vm)
{
	const vec_t a = qcvm_argv_float(vm, 0);
	const int32_t b = qcvm_argv_int32(vm, 1);
	gi.DebugGraph(a, b);
}

void qcvm_init_gi_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(bprintf);
	qcvm_register_builtin(dprintf);
	qcvm_register_builtin(cprintf);
	qcvm_register_builtin(centerprintf);
	qcvm_register_builtin(sound);
	qcvm_register_builtin(positioned_sound);
	qcvm_register_builtin(cvar);
	qcvm_register_builtin(cvar_set);
	qcvm_register_builtin(cvar_forceset);

	qcvm_register_builtin(configstring);
	
	qcvm_register_builtin(error);

	qcvm_register_builtin(modelindex);
	qcvm_register_builtin(soundindex);
	qcvm_register_builtin(imageindex);

	qcvm_register_builtin(setmodel);
	
	qcvm_register_builtin(trace);
	qcvm_register_builtin(pointcontents);
	qcvm_register_builtin(inPVS);
	qcvm_register_builtin(inPHS);
	qcvm_register_builtin(SetAreaPortalState);
	qcvm_register_builtin(AreasConnected);
	
	qcvm_register_builtin(linkentity);
	qcvm_register_builtin(unlinkentity);
	qcvm_register_builtin(BoxEdicts);
	qcvm_register_builtin(Pmove);
	
	qcvm_register_builtin(multicast);
	qcvm_register_builtin(unicast);
	qcvm_register_builtin(WriteChar);
	qcvm_register_builtin(WriteByte);
	qcvm_register_builtin(WriteShort);
	qcvm_register_builtin(WriteLong);
	qcvm_register_builtin(WriteFloat);
	qcvm_register_builtin(WriteString);
	qcvm_register_builtin(WritePosition);
	qcvm_register_builtin(WriteDir);
	qcvm_register_builtin(WriteAngle);
	
	qcvm_register_builtin(argv);
	qcvm_register_builtin(argc);
	qcvm_register_builtin(args);

	qcvm_register_builtin(AddCommandString);

	qcvm_register_builtin(DebugGraph);
	
	qcvm_register_builtin(entity_set_alloc);
	qcvm_register_builtin(entity_set_get);
	qcvm_register_builtin(entity_set_add);
	qcvm_register_builtin(entity_set_remove);
	qcvm_register_builtin(entity_set_length);
	qcvm_register_builtin(entity_set_clear);
	
	qcvm_register_builtin(cvar_get_name);
	qcvm_register_builtin(cvar_get_string);
	qcvm_register_builtin(cvar_get_latched_string);
	qcvm_register_builtin(cvar_get_modified);
	qcvm_register_builtin(cvar_set_modified);
	qcvm_register_builtin(cvar_get_flags);
	qcvm_register_builtin(cvar_get_floatVal);
	qcvm_register_builtin(cvar_get_intVal);
}