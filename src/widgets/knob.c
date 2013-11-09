/**
 * rutabaga: an OpenGL widget toolkit
 * Copyright (c) 2013 William Light.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "rutabaga/rutabaga.h"
#include "rutabaga/element.h"
#include "rutabaga/window.h"
#include "rutabaga/render.h"
#include "rutabaga/event.h"
#include "rutabaga/keyboard.h"

#include "rutabaga/widgets/knob.h"

#include "private/util.h"

#define SELF_FROM(elem) \
	struct rtb_knob *self = RTB_ELEMENT_AS(elem, rtb_knob)

#define MIN_DEGREES 35.f
#define MAX_DEGREES (360.f - MIN_DEGREES)
#define DEGREE_RANGE (MAX_DEGREES - MIN_DEGREES)

/* would be cool to have this be like a smoothed equation or smth */
#define DELTA_VALUE_STEP_BUTTON1 .005f
#define DELTA_VALUE_STEP_BUTTON3 .0005f

struct rtb_element_implementation super;

/**
 * drawing
 */

static void
draw(struct rtb_element *elem)
{
	SELF_FROM(elem);

	rtb_stylequad_draw_with_modelview(&self->stylequad, elem,
			&self->modelview);
	rtb_elem_draw_children(RTB_ELEMENT(self));
}

/**
 * event dispatching
 */

static int
dispatch_value_change_event(struct rtb_knob *self)
{
	struct rtb_knob_event event = {
		.type = KNOB_VALUE_CHANGE,
		.value =
			(self->value * (self->max - self->min)) + self->min,
	};

	return rtb_handle(RTB_ELEMENT(self), RTB_EVENT(&event));
}

static void
set_value_internal(struct rtb_knob *self, float new_value)
{
	self->value = fmin(fmax(new_value, 0.f), 1.f);

	mat4_set_rotation(&self->modelview,
			MIN_DEGREES + (self->value * DEGREE_RANGE),
			0.f, 0.f, 1.f);

	rtb_elem_mark_dirty(RTB_ELEMENT(self));

	if (self->state != RTB_STATE_UNATTACHED)
		dispatch_value_change_event(self);
}

/**
 * event handling
 */

static int
handle_drag(struct rtb_knob *self, const struct rtb_drag_event *e)
{
	float new_value;

	if (e->target != RTB_ELEMENT(self))
		return 0;

	new_value = self->value;

	switch (e->button) {
	case RTB_MOUSE_BUTTON1: new_value += -e->delta.y * DELTA_VALUE_STEP_BUTTON1; break;
	case RTB_MOUSE_BUTTON3: new_value += -e->delta.y * DELTA_VALUE_STEP_BUTTON3; break;
	default: return 1;
	}

	set_value_internal(self, new_value);
	return 1;
}

static int
handle_mouse_down(struct rtb_knob *self, const struct rtb_mouse_event *e)
{
	switch (e->button) {
	case RTB_MOUSE_BUTTON2:
		rtb_knob_set_value(self, self->origin);
		dispatch_value_change_event(self);
		return 1;

	case RTB_MOUSE_BUTTON1:
		return 1;

	default:
		return 0;
	}
}

static int
handle_key(struct rtb_knob *self, const struct rtb_key_event *e)
{
	float step;

	if (e->modkeys & RTB_KEY_MOD_ALT)
		step = DELTA_VALUE_STEP_BUTTON3;
	else if (e->modkeys & RTB_KEY_MOD_SHIFT)
		step = DELTA_VALUE_STEP_BUTTON1 * 2;
	else
		step = DELTA_VALUE_STEP_BUTTON1;

	switch (e->keysym) {
	case RTB_KEY_UP:
	case RTB_KEY_NUMPAD_UP:
		rtb_knob_set_value(self, self->value + step);
		return 1;

	case RTB_KEY_DOWN:
	case RTB_KEY_NUMPAD_DOWN:
		rtb_knob_set_value(self, self->value - step);
		return 1;

	default:
		return 0;
	}
}

static int
on_event(struct rtb_element *elem, const struct rtb_event *e)
{
	SELF_FROM(elem);

	switch (e->type) {
	case RTB_DRAG_START:
	case RTB_DRAGGING:
		return handle_drag(self, RTB_EVENT_AS(e, rtb_drag_event));

	case RTB_MOUSE_DOWN:
		return handle_mouse_down(self, RTB_EVENT_AS(e, rtb_mouse_event));

	case RTB_KEY_PRESS:
		return handle_key(self, RTB_EVENT_AS(e, rtb_key_event));

	default:
		return super.on_event(elem, e);
	}

	return 0;
}

static void
attached(struct rtb_element *elem,
		struct rtb_element *parent, struct rtb_window *window)
{
	SELF_FROM(elem);

	super.attached(elem, parent, window);
	self->type = rtb_type_ref(window, self->type,
			"net.illest.rutabaga.widgets.knob");

	rtb_knob_set_value(self, self->origin);
}

/**
 * public API
 */

void
rtb_knob_set_value(struct rtb_knob *self, float new_value)
{
	float range = self->max - self->min;
	set_value_internal(self, (new_value - self->min) / range);
}

int
rtb_knob_init(struct rtb_knob *self)
{
	if (RTB_SUBCLASS(RTB_ELEMENT(self), rtb_elem_init, &super))
		return -1;

	self->origin = 0.f;
	self->min = 0.f;
	self->max = 1.f;

	self->w = 30;
	self->h = 30;

	self->draw     = draw;
	self->on_event = on_event;
	self->attached = attached;

	return 0;
}

void
rtb_knob_fini(struct rtb_knob *self)
{
	rtb_elem_fini(RTB_ELEMENT(self));
}

struct rtb_knob *
rtb_knob_new()
{
	struct rtb_knob *self = calloc(1, sizeof(struct rtb_knob));
	rtb_knob_init(self);
	return self;
}

void
rtb_knob_free(struct rtb_knob *self)
{
	rtb_knob_fini(self);
	free(self);
}
