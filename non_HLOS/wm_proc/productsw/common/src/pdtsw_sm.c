/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include "pdtsw_sm.h"
#include<stdlib.h>

/**
 * Find a common parent state between two states.
 */
static pdtsw_sm_state_t find_common_parent(pdtsw_sm_state_map_t* state_map,
    pdtsw_sm_state_t new_state)
{
    pdtsw_sm_state_t common_parent, temp_state;
    uint8_t break_outer_loop = false;

    /*Find common parent of two states.*/
    for(common_parent = state_map->pStateMap[state_map->curr_state].pParent;
            common_parent != PDTSW_SM_ROOT_STATE_INDEX;
            common_parent = state_map->pStateMap[common_parent].pParent)
    {

        for(temp_state = state_map->pStateMap[new_state].pParent;
                temp_state != PDTSW_SM_ROOT_STATE_INDEX;
                temp_state = state_map->pStateMap[temp_state].pParent)
        {

            /*break loop if we reached root state or a common parent state*/
            if(common_parent == temp_state)
            {
                break_outer_loop = true;
                break;
            }
        }
        if(break_outer_loop)
        {
            break;
        }
    }

    return common_parent;
}

/**
 * Execute exit functions from the current state till the provided parent state.
 */
static void execute_state_exit_sequence(pdtsw_sm_state_map_t* state_map,
        pdtsw_sm_state_t parent_state, void* msg)
{
    pdtsw_sm_state_t curr_state = state_map->curr_state;

    /*Exit from current state till the common parent state*/
    for(; curr_state != parent_state;
            curr_state = state_map->pStateMap[curr_state].pParent)
    {
        if(state_map->pStateMap[curr_state].pStateExitFn != NULL)
        {
            state_map->pStateMap[curr_state].pStateExitFn(msg);
        }
    }
}

/**
 * Execute all entry functions from parent state till the target state.
 */
static void execute_state_entry_sequence(pdtsw_sm_state_map_t* state_map,
        pdtsw_sm_state_t target_state, pdtsw_sm_state_t parent_state, void* msg)
{
    if(target_state != parent_state)
    {
        /*Recurse till the parent state */

        execute_state_entry_sequence(state_map,
                state_map->pStateMap[target_state].pParent, parent_state, msg);

        /*Execute the entry function*/
        if(state_map->pStateMap[target_state].pStateEntryFn != NULL)
        {
            state_map->pStateMap[target_state].pStateEntryFn(msg);
        }
    }
    else
    {
        /* Reached the common state, stop recursing so that the
         * entry functions will be executed*/
    }
}

void pdtsw_sm_set_state(pdtsw_sm_state_map_t* state_map, pdtsw_sm_state_t new_state,
        void* msg)
{
    /* Perform input sanity.*/
    if(state_map == NULL)
    {
        /** Todo: Fatal */
        while(1);
    }
    else
    {
        if((state_map->curr_state >= state_map->map_size) ||
            (new_state >= state_map->map_size) ||
            (new_state == state_map->curr_state))
        {
            /** Todo: Fatal */
            while(1);
        }

        pdtsw_sm_state_t common_parent;

        /* Find common parent of two states.*/
        common_parent = find_common_parent(state_map, new_state);

        /* Execute exit functions*/
        execute_state_exit_sequence(state_map, common_parent, msg);

        /* Execute entry functions*/
        execute_state_entry_sequence(state_map, new_state, common_parent, msg);

        state_map->curr_state = new_state;
    }
}

void pdtsw_sm_do_transition(pdtsw_sm_state_map_t* state_map, pdtsw_sm_state_t state, 
    pdtsw_sm_event_t evt, void* msg)
{
    const pdtsw_sm_state_routines_t* _pStateMap = &(state_map->pStateMap[state]);

    if(false == *(_pStateMap->p_skip_transition))
    {
        const pdtsw_sm_state_transition_t *list = _pStateMap->transition_table.plist;

        if(NULL != list)
        {
            uint32_t size = _pStateMap->transition_table.size;

            /*
             * Traverse the current state transition list.
             */
            while (size > 0)
            {
                if (list->event == evt)
                {
                    /* Execute the conditional function for the transition.*/
                    if(true == list->pConditionFn(msg))
                    {
                        /* Set the new state based on the event*/
                        pdtsw_sm_set_state (state_map, list->next_state, msg);
                    }
                    else
                    {
                        /* Don't do transition*/
                    }
                    break;
                }
                ++list;
                --size;
            }
        }
    }

    /* Don't skip the next transition unless specified explicitly again*/
    *(state_map->pStateMap[state].p_skip_transition) = false;
}

void pdtsw_sm_handle_event(pdtsw_sm_state_map_t* state_map, pdtsw_sm_event_t evt, void* msg)
{
    pdtsw_sm_state_t _state;
    pdtsw_sm_state_handler_result_t status = PDTSW_SM_EVENT_NOT_HANDLED;

    /* Iterate from current state to top till the event is handled.*/
    for(_state = state_map->curr_state;
            (PDTSW_SM_ROOT_STATE_INDEX != _state) && (PDTSW_SM_EVENT_NOT_HANDLED == status);
            _state = state_map->pStateMap[_state].pParent)
    {
        /* Execute the state handler and see if it is able to process the message.*/
        if(NULL != state_map->pStateMap[_state].pEventHandlerFn)
        {
            status = state_map->pStateMap[_state].pEventHandlerFn(msg);

            pdtsw_sm_do_transition(state_map, _state, evt, msg);
        }
    }
}

void pdtsw_sm_skip_state_transition(pdtsw_sm_state_map_t* state_map)
{
    *(state_map->pStateMap[state_map->curr_state].p_skip_transition) = true;
}
