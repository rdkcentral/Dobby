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

