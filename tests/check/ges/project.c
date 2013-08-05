/* GStreamer Editing Services
 *
 * Copyright (C) <2012> Thibault Saunier <thibault.saunier@collabora.com>
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

#include "test-utils.h"
#include <ges/ges.h>
#include <gst/check/gstcheck.h>
#include <gst/controller/gstdirectcontrolbinding.h>
#include <gst/controller/gstinterpolationcontrolsource.h>

static void
project_loaded_cb (GESProject * project, GESTimeline * timeline,
    GMainLoop * mainloop)
{
  g_main_loop_quit (mainloop);
}

static gchar *
get_tmp_uri (const gchar * filename)
{
  gchar *location, *uri;

  location = g_build_filename (g_get_tmp_dir (),
      "test-keyframes-save.xges", NULL);

  uri = g_strconcat ("file://", location, NULL);
  g_free (location);

  return uri;
}

GST_START_TEST (test_project_simple)
{
  gchar *id;
  GESProject *project;
  GESTimeline *timeline;
  GMainLoop *mainloop;

  ges_init ();

  mainloop = g_main_loop_new (NULL, FALSE);
  project = GES_PROJECT (ges_asset_request (GES_TYPE_TIMELINE, NULL, NULL));
  fail_unless (GES_IS_PROJECT (project));
  assert_equals_string (ges_asset_get_id (GES_ASSET (project)), "project-0");
  g_signal_connect (project, "loaded", (GCallback) project_loaded_cb, mainloop);

  timeline = GES_TIMELINE (ges_asset_extract (GES_ASSET (project), NULL));
  g_main_loop_run (mainloop);

  fail_unless (GES_IS_TIMELINE (timeline));
  id = ges_extractable_get_id (GES_EXTRACTABLE (timeline));
  assert_equals_string (id, "project-0");
  ASSERT_OBJECT_REFCOUNT (timeline, "We own the only ref", 1);

  g_free (id);
  gst_object_unref (project);
  gst_object_unref (timeline);
  g_main_loop_unref (mainloop);
  g_signal_handlers_disconnect_by_func (project, (GCallback) project_loaded_cb,
      mainloop);
}

GST_END_TEST;

static void
asset_removed_add_cb (GESProject * project, GESAsset * asset, gboolean * called)
{
  *called = TRUE;
}

GST_START_TEST (test_project_add_assets)
{
  GESProject *project;
  GESAsset *asset;
  gboolean added_cb_called = FALSE;
  gboolean removed_cb_called = FALSE;

  ges_init ();

  project = GES_PROJECT (ges_asset_request (GES_TYPE_TIMELINE, NULL, NULL));
  fail_unless (GES_IS_PROJECT (project));
  fail_unless (GES_IS_PROJECT (project));

  g_signal_connect (project, "asset-added",
      (GCallback) asset_removed_add_cb, &added_cb_called);
  g_signal_connect (project, "asset-removed",
      (GCallback) asset_removed_add_cb, &removed_cb_called);

  asset = ges_asset_request (GES_TYPE_TEST_CLIP, NULL, NULL);
  fail_unless (GES_IS_ASSET (asset));

  fail_unless (ges_project_add_asset (project, asset));
  fail_unless (added_cb_called);
  ASSERT_OBJECT_REFCOUNT (project, "The project", 2);
  ASSERT_OBJECT_REFCOUNT (asset, "The asset (1 for project and one for "
      "us + 1 cache)", 3);

  fail_unless (ges_project_remove_asset (project, asset));
  fail_unless (removed_cb_called);
  gst_object_unref (asset);
  gst_object_unref (project);

  g_signal_handlers_disconnect_by_func (project,
      (GCallback) asset_removed_add_cb, &added_cb_called);
  g_signal_handlers_disconnect_by_func (project,
      (GCallback) asset_removed_add_cb, &removed_cb_called);

  ASSERT_OBJECT_REFCOUNT (asset, "The asset (1 ref in cache)", 1);
  ASSERT_OBJECT_REFCOUNT (project, "The project (1 ref in cache)", 1);

}

GST_END_TEST;

static void
error_loading_asset_cb (GESProject * project, GError * error, gchar * id,
    GType extractable_type, GMainLoop * mainloop)
{
  fail_unless (g_error_matches (error, GST_PARSE_ERROR,
          GST_PARSE_ERROR_NO_SUCH_ELEMENT));
  g_main_loop_quit (mainloop);
}

GST_START_TEST (test_project_unexistant_effect)
{
  GESProject *project;
  GMainLoop *mainloop;
  gboolean added_cb_called = FALSE;
  gboolean removed_cb_called = FALSE;

  ges_init ();

  project = GES_PROJECT (ges_asset_request (GES_TYPE_TIMELINE, NULL, NULL));
  fail_unless (GES_IS_PROJECT (project));
  fail_unless (GES_IS_PROJECT (project));

  mainloop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (project, "asset-added",
      (GCallback) asset_removed_add_cb, &added_cb_called);
  g_signal_connect (project, "asset-removed",
      (GCallback) asset_removed_add_cb, &removed_cb_called);
  g_signal_connect (project, "error-loading-asset",
      (GCallback) error_loading_asset_cb, mainloop);

  fail_unless (ges_project_create_asset (project, "nowaythiselementexists",
          GES_TYPE_EFFECT));
  g_main_loop_run (mainloop);

  /* And.... try again! */
  fail_if (ges_project_create_asset (project, "nowaythiselementexists",
          GES_TYPE_EFFECT));

  fail_if (added_cb_called);
  fail_if (removed_cb_called);

  ASSERT_OBJECT_REFCOUNT (project, "The project", 2);
  gst_object_unref (project);
  g_main_loop_unref (mainloop);

  ASSERT_OBJECT_REFCOUNT (project, "The project (1 ref in cache)", 1);

}

GST_END_TEST;

static void
asset_added_cb (GESProject * project, GESAsset * asset)
{
  gchar *uri = ges_test_file_uri ("audio_video.ogg");
  GstDiscovererInfo *info;

  if (ges_asset_get_extractable_type (asset) == GES_TYPE_EFFECT) {
    assert_equals_string (ges_asset_get_id (asset), "agingtv");
  } else {
    info = ges_uri_clip_asset_get_info (GES_URI_CLIP_ASSET (asset));
    fail_unless (GST_IS_DISCOVERER_INFO (info));
    assert_equals_string (ges_asset_get_id (asset), uri);
  }

  g_free (uri);
}

static gchar *
_set_new_uri (GESProject * project, GError * error, GESAsset * wrong_asset)
{
  fail_unless (!g_strcmp0 (ges_asset_get_id (wrong_asset),
          "file:///test/not/exisiting"));

  return ges_test_file_uri ("audio_video.ogg");
}

static void
_test_project (GESProject * project, GESTimeline * timeline)
{
  guint a_meta;
  gchar *media_uri;
  GESTrack *track;
  const GList *profiles;
  GstEncodingContainerProfile *profile;
  GList *tracks, *tmp, *tmptrackelement, *clips;

  fail_unless (GES_IS_TIMELINE (timeline));
  assert_equals_int (g_list_length (timeline->layers), 2);

  assert_equals_string (ges_meta_container_get_string (GES_META_CONTAINER
          (project), "name"), "Example project");
  clips = ges_layer_get_clips (GES_LAYER (timeline->layers->data));
  fail_unless (ges_meta_container_get_uint (GES_META_CONTAINER
          (timeline->layers->data), "a", &a_meta));
  assert_equals_int (a_meta, 3);
  assert_equals_int (g_list_length (clips), 1);
  media_uri = ges_test_file_uri ("audio_video.ogg");
  assert_equals_string (ges_asset_get_id (ges_extractable_get_asset
          (GES_EXTRACTABLE (clips->data))), media_uri);
  g_free (media_uri);
  g_list_free_full (clips, gst_object_unref);

  /* Check tracks and the objects  they contain */
  tracks = ges_timeline_get_tracks (timeline);
  assert_equals_int (g_list_length (tracks), 2);
  for (tmp = tracks; tmp; tmp = tmp->next) {
    GList *trackelements;
    track = GES_TRACK (tmp->data);

    trackelements = ges_track_get_elements (track);
    GST_DEBUG_OBJECT (track, "Testing track");
    switch (track->type) {
      case GES_TRACK_TYPE_VIDEO:
        assert_equals_int (g_list_length (trackelements), 2);
        for (tmptrackelement = trackelements; tmptrackelement;
            tmptrackelement = tmptrackelement->next) {
          GESTrackElement *trackelement =
              GES_TRACK_ELEMENT (tmptrackelement->data);

          if (GES_IS_BASE_EFFECT (trackelement)) {
            guint nb_scratch_lines;

            ges_track_element_get_child_properties (trackelement,
                "scratch-lines", &nb_scratch_lines, NULL);
            assert_equals_int (nb_scratch_lines, 12);

            gnl_object_check (ges_track_element_get_gnlobject (trackelement),
                0, 1000000000, 0, 1000000000, MIN_GNL_PRIO, TRUE);
          } else {
            gnl_object_check (ges_track_element_get_gnlobject (trackelement),
                0, 1000000000, 0, 1000000000, MIN_GNL_PRIO + 1, TRUE);
          }
        }
        break;
      case GES_TRACK_TYPE_AUDIO:
        assert_equals_int (g_list_length (trackelements), 2);
        break;
      default:
        g_assert (1);
    }

    g_list_free_full (trackelements, gst_object_unref);

  }
  g_list_free_full (tracks, gst_object_unref);

  /* Now test the encoding profile */
  profiles = ges_project_list_encoding_profiles (project);
  assert_equals_int (g_list_length ((GList *) profiles), 1);
  profile = profiles->data;
  fail_unless (GST_IS_ENCODING_CONTAINER_PROFILE (profile));
  profiles = gst_encoding_container_profile_get_profiles (profile);
  assert_equals_int (g_list_length ((GList *) profiles), 2);
}

static void
_add_keyframes (GESTimeline * timeline)
{
  GList *tracks;
  GList *tmp;

  tracks = ges_timeline_get_tracks (timeline);
  for (tmp = tracks; tmp; tmp = tmp->next) {
    GESTrack *track;
    GList *track_elements;
    GList *tmp_tck;

    track = GES_TRACK (tmp->data);
    switch (track->type) {
      case GES_TRACK_TYPE_VIDEO:
        track_elements = ges_track_get_elements (track);

        for (tmp_tck = track_elements; tmp_tck; tmp_tck = tmp_tck->next) {
          GESTrackElement *element = GES_TRACK_ELEMENT (tmp_tck->data);

          if (GES_IS_EFFECT (element)) {
            GstControlSource *source;
            GstControlBinding *tmp_binding, *binding;

            source = gst_interpolation_control_source_new ();

            /* Check binding creation and replacement */
            binding =
                ges_track_element_get_control_binding (element,
                "scratch-lines");
            fail_unless (binding == NULL);
            ges_track_element_set_control_source (element,
                source, "scratch-lines", "direct");
            tmp_binding =
                ges_track_element_get_control_binding (element,
                "scratch-lines");
            fail_unless (tmp_binding != NULL);
            ges_track_element_set_control_source (element,
                source, "scratch-lines", "direct");
            binding =
                ges_track_element_get_control_binding (element,
                "scratch-lines");
            fail_unless (binding != tmp_binding);


            g_object_set (source, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);
            gst_timed_value_control_source_set (GST_TIMED_VALUE_CONTROL_SOURCE
                (source), 0 * GST_SECOND, 0.);
            gst_timed_value_control_source_set (GST_TIMED_VALUE_CONTROL_SOURCE
                (source), 5 * GST_SECOND, 0.);
            gst_timed_value_control_source_set (GST_TIMED_VALUE_CONTROL_SOURCE
                (source), 10 * GST_SECOND, 1.);
          }
        }
        break;
      default:
        break;
    }
  }
}

static void
_check_keyframes (GESTimeline * timeline)
{
  GList *tracks;
  GList *tmp;

  tracks = ges_timeline_get_tracks (timeline);
  for (tmp = tracks; tmp; tmp = tmp->next) {
    GESTrack *track;
    GList *track_elements;
    GList *tmp_tck;

    track = GES_TRACK (tmp->data);
    switch (track->type) {
      case GES_TRACK_TYPE_VIDEO:
        track_elements = ges_track_get_elements (track);

        for (tmp_tck = track_elements; tmp_tck; tmp_tck = tmp_tck->next) {
          GESTrackElement *element = GES_TRACK_ELEMENT (tmp_tck->data);

          if (GES_IS_EFFECT (element)) {
            GstControlBinding *binding;
            GstControlSource *source;
            GList *timed_values;
            GstTimedValue *value;

            binding =
                ges_track_element_get_control_binding (element,
                "scratch-lines");
            fail_unless (binding != NULL);
            g_object_get (binding, "control-source", &source, NULL);
            fail_unless (source != NULL);

            /* Now check keyframe position */
            timed_values =
                gst_timed_value_control_source_get_all
                (GST_TIMED_VALUE_CONTROL_SOURCE (source));
            value = timed_values->data;
            fail_unless (value->value == 0.);
            fail_unless (value->timestamp == 0 * GST_SECOND);
            timed_values = timed_values->next;
            value = timed_values->data;
            fail_unless (value->value == 0.);
            fail_unless (value->timestamp == 5 * GST_SECOND);
            timed_values = timed_values->next;
            value = timed_values->data;
            fail_unless (value->value == 1.);
            fail_unless (value->timestamp == 10 * GST_SECOND);
          }
        }
        break;
      default:
        break;
    }
  }
}

GST_START_TEST (test_project_add_keyframes)
{
  GMainLoop *mainloop;
  GESProject *project;
  GESTimeline *timeline;
  GESAsset *formatter_asset;
  gboolean saved;
  gchar *uri = ges_test_file_uri ("test-keyframes.xges");

  project = ges_project_new (uri);
  mainloop = g_main_loop_new (NULL, FALSE);

  /* Connect the signals */
  g_signal_connect (project, "loaded", (GCallback) project_loaded_cb, mainloop);
  g_signal_connect (project, "missing-uri", (GCallback) _set_new_uri, NULL);

  /* Now extract a timeline from it */
  GST_LOG ("Loading project");
  timeline = GES_TIMELINE (ges_asset_extract (GES_ASSET (project), NULL));

  g_main_loop_run (mainloop);

  GST_LOG ("Test first loading");

  g_free (uri);

  _add_keyframes (timeline);

  uri = get_tmp_uri ("test-keyframes-save.xges");
  formatter_asset = ges_asset_request (GES_TYPE_FORMATTER, "ges", NULL);
  saved =
      ges_project_save (project, timeline, uri, formatter_asset, TRUE, NULL);
  fail_unless (saved);

  gst_object_unref (timeline);
  gst_object_unref (project);

  project = ges_project_new (uri);

  ASSERT_OBJECT_REFCOUNT (project, "Our + cache", 2);

  g_signal_connect (project, "loaded", (GCallback) project_loaded_cb, mainloop);

  GST_LOG ("Loading saved project");
  timeline = GES_TIMELINE (ges_asset_extract (GES_ASSET (project), NULL));
  fail_unless (GES_IS_TIMELINE (timeline));

  g_main_loop_run (mainloop);

  _check_keyframes (timeline);

  gst_object_unref (timeline);
  gst_object_unref (project);
  g_free (uri);

  g_main_loop_unref (mainloop);
  g_signal_handlers_disconnect_by_func (project, (GCallback) project_loaded_cb,
      mainloop);
  g_signal_handlers_disconnect_by_func (project, (GCallback) asset_added_cb,
      NULL);
}

GST_END_TEST;

GST_START_TEST (test_project_load_xges)
{
  gboolean saved;
  GMainLoop *mainloop;
  GESProject *project;
  GESTimeline *timeline;
  GESAsset *formatter_asset;
  gchar *uri = ges_test_file_uri ("test-project.xges");

  project = ges_project_new (uri);
  mainloop = g_main_loop_new (NULL, FALSE);
  fail_unless (GES_IS_PROJECT (project));

  /* Connect the signals */
  g_signal_connect (project, "asset-added", (GCallback) asset_added_cb, NULL);
  g_signal_connect (project, "loaded", (GCallback) project_loaded_cb, mainloop);

  /* Make sure we update the project's dummy URL to some actual URL */
  g_signal_connect (project, "missing-uri", (GCallback) _set_new_uri, NULL);

  /* Now extract a timeline from it */
  GST_LOG ("Loading project");
  timeline = GES_TIMELINE (ges_asset_extract (GES_ASSET (project), NULL));
  fail_unless (GES_IS_TIMELINE (timeline));
  assert_equals_int (g_list_length (ges_project_get_loading_assets (project)),
      1);

  g_main_loop_run (mainloop);
  GST_LOG ("Test first loading");
  _test_project (project, timeline);
  g_free (uri);

  uri = get_tmp_uri ("test-project_TMP.xges");
  formatter_asset = ges_asset_request (GES_TYPE_FORMATTER, "ges", NULL);
  saved =
      ges_project_save (project, timeline, uri, formatter_asset, TRUE, NULL);
  fail_unless (saved);
  gst_object_unref (timeline);
  gst_object_unref (project);

  project = ges_project_new (uri);
  ASSERT_OBJECT_REFCOUNT (project, "Our + cache", 2);
  g_signal_connect (project, "asset-added", (GCallback) asset_added_cb, NULL);
  g_signal_connect (project, "loaded", (GCallback) project_loaded_cb, mainloop);

  GST_LOG ("Loading saved project");
  timeline = GES_TIMELINE (ges_asset_extract (GES_ASSET (project), NULL));
  fail_unless (GES_IS_TIMELINE (timeline));
  g_main_loop_run (mainloop);
  _test_project (project, timeline);
  gst_object_unref (timeline);
  gst_object_unref (project);
  g_free (uri);

  ASSERT_OBJECT_REFCOUNT (project, "Still 1 ref for asset cache", 1);

  g_main_loop_unref (mainloop);
  g_signal_handlers_disconnect_by_func (project, (GCallback) project_loaded_cb,
      mainloop);
  g_signal_handlers_disconnect_by_func (project, (GCallback) asset_added_cb,
      NULL);
}

GST_END_TEST;

GST_START_TEST (test_project_auto_transition)
{
  GList *layers;
  GMainLoop *mainloop;
  GESProject *project;
  GESTimeline *timeline;
  GESLayer *layer = NULL;
  GESAsset *formatter_asset;
  gboolean saved;
  gchar *tmpuri, *uri = ges_test_file_uri ("test-auto-transition.xges");

  project = ges_project_new (uri);
  mainloop = g_main_loop_new (NULL, FALSE);
  fail_unless (GES_IS_PROJECT (project));

  /* Connect the signals */
  g_signal_connect (project, "loaded", (GCallback) project_loaded_cb, mainloop);
  g_signal_connect (project, "missing-uri", (GCallback) _set_new_uri, NULL);

  /* Now extract a timeline from it */
  GST_LOG ("Loading project");
  timeline = GES_TIMELINE (ges_asset_extract (GES_ASSET (project), NULL));

  g_main_loop_run (mainloop);

  /* Check timeline and layers auto-transition, must be FALSE */
  fail_if (ges_timeline_get_auto_transition (timeline));
  layers = ges_timeline_get_layers (timeline);
  for (; layers; layers = layers->next) {
    layer = layers->data;
    fail_if (ges_layer_get_auto_transition (layer));
  }

  g_list_free_full (layers, gst_object_unref);
  g_free (uri);

  /* Set timeline and layers auto-transition to TRUE */
  ges_timeline_set_auto_transition (timeline, TRUE);

  tmpuri = get_tmp_uri ("test-auto-transition-save.xges");
  formatter_asset = ges_asset_request (GES_TYPE_FORMATTER, "ges", NULL);
  saved =
      ges_project_save (project, timeline, tmpuri, formatter_asset, TRUE, NULL);
  fail_unless (saved);

  gst_object_unref (timeline);
  gst_object_unref (project);

  project = ges_project_new (tmpuri);

  ASSERT_OBJECT_REFCOUNT (project, "Our + cache", 2);

  g_signal_connect (project, "loaded", (GCallback) project_loaded_cb, mainloop);

  GST_LOG ("Loading saved project");
  timeline = GES_TIMELINE (ges_asset_extract (GES_ASSET (project), NULL));
  fail_unless (GES_IS_TIMELINE (timeline));

  g_main_loop_run (mainloop);

  /* Check timeline and layers auto-transition, must be TRUE  */
  fail_unless (ges_timeline_get_auto_transition (timeline));
  layers = ges_timeline_get_layers (timeline);
  for (; layers; layers = layers->next) {
    layer = layers->data;
    fail_unless (ges_layer_get_auto_transition (layer));
  }

  g_list_free_full (layers, gst_object_unref);
  gst_object_unref (timeline);
  gst_object_unref (project);
  g_free (tmpuri);

  g_main_loop_unref (mainloop);
  g_signal_handlers_disconnect_by_func (project, (GCallback) project_loaded_cb,
      mainloop);
  g_signal_handlers_disconnect_by_func (project, (GCallback) asset_added_cb,
      NULL);
}

GST_END_TEST;

static GstEncodingProfile *
_create_ogg_theora_profile (void)
{
  GstEncodingContainerProfile *prof;
  GstCaps *caps;

  caps = gst_caps_from_string ("application/ogg");
  prof = gst_encoding_container_profile_new ("Ogg audio/video",
      "Standard OGG/THEORA/VORBIS", caps, NULL);
  gst_caps_unref (caps);

  caps = gst_caps_from_string ("video/x-theora");
  gst_encoding_container_profile_add_profile (prof,
      (GstEncodingProfile *) gst_encoding_video_profile_new (caps, NULL, NULL,
          0));
  gst_caps_unref (caps);

  caps = gst_caps_from_string ("audio/x-vorbis");
  gst_encoding_container_profile_add_profile (prof,
      (GstEncodingProfile *) gst_encoding_audio_profile_new (caps, NULL, NULL,
          0));
  gst_caps_unref (caps);

  return (GstEncodingProfile *) prof;
}

static void
project_proxies_created_cb (GESProject * project, GMainLoop * mainloop)
{
  g_main_loop_quit (mainloop);
}

static void
project_proxies_creation_started_cb (GESProject * project, gpointer user_data)
{
  g_print ("Proxies creation started->paused...\n");
  //ges_project_pause_proxy_creation (GES_PROJECT (project));
}

static void
project_proxies_creation_paused_cb (GESProject * project, gpointer user_data)
{

  g_print ("Proxies creation paused->started again...\n");
  //ges_project_start_proxy_creation (GES_PROJECT (project), NULL, NULL);
}

GST_START_TEST (test_project_proxy_editing)
{
  GMainLoop *mainloop;
  GstEncodingProfile *profile, *tmpprofile;
  GESProject *project;
  GESTimeline *timeline;
  GCancellable *cancellable;
  //gchar *uri = ges_test_file_uri ("test-project.xges");
  gchar *uri = gst_filename_to_uri ("/home/dark-al/test.xges", NULL);

  project = ges_project_new (uri);
  cancellable = g_cancellable_new ();
  mainloop = g_main_loop_new (NULL, FALSE);
  fail_unless (GES_IS_PROJECT (project));

  /* Connect the signals */
  g_signal_connect (project, "proxies_created",
      (GCallback) project_proxies_created_cb, mainloop);
  g_signal_connect (project, "proxies_creation_started",
      (GCallback) project_proxies_creation_started_cb, NULL);
  g_signal_connect (project, "proxies_creation_paused",
      (GCallback) project_proxies_creation_paused_cb, NULL);

  /* Make sure we update the project's dummy URL to some actual URL */
  g_signal_connect (project, "missing-uri", (GCallback) _set_new_uri, NULL);

  /* Now extract a timeline from it */
  GST_LOG ("Loading project");
  timeline = GES_TIMELINE (ges_asset_extract (GES_ASSET (project), NULL));
  fail_unless (GES_IS_TIMELINE (timeline));

  profile = _create_ogg_theora_profile ();
  ges_project_set_proxy_profile (project, profile, NULL);
  tmpprofile = ges_project_get_proxy_profile (project, NULL);
  fail_unless (gst_encoding_profile_is_equal (profile, tmpprofile));

  //ges_project_start_proxy_creation_async (project, NULL, cancellable, NULL, NULL);
  ges_project_start_proxy_creation (project, NULL, cancellable);
  g_cancellable_cancel (cancellable);

  g_main_loop_run (mainloop);

  gst_object_unref (timeline);
  gst_object_unref (project);
  g_free (uri);

  g_main_loop_unref (mainloop);
  g_signal_handlers_disconnect_by_func (project,
      (GCallback) project_proxies_created_cb, mainloop);
}

GST_END_TEST;

/*  FIXME This test does not pass for some bad reason */
#if 0
static void
project_loaded_now_play_cb (GESProject * project, GESTimeline * timeline)
{
  GstBus *bus;
  GstMessage *message;
  gboolean carry_on = TRUE;

  GESPipeline *pipeline = ges_pipeline_new ();

  fail_unless (ges_pipeline_add_timeline (pipeline, timeline));

  bus = gst_element_get_bus (GST_ELEMENT (pipeline));
  fail_if (gst_element_set_state (GST_ELEMENT (pipeline),
          GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE);

  GST_DEBUG ("Let's poll the bus");

  while (carry_on) {
    message = gst_bus_poll (bus, GST_MESSAGE_ANY, GST_SECOND / 10);
    if (message) {
      GST_ERROR ("GOT MESSAGE: %" GST_PTR_FORMAT, message);
      switch (GST_MESSAGE_TYPE (message)) {
        case GST_MESSAGE_EOS:
          /* we should check if we really finished here */
          GST_WARNING ("Got an EOS, we did not even start!");
          carry_on = FALSE;
          fail_if (TRUE);
          break;
        case GST_MESSAGE_SEGMENT_START:
        case GST_MESSAGE_SEGMENT_DONE:
          /* We shouldn't see any segement messages, since we didn't do a segment seek */
          GST_WARNING ("Saw a Segment start/stop");
          fail_if (TRUE);
          break;
        case GST_MESSAGE_ERROR:
          fail_error_message (message);
          break;
        case GST_MESSAGE_ASYNC_DONE:
          GST_DEBUG ("prerolling done");
          carry_on = FALSE;
          break;
        default:
          break;
      }
      gst_mini_object_unref (GST_MINI_OBJECT (message));
    }
  }

  fail_if (gst_element_set_state (GST_ELEMENT (pipeline),
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  gst_object_unref (pipeline);
  g_main_loop_quit (mainloop);
}


GST_START_TEST (test_load_xges_and_play)
{
  GESProject *project;
  GESTimeline *timeline;
  gchar *uri = ges_test_file_uri ("test-project_TMP.xges");

  project = ges_project_new (uri);
  fail_unless (GES_IS_PROJECT (project));

  mainloop = g_main_loop_new (NULL, FALSE);
  /* Connect the signals */
  g_signal_connect (project, "loaded", (GCallback) project_loaded_now_play_cb,
      NULL);

  /* Now extract a timeline from it */
  timeline = GES_TIMELINE (ges_asset_extract (GES_ASSET (project), NULL));
  fail_unless (GES_IS_TIMELINE (timeline));

  g_main_loop_run (mainloop);

  g_free (uri);
  gst_object_unref (project);
  gst_object_unref (timeline);
  g_main_loop_unref (mainloop);
}

GST_END_TEST;
#endif

static Suite *
ges_suite (void)
{
  Suite *s = suite_create ("ges-project");
  TCase *tc_chain = tcase_create ("project");

  suite_add_tcase (s, tc_chain);
  ges_init ();

  tcase_add_test (tc_chain, test_project_simple);
  tcase_add_test (tc_chain, test_project_add_assets);
  tcase_add_test (tc_chain, test_project_load_xges);
  tcase_add_test (tc_chain, test_project_add_keyframes);
  tcase_add_test (tc_chain, test_project_auto_transition);
  tcase_add_test (tc_chain, test_project_proxy_editing);
  /*tcase_add_test (tc_chain, test_load_xges_and_play); */
  tcase_add_test (tc_chain, test_project_unexistant_effect);

  return s;
}

GST_CHECK_MAIN (ges);
