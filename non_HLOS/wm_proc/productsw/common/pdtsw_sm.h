/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#ifndef PDTSW_SM_H
#define PDTSW_SM_H

#include<stdint.h>
#include<stdbool.h>

/**
 * state type
 */
typedef uint32_t pdtsw_sm_state_t;

/**
 * event type
 */
typedef uint32_t pdtsw_sm_event_t;

/**
 * This structure defines state transition element.
 */
typedef struct
{
    /* Event received.*/
    pdtsw_sm_event_t event;

    /* Function to execute transition.
     * Transition will happen only if the function returns true.*/
    bool (*pConditionFn)(void* data);

    /* State to propagate to based on the event.*/
    pdtsw_sm_state_t next_state;
} pdtsw_sm_state_transition_t;

/**
 * This structure defines state transition table for a state.
 */
typedef struct
{
    /* transition list for that particular state*/
    const pdtsw_sm_state_transition_t* plist;

    /* Size of the list*/
    uint32_t size;
} pdtsw_sm_state_transition_table_t;

/**
 * event handled or not
 */
typedef enum {
    PDTSW_SM_EVENT_HANDLED,
    PDTSW_SM_EVENT_NOT_HANDLED,
}pdtsw_sm_state_handler_result_t;

/**
 * state handler prototype
 */
typedef pdtsw_sm_state_handler_result_t (*EventHandlerFn) (void* msg);

/**
 * This structure maintains list of entry and exit routines for each state
 */
typedef struct
{
    /* Function to execute on state entry.*/
    void (*pStateEntryFn) (void* msg);

    /* Function to execute on state exit.*/
    void (*pStateExitFn) (void* msg);

    /* Parent state of this state.*/
    pdtsw_sm_state_t pParent;

    /* State handler of the messages.*/
    EventHandlerFn pEventHandlerFn;

    /* Transition table for the current state.*/
    pdtsw_sm_state_transition_table_t transition_table;

    bool *p_skip_transition;
}pdtsw_sm_state_routines_t;

/**
 * Structure to define state map
 */
typedef struct
{
    /* All states for a session.*/
    const pdtsw_sm_state_routines_t* pStateMap;

    /* number of states.*/
    uint32_t map_size;

    /* Current state.*/
    pdtsw_sm_state_t curr_state;
}pdtsw_sm_state_map_t;

/**
 * Every state should have a parent state except the root state.
 * a root state is a mandatory state and should be at 0th index of as_state_map_t.
 */
#define PDTSW_SM_ROOT_STATE_INDEX        0

/**
 * Set audio service state. and updates the current state
 * @param[in]       state_map state table.
 * @param[in]       new_state   next state.
 * @param[in]       msg message to be handled.
 */
void pdtsw_sm_set_state (pdtsw_sm_state_map_t* state_map, pdtsw_sm_state_t new_state, void* msg);

/**
 * Perform state transition
 * @param[in] state_map state table.
 * @param[in] state state routine to use for this event.
 * @param[in] evt event to perform transition on.
 * @param[in] msg message to be handled.
 */
void pdtsw_sm_do_transition(pdtsw_sm_state_map_t* state_map, pdtsw_sm_state_t state, 
    pdtsw_sm_event_t evt, void* msg);

/**
 * Do state based event handling.
 * @param[in] state_map state table.
 * @param[in] evt event to handle.
 * @param[in] msg message to be handled.
 */
void pdtsw_sm_handle_event(pdtsw_sm_state_map_t* state_map, pdtsw_sm_event_t evt, void* msg);

/**
 * Skip state transition for the current event being processed.
 * This shall be used by state dependent handlers to skip the transition
 * if the pre-conditions are not met.
 * @param[in] state_map state table of the audio service.
 */
void pdtsw_sm_skip_state_transition(pdtsw_sm_state_map_t* state_map);

#endif /* PDTSW_SM_H */
