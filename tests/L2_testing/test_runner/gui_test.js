/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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

'use strict';

px.import({
  scene:'px:scene.1.js',
  ws: 'ws'
}).then( function ready(imported) {
  const WebSocket = imported.ws;

  let ws;
  const connect = () => {
    return new Promise( (resolve, reject) => {
      ws = new WebSocket('ws://localhost:9005/as/apps/status');
      ws.onopen = () => { resolve(ws); };
      ws.onerror = error => { reject(error); };
      ws.onmessage = e => {
		  console.log("Found status:\n-----------------------begin_as_apps_status-----------------------\n" +
		  e.data +
		  "\n----------------------end_of_as_apps_status----------------------");
		  }
    });
  };


  // Run the test
  connect()
    .catch( error => { console.log('web socket can\'t be opened: ' + error);})
    .then( () => {
      // TODO - need to close Spark
    });

}).catch( err => { console.error('Import failed: ' + err); });

