/*
 * PlayerEndPoint.cpp - Kurento Media Server
 *
 * Copyright (C) 2013 Kurento
 * Contact: Miguel París Díaz <mparisdiaz@gmail.com>
 * Contact: José Antonio Santos Cadenas <santoscadenas@kurento.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PlayerEndPoint.hpp"

namespace kurento
{

PlayerEndPoint::PlayerEndPoint (std::shared_ptr<MediaManager> parent, const std::string &uri)
  : UriEndPoint (parent, uri, UriEndPointType::type::PLAYER_END_POINT)
{
  gchar *name;

  name = getIdStr ();
  element = gst_element_factory_make ("playerendpoint", name);
  g_free (name);

  g_object_ref (element);
  gst_bin_add (GST_BIN (parent->element), element);
  gst_element_sync_state_with_parent (element);
}

PlayerEndPoint::~PlayerEndPoint() throw ()
{
  gst_bin_remove (GST_BIN (parent->element), element);
  gst_element_set_state (element, GST_STATE_NULL);
  g_object_unref (element);
}

} // kurento