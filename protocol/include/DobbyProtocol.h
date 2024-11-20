/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
/*
 * File:   DobbyProtocol.h
 *
 */
#ifndef DOBBYPROTOCOL_H
#define DOBBYPROTOCOL_H

// Use default service name and object path unless if overridden with
// Dobby CMake options "DOBBY_SERVICE"/"DOBBY_OBJECT".
#if defined(DOBBY_SERVICE_OVERRIDE)
    #define DOBBY_SERVICE   DOBBY_SERVICE_OVERRIDE
#else
    #define DOBBY_SERVICE   "org.rdk.dobby"
#endif
#if defined(DOBBY_OBJECT_OVERRIDE)
    #define DOBBY_OBJECT   DOBBY_OBJECT_OVERRIDE
#else
    #define DOBBY_OBJECT   "/org/rdk/dobby"
#endif


#define DOBBY_ADMIN_INTERFACE                   DOBBY_SERVICE ".admin1"
#define DOBBY_ADMIN_METHOD_PING                     "Ping"
#define DOBBY_ADMIN_METHOD_SHUTDOWN                 "Shutdown"
#define DOBBY_ADMIN_METHOD_SET_LOG_METHOD           "SetLogMethod"
#define DOBBY_ADMIN_METHOD_SET_LOG_LEVEL            "SetLogLevel"
#define DOBBY_ADMIN_METHOD_SET_AI_DBUS_ADDR         "SetAIDbusAddress"
#define DOBBY_ADMIN_EVENT_READY                     "Ready"

#define DOBBY_CTRL_INTERFACE                    DOBBY_SERVICE ".ctrl1"
#define DOBBY_CTRL_METHOD_START                     "Start"
#define DOBBY_CTRL_METHOD_START_FROM_SPEC           "StartFromSpec"
#define DOBBY_CTRL_METHOD_START_FROM_BUNDLE         "StartFromBundle"
#define DOBBY_CTRL_METHOD_STOP                      "Stop"
#define DOBBY_CTRL_METHOD_PAUSE                     "Pause"
#define DOBBY_CTRL_METHOD_RESUME                    "Resume"
#define DOBBY_CTRL_METHOD_HIBERNATE                 "Hibernate"
#define DOBBY_CTRL_METHOD_WAKEUP                    "Wakeup"
#define DOBBY_CTRL_METHOD_MOUNT                     "Mount"
#define DOBBY_CTRL_METHOD_UNMOUNT                   "Unmount"
#define DOBBY_CTRL_METHOD_ANNOTATE                  "Annotate"
#define DOBBY_CTRL_METHOD_REMOVE_ANNOTATION         "RemoveAnnotation"
#define DOBBY_CTRL_METHOD_EXEC                      "Exec"
#define DOBBY_CTRL_METHOD_GETSTATE                  "GetState"
#define DOBBY_CTRL_METHOD_GETINFO                   "GetInfo"
#define DOBBY_CTRL_METHOD_LIST                      "List"
#define DOBBY_CTRL_EVENT_STARTED                    "Started"
#define DOBBY_CTRL_EVENT_STOPPED                    "Stopped"
#define DOBBY_CTRL_EVENT_STOPPED_WITH_STATUS        "StoppedWithStatus"
#define DOBBY_CTRL_EVENT_HIBERNATED                 "Hibernated"
#define DOBBY_CTRL_EVENT_AWOKEN                     "Awoken"

#define DOBBY_DEBUG_INTERFACE                   DOBBY_SERVICE ".debug1"
#define DOBBY_DEBUG_METHOD_CREATE_BUNDLE            "CreateBundle"
#define DOBBY_DEBUG_METHOD_GET_SPEC                 "GetSpec"
#define DOBBY_DEBUG_METHOD_GET_OCI_CONFIG           "GetOCIConfig"
#define DOBBY_DEBUG_START_INPROCESS_TRACING         "StartInProcessTracing"
#define DOBBY_DEBUG_STOP_INPROCESS_TRACING          "StopInProcessTracing"

#define CONTAINER_STATE_INVALID                 0
#define CONTAINER_STATE_STARTING                1
#define CONTAINER_STATE_RUNNING                 2
#define CONTAINER_STATE_STOPPING                3
#define CONTAINER_STATE_PAUSED                  4
#define CONTAINER_STATE_HIBERNATING             5
#define CONTAINER_STATE_HIBERNATED              6
#define CONTAINER_STATE_AWAKENING               7

#define DOBBY_LOG_NULL                          0
#define DOBBY_LOG_SYSLOG                        1
#define DOBBY_LOG_ETHANLOG                      2
#define DOBBY_LOG_CONSOLE                       3

#endif // !defined(DOBBYPROTOCOL_H)
