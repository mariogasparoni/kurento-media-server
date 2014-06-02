/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 2.1 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 */

#include <gst/gst.h>
#include "RabbitMQService.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define GST_CAT_DEFAULT kurento_rabbitmq_service
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoRabbitMQService"

/* This is included to avoid problems with slots and lamdas */
#include <type_traits>
#include <sigc++/sigc++.h>
#include <event2/event_struct.h>
namespace sigc
{
template <typename Functor>
struct functor_trait<Functor, false> {
  typedef decltype (::sigc::mem_fun (std::declval<Functor &> (),
                                     &Functor::operator() ) ) _intermediate;

  typedef typename _intermediate::result_type result_type;
  typedef Functor functor_type;
};
}

/* Config file values */
#define RABBITMQ_GROUP "RabbitMQ"
#define RABBITMQ_SERVER_ADDRESS "serverAddress"
#define RABBITMQ_SERVER_PORT "serverPort"

/* Default config values */
#define RABBITMQ_SERVER_ADDRESS_DEFAULT "127.0.0.1"
#define RABBITMQ_SERVER_PORT_DEFAULT 5672

#define PIPELINE_CREATION "pipeline_creation"

namespace kurento
{

static void
check_port (int port)
{
  if (port <= 0 || port > G_MAXUSHORT) {
    throw Glib::KeyFileError (Glib::KeyFileError::PARSE, "Invalid port value");
  }
}

RabbitMQService::RabbitMQService (Glib::KeyFile &confFile) : Service (confFile)
{
  sigset_t mask;
  std::string address;
  int port;

  try {
    address = confFile.get_string (RABBITMQ_GROUP, RABBITMQ_SERVER_ADDRESS);
  } catch (const Glib::KeyFileError &err) {
    GST_WARNING ("Setting default address %s to media server",
                 RABBITMQ_SERVER_ADDRESS_DEFAULT);
    address = RABBITMQ_SERVER_ADDRESS_DEFAULT;
  }

  try {
    port = confFile.get_integer (RABBITMQ_GROUP, RABBITMQ_SERVER_PORT);
    check_port (port);
  } catch (const Glib::KeyFileError &err) {
    GST_WARNING ("Setting default port %d to media server",
                 RABBITMQ_SERVER_PORT_DEFAULT);
    port = RABBITMQ_SERVER_PORT_DEFAULT;
  }

  this->address = address;
  this->port = port;

  setConfig (address, port);
  this->confFile.load_from_data (confFile.to_data() );

  sigemptyset (&mask);
  sigaddset (&mask, SIGCHLD);
  signalHandler = std::shared_ptr <SignalHandler> (new SignalHandler (mask,
                  std::bind (&RabbitMQService::childSignal, this, std::placeholders::_1) ) );
}

RabbitMQService::~RabbitMQService()
{
}

void
RabbitMQService::processMessage (RabbitMQMessage &message)
{
  int pid = fork();

  if (pid < 0) {
    throw RabbitMQException ("Proccess cannot be forked");
  } else if (pid == 0) {
    /* Child process */
    childs.clear();
    stopListen();
    getConnection()->noCloseOnRelease();

    pipeline = std::shared_ptr<RabbitMQPipeline> (new RabbitMQPipeline (confFile,
               address,
               port) );
    pipeline->startRequest (message);

  } else {
    /* Parend process  */
    childs.push_back (pid);
    message.noRejectOnRelease();
  }
}

void RabbitMQService::start ()
{
  listenQueue (PIPELINE_CREATION, true);
}

void RabbitMQService::stop ()
{
  stopListen();
  GST_DEBUG ("stop service");

  if (pipeline) {
    pipeline.reset();
  }

  for (auto pid : childs) {
    GST_DEBUG ("Killing child %d", pid);
    kill (pid, SIGINT);
  }
}

void
RabbitMQService::childSignal (uint32_t signal)
{
  int pid;
  int status;

  pid = waitpid (-1, &status, WNOHANG);

  if (pid > 0) {
    GST_DEBUG ("Child %d terminated", pid);
    childs.remove (pid);
  }
}


RabbitMQService::StaticConstructor RabbitMQService::staticConstructor;

RabbitMQService::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */