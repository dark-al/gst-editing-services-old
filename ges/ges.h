/* GStreamer Editing Services
 * Copyright (C) 2009 Edward Hervey <bilboed@bilboed.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GES_H__
#define __GES_H__

#include <glib.h>
#include <gst/gst.h>

#include <ges/ges-types.h>

#include <ges/ges-timeline.h>
#include <ges/ges-timeline-layer.h>
#include <ges/ges-simple-timeline-layer.h>
#include <ges/ges-timeline-object.h>
#include <ges/ges-timeline-pipeline.h>
#include <ges/ges-timeline-source.h>
#include <ges/ges-timeline-transition.h>
#include <ges/ges-track.h>
#include <ges/ges-track-object.h>
#include <ges/ges-track-source.h>

G_BEGIN_DECLS

void ges_init (void);

G_END_DECLS

#endif /* __GES_H__ */