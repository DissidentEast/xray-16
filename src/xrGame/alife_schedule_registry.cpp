////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_schedule_registry.сзз
//	Created 	: 15.01.2003
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife schedule registry
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "alife_schedule_registry.h"

CALifeScheduleRegistry::~CALifeScheduleRegistry() {}
void CALifeScheduleRegistry::add(CSE_ALifeDynamicObject* object)
{
    CSE_ALifeSchedulable* schedulable = smart_cast<CSE_ALifeSchedulable*>(object);
    if (!schedulable)
        return;

    if (!schedulable->need_update(object))
        return;

    // the same object legitimately reaches the schedule registry more than once:
    // register_object() adds every object, and unregister_member() re-adds a squad
    // member which just died or left the squad, while the teardown in
    // CALifeObjectRegistry::~CALifeObjectRegistry() calls on_unregister() (which
    // re-adds through groups().unregister_member()) without the paired remove().
    // Inserting twice raises "Specified object has been already found in the
    // registry!", so skip when the entry is already there and replace it only
    // when it points at a different object (a reused object id).
    CSE_ALifeSchedulable* const registered = this->object(object->ID, true);
    if (registered == schedulable)
        return;

    if (registered)
        inherited::remove(object->ID, true);

    inherited::add(object->ID, schedulable);
}

void CALifeScheduleRegistry::remove(CSE_ALifeDynamicObject* object, bool no_assert)
{
    CSE_ALifeSchedulable* schedulable = smart_cast<CSE_ALifeSchedulable*>(object);
    if (!schedulable)
        return;

    inherited::remove(object->ID, no_assert || !schedulable->need_update(object));
}
