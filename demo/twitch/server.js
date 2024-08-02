
const path = require("path");
const tmi = require('tmi.js');
const fs = require('fs');
var graph = require('./graph');

// how frequently to update the graph
const graph_update_interval = 0.5;

// how many times to send a command (to prevent interference)
const command_multiplicity = 8;
const command_multiplicity_interval = 100;

const VoteType = {
  // Throttle: Symbol("throttle"),
  // Horn: Symbol("horn"),
  // Bell: Symbol("bell"),
  // Direction: Symbol("direction"),
  // Juction: Symbol("junction")
  Throttle: "throttle",
  Horn: "horn",
  Bell: "bell",
  Direction: "direction",
  Juction: "junction"
};

function getVoteTypeName(type) {
  if (type === VoteType.Throttle)
    return "Throttle";
  else if (type === VoteType.Horn)
    return "Horn";
  else if (type === VoteType.Bell)
    return "Bell";
  else if (type === VoteType.Direction)
    return "Direction";
  else if (type === VoteType.Junction)
    return "Junction";
  return "Unkown";
}

var vote_callbacks = {};
var channel_vote_data = {};

const port = process.env.PORT || 80;

const args = process.argv.slice(2).map(s => s.trim("\""));
if (args.length < 1)
  console.error("Error: No engine number specified! Defaulting to 0.");

var client = null;
var channel_engine_ids = {};
var input_engine_ids = [];
var bot_user = "";
var bot_client_id = "";
var bot_client_secret = "";
var bot_channels = [];
var vote_time = 5.0;
var dialog_cooldown_time = 5.0;
var max_throttle = 200.0;
var input_is_legacy = null;

for (let i = 0; i < args.length; i++) {
  if (args[i] === "-engine" && i + 1 < args.length) {
    input_engine_ids = args[i + 1].split(",").map(s => parseInt(s.trim()));

    for (let i = 0; i < input_engine_ids.length; i++) {
      if (isNaN(input_engine_ids[i]) || input_engine_ids[i] < 0) {
        console.error(`Invalid engine ID entered: '${input_engine_ids[i]}'. Defaulting to 0.`);
        input_engine_ids[i] = 0;
      }
    }
  }
  else if (args[i] === "-user" && i + 1 < args.length) {
    bot_user = args[i + 1];
  }
  else if (args[i] === "-client" && i + 1 < args.length) {
    bot_client_id = args[i + 1];
  }
  else if (args[i] === "-secret" && i + 1 < args.length) {
    bot_client_secret = args[i + 1];
  }
  else if (args[i] === "-channel" && i + 1 < args.length) {
    bot_channels = args[i + 1].split(",").map(s => s.trim());
  }
  else if (args[i] === "-votetime" && i + 1 < args.length) {
    // cooldown time in seconds
    vote_time = parseFloat(args[i + 1]);
  }
  else if (args[i] === "-dialogtime" && i + 1 < args.length) {
    // cooldown time in seconds
    dialog_cooldown_time = parseFloat(args[i + 1]);
  }
  else if (args[i] === "-maxthrottle" && i + 1 < args.length) {
    // cooldown time in seconds
    max_throttle = parseFloat(args[i + 1]);

    if (max_throttle > 200) {
      console.error("Invalid maxthrottle specified! Max 200.");
      max_throttle = 200;
    }
  }
  else if (args[i] === "-legacy" && i + 1 < args.length) {
    input_is_legacy = args[i + 1].split(",").map(s => parseInt(s.trim()));

  }
}

if (!bot_user)
  console.error("No bot username specified!");

if (!bot_client_id)
  console.error("No bot client ID specified!");
  
if (!bot_client_secret)
  console.error("No bot client secret specified!");

if (bot_channels.length == 0)
  console.error("No bot channels specified!");

if (input_engine_ids.length == 0)
  console.error("No engine IDs specified!");

if (input_engine_ids.length == bot_channels.length) {
  for (let i = 0; i < input_engine_ids.length; i++) {
    console.log(`${bot_channels[i]}: Engine ${input_engine_ids[i]}`);
    channel_engine_ids["#" + bot_channels[i]] = input_engine_ids[i];
  }
}
else {
  console.error("Number of engine IDs doesn't match number of channels!");
}

if (input_is_legacy != null && input_is_legacy.length !== input_engine_ids.length) {
  console.error("Number of engine IDs doesn't match number of -legacy!");
  input_is_legacy = null;
}

if (input_is_legacy == null) {
  input_is_legacy = [];
  for (let i = 0; i < input_engine_ids.length; i++)
    input_is_legacy[i] = true;
}

var engine_is_legacy = {};
for (let i = 0; i < input_engine_ids.length; i++)
  engine_is_legacy[input_engine_ids[i]] = input_is_legacy[i];


var twitch = require('./twitch');

const tokenPath = './tokens.json';
var bot_token = null;
var bot_refresh_token = null;

if (fs.existsSync(tokenPath)) {
  let rawdata = fs.readFileSync(tokenPath);
  let json = JSON.parse(rawdata);

  bot_token = json.token;
  bot_refresh_token = json.refresh_token;
  
  tryStartClient(true);
}
else {
  // first time auth
  twitch.getAuthCode(bot_client_id)
  .then((auth_code) => {
    twitch.getToken(bot_client_id, bot_client_secret, auth_code)
    .then((data) => {
      bot_token = data.token;
      bot_refresh_token = data.refresh_token;

      let json = JSON.stringify({
        token: bot_token,
        refresh_token: bot_refresh_token
      });
      fs.writeFileSync(tokenPath, json);

      tryStartClient(false);
    });
  });
}


// connect to C++ app
const tmcc_port = 8080;
tmcc = null;

require('net').createServer(function (socket) {
  console.log("Connected to TMCC");

  tmcc = socket;
  socket.on('data', function (data) {
      console.log(data.toString());
  });

  // socket.write('Hello\n');
  // socket.write('Hello 2\n');
}).listen(tmcc_port);


var is_currently_legacy = true;
function checkLegacy(engine_id) {
  let should_be_legacy = engine_is_legacy[engine_id];
  if (should_be_legacy !== is_currently_legacy)
    tmcc.write(`setLegacy ${should_be_legacy ? 1 : 0}\r\n`);
}

function writeCommand(command) {
  tmcc.write(command);
  for (let i = 1; i < command_multiplicity; i++) {
    setTimeout(function () {
      tmcc.write(command);
    }, i * command_multiplicity_interval);
  }
}


// process.on('uncaughtException', function (err) {
//   console.error(err.stack);
// });


function tryStartClient(retry) {
  console.log(`bot token: '${bot_token}', refresh token '${bot_refresh_token}'`);
  if (!bot_token || !bot_refresh_token) {
    console.error("Invalid token or refresh token!");
    return;
  }

  const opts = {
    identity: {
      username: bot_user,
      password: bot_token
    },
    channels: bot_channels
  };

  // Create a client with our options
  client = new tmi.client(opts);

  // Register our event handlers (defined below)
  client.on('message', onMessageHandler);
  client.on('connected', onConnectedHandler);

  // Connect to Twitch:
  client.connect().catch(err => {
    if (retry) {
      console.log("Token has (probably) expired - refreshing.");

      twitch.getAuthCode(bot_client_id)
      .then((auth_code) => {
        twitch.refreshToken(bot_client_id, bot_client_secret, auth_code, bot_refresh_token)
        .then((data) => {
          bot_token = data.token;
          bot_refresh_token = data.refresh_token;

          console.log("Updated token file.");
          let json = JSON.stringify({
              token: bot_token,
              refresh_token: bot_refresh_token
          });
          fs.writeFileSync(tokenPath, json);

          tryStartClient(false);
        });
      });
    }
    else {
      console.error("Failed to refresh token.");
    }
  });
}

var last_dialog_time = 0;

var channel_commands = {};
bot_channels.forEach(function(c) {
  channel_commands[c] = new Set();
});

// Called every time a message comes in
function onMessageHandler (target, tags, msg, self) {
  if (self) { return; } // Ignore messages from the bot

  // Remove whitespace from chat message
  const commandName = msg.trim();

  if (commandName === "!help") {
    // always give help immediately, don't trigger the cooldown
    client.say(target, `@${tags['display-name']}
      Commands:
      '!throttle [0-200]' - Set the locomotive's speed.
      '!horn' - Honk the horn!
      '!bell [on/off]' - Toggle the locomotive's bell.
      '!direction [forward/backward]' - Should the train move forward or backwards?
      '!junction [junction_id] [out/through]' - Switch the direction of a junction on the layout.
    `);
    return;
  }

  let name = tags['display-name'];

  // dialog is handled separately - it has its own cooldown, separate from voting periods
  if (commandName.startsWith("!dialog")) {
    let args = commandName.split(" ");
    if (args.length < 2)
      return;
    
    const time = Date.now() / 1000.0;

    if (args[0] === "!dialog" && time - last_dialog_time >= dialog_cooldown_time) {
      if (processDialog(client, target, args[1])) {
        last_dialog_time = time;
      }
      else {
        // REMOVE THIS LOGGING?
        client.say(target, `@${tags['display-name']}: Invalid dialog command!`);
      }
    }

    return;
  }


  // user can only send one command per command interval
  if (channel_commands[target].has(name)) {
    // REMOVE THIS LOGGING?
    client.say(target, `@${tags['display-name']}: You have already entered a command!`);
    return;
  }
  channel_commands[target].add(name);

  // const time = Date.now() / 1000.0;

  // command cooldown of 5 seconds
  // if (time - last_command_time < vote_time)
  //   return;

  // only set the command time if we actually run a command
  // last_command_time = time;
  
  // If the command is known, let's execute it
  if (processCommand(client, target, commandName)) {
    // client.say(target, `You rolled a ${num}`);
    // last_command_time = time;
  }
}

// Called every time the bot connects to Twitch chat
function onConnectedHandler (addr, port) {
  console.log(`* Connected to ${addr}:${port}`);

  // once connected, begin processing votes
  setInterval(function() {
    bot_channels.forEach(c => processVotes(c));
  }, vote_time * 1000);

  setInterval(function() {
    bot_channels.forEach(c => updateGraph(c));
  }, graph_update_interval * 1000);
}

function processCommand(client, target, command) {
  let args = command.split(" ");
  if (args.length == 0)
    return false;

  if (args[0] === "!throttle" && args.length > 1) {
    let throttleValue = parseInt(args[1]);
    if (!isNaN(throttleValue) && throttleValue >= 0) { //  && throttleValue <= 200
      if (throttleValue > max_throttle)
        throttleValue = max_throttle;
      addVote(target, VoteType.Throttle, throttleValue);
    }
  }
  else if (args[0] === "!horn") {
    addVote(target, VoteType.Horn);
    return true;
  }
  else if (args[0] === "!bell" && args.length > 1) {
    if (args[1] === "on" || args[1] === "off") {
      addVote(target, VoteType.Bell, args[1]);
      return true;
    }
  }
  else if (args[0] === "!direction" && args.length > 1) {
    if (args[1] === "forward" || args[1] === "backward") {
      addVote(target, VoteType.Direction, args[1]);
      return true;
    }
  }
  else if (args[0] === "!junction" && args.length > 2) {
    let junctionId = parseInt(args[1]);
    let junctionDir = args[2];
    if (!isNaN(junctionId) && junctionId >= 0 && (junctionDir === "out" || junctionDir === "through")) {
      addVote(target, VoteType.Junction, junctionId, junctionDir);
      return true;
    }
  }
  return false;
}

var dialogCommands = {
  "conventional-shutdown":       0b00000001,
  // "DC_SCENE_2_KEY_CONTEXT_DEPENDENT":    0b00000010,
  // "DC_SCENE_7_KEY_CONTEXT_DEPENDENT":    0b00000011,
  // "DC_SCENE_5_KEY_CONTEXT_DEPENDENT":    0b00000100,
  "conventional-short-horn-trigger":  0b00000101,
  "initial-engine-startup":     0b00000110,
  "engineer-departure-denied":        0b00000111,
  "engineer-departure-granted":       0b00001000,
  "engineer-have-departed":           0b00001001,
  "engineer-allclear":               0b00001010,
  "tower-non-emg-stop":               0b00001011,
  "tower-restricted-speed":           0b00001100,
  "tower-slow-speed":                 0b00001101,
  "tower-medium-speed":               0b00001110,
  "tower-limited-speed":              0b00001111,
  "tower-normal-speed":               0b00010000,
  "tower-highball-speed":            0b00010001,
  "engineer-arriving-soon":            0b00010010,
  "engineer-have-arrived":             0b00010011,
  "engineer-shutdown":               0b00010100,
  "engineer-identify":                0b00010101,
  "engineer-ack-comm":                0b00010110,
  "engineer-ack-speed-to-stop":       0b00010111,
  "engineer-ack-speed-to-restricted": 0b00011000,
  "engineer-ack-speed-to-slow":       0b00011001,
  "engineer-ack-speed-to-medium":     0b00011010,
  "engineer-ack-speed-to-limited":    0b00011011,
  "engineer-ack-speed-to-normal":     0b00011100,
  "engineer-ack-speed-to-highball":  0b00011101,
  "engineer-context-dependent":       0b00011110,
  "emergency-context-dependent":      0b00011111,
  "tower-context-dependent":          0b00100000,
  //DC_RESERVED2                        = 0b00100001,
  "tower-departure-denied":           0b00100010,
  "tower-departure-granted":          0b00100011,
  "tower-have-departed":              0b00100100,
  "tower-allclear":                  0b00100101,

  "tower-arriving-soon":         0b00101101,
  "tower-have-arrived":          0b00101110,
  "tower-shutdown":             0b00101111,
  "conductor-all-aboard":        0b00110000,
  "engineer-ack-standing-by":    0b00110001,
  "engineer-ack-cleared-to-go":  0b00110010,
  "engineer-ack-clear-ahead":    0b00110011,
  "engineer-ack-clear-inbound":  0b00110100,
  "engineer-ack-welcome-back":   0b00110101,
  "engineer-ack-identify-out":   0b00110110,

  // DC_SCENE_2_CONTEXT_DEPENDANT      = 0b00110111,
  // DC_SCENE_AUX1_2_CONTEXT_DEPENDANT = 0b00111000,
  // DC_SCENE_7_CONTEXT_DEPENDANT      = 0b00111001,
  // DC_SCENE_AUX1_7_CONTEXT_DEPENDANT = 0b00111010,
  // DC_SCENE_5_CONTEXT_DEPENDANT      = 0b00111011,
  // DC_SCENE_AUX1_5_CONTEXT_DEPENDANT = 0b00111100,

  "engineer-speak-fuel-level":      0b00111101,
  "engineer-speak-fuel-refill":   0b00111110,
  "engineer-speak-speed":         0b00111111,
  "engineer-speak-water-level":   0b01000000,
  "engineer-speak-water-refill":  0b01000001,

  // Doc 1.22 says nothing until 0x50, i think otherwise. 
  "seq-control-disable":             0b01010000,
  "seq-control-enable":              0b01010001,
  "seq-control-cleared":             0b01010010,
  "seq-control-have-departed":       0b01010011,
  "seq-control-in-transit":          0b01010100,
  "seq-control-max-speed":           0b01010101,
  "seq-control-clear-ahead":         0b01010110,
  "seq-control-random":              0b01010111,
  "seq-control-arriving-short-trip": 0b01011000,
  "seq-control-arriving-long-trip":  0b01011001,
  // "seq-control-RESERVED":            0b01011010,
  // "seq-control-RESERVED2":           0b01011011,
  "seq-control-arrived-short-trip":  0b01011100,
  "seq-control-arrived-long-trip":   0b01011101,
  // "seq-control-RESERVED3":           0b01011110,
  // "seq-control-RESERVED4":           0b01011111,

  "soundcar-conductor-next-stop":      0b01101000,
  "soundcar-conductor-watch-step":     0b01101001,
  "soundcar-conductor-all-aboard":     0b01101010,
  "soundcar-conductor-tickets-please": 0b01101011,
  "soundcar-conductor-premature-stop": 0b01101100,
  "soundcar-steward-welcome":          0b01101101,
  "soundcar-steward-first-seating":    0b01101110,
  "soundcar-steward-second-seating":   0b01101111,
  "soundcar-steward-lounge-open":      0b01110000,
  "soundcar-pa-train-arriving":        0b01110001,
  "soundcar-pa-train-arrived":         0b01110010,
  "soundcar-pa-train-boarding":        0b01110011,
  "soundcar-pa-train-departing":       0b01110100,
  "soundcar-startup":                  0b01110101,
  "soundcar-shutdown":                 0b01110110,
};

function processDialog(client, channel, dialog) {
  if (dialog in dialogCommands) {
    client.say(channel, `Playing dialog: ${dialog}`);
    graph.postCommand(channel, `Play dialog: '${dialog}'`);

    let value = dialogCommands[dialog];
    if (tmcc) {
      checkLegacy(channel_engine_ids[channel]);
      writeCommand(`dialog ${channel_engine_ids[channel]} ${value}\r\n`);
    }
    return true;
  }
  return false;
}

// thanks stack overflow, this is pretty concise!
// basically just counts up how many of each element there are,
// then loops through again to find which one had the most occurences.
function mode(a) {
  return Object.values(
    a.reduce((count, e) => {
      if (!(e in count)) {
        count[e] = [0, e];
      }
      
      count[e][0]++;
      return count;
    }, {})
  ).reduce((a, v) => v[0] < a[0] ? a : v, [0, null])[1];
}

declareVoteType(VoteType.Throttle,
  // Vote data
  function() {
    return { values: [] };
  },
  // Process a vote
  function(channel, data, args) {
    data.values.push(args[0]);
  },
  // Category has won
  function(channel, data) {
    // let choice = mode(data.values);
    let avg = data.values.reduce((a, b) => a + b) / data.values.length;

    client.say(channel, `Setting the throttle to ${avg}!`);
    graph.postCommand(channel, `Set Throttle to '${avg}'`);

    graph.updateSpeed(channel, avg);
    if (tmcc) {
      checkLegacy(channel_engine_ids[channel]);
      writeCommand(`setThrottle ${channel_engine_ids[channel]} ${(avg / 200.0)}\r\n`);
    }
  }
);

declareVoteType(VoteType.Horn,
  // Vote data
  function() {
    return null;
  },
  // Process a vote
  function(channel, data, args) {
  },
  // Category has won
  function(channel, data) {
    client.say(channel, "Honking the horn!");
    graph.postCommand(channel, "Blow Horn");

    // if (tmcc) {
    //   writeCommand(`blowHorn ${channel_engine_ids[channel]}\r\n`);
    // }

    // interval between blow horn commands
    // const hornDelay = 200;
    // let engineID = channel_engine_ids[channel];
    // for (let i = 0; i < 5; i++) {
    //   setTimeout(function () {
    //     if (tmcc) {
    //       checkLegacy(engineID);
    //       writeCommand(`blowHorn ${engineID}\r\n`);
    //     }

    //   }, i * hornDelay);
    // }
    writeCommand(`blowHorn ${engineID}\r\n`);
  }
);

declareVoteType(VoteType.Bell,
  // Vote data
  function() {
    return { values: [] };
  },
  // Process a vote
  function(channel, data, args) {
    data.values.push(args[0]);
  },
  // Category has won
  function(channel, data) {
    let choice = mode(data.values);

    if (choice === "on") {
      client.say(channel, "Ringing the bell!");
      graph.postCommand(channel, "Ring Bell");

      if (tmcc) {
        checkLegacy(channel_engine_ids[channel]);
        writeCommand(`setBell ${channel_engine_ids[channel]} 1\r\n`);
      }
    }
    else {
      client.say(channel, "Turning off the bell!");
      graph.postCommand(channel, "Bell Off");

      if (tmcc) {
        checkLegacy(channel_engine_ids[channel]);
        writeCommand(`setBell ${channel_engine_ids[channel]} 0\r\n`);
      }
    }
  }
);

declareVoteType(VoteType.Direction,
  // Vote data
  function() {
    return { values: [] };
  },
  // Process a vote
  function(channel, data, args) {
    data.values.push(args[0]);
  },
  // Category has won
  function(channel, data) {
    let choice = mode(data.values);

    client.say(channel, `Setting the direction to ${choice}!`);
    graph.postCommand(channel, `Set Direction to '${choice}'`);

    graph.updateSpeed(channel, 0);
    if (choice === "forward") {
      if (tmcc) {
        checkLegacy(channel_engine_ids[channel]);
        writeCommand(`setDirection ${channel_engine_ids[channel]} 1\r\n`);
      }
    }
    else {
      if (tmcc) {
        checkLegacy(channel_engine_ids[channel]);
        writeCommand(`setDirection ${channel_engine_ids[channel]} 0\r\n`);
      }
    }
  }
);

declareVoteType(VoteType.Junction,
  // Vote data
  function() {
    return { junction_votes: {} };
  },
  // Process a vote
  function(channel, data, args) {
    let id = args[0];
    let dir = args[1];
    if (!(id in data.junction_votes))
      data.junction_votes[id] = [];
    
    data.junction_votes[id].push(dir);
  },
  // Category has won
  function(channel, data) {
    let highest_votes = 0;
    let highest_id = null;
    for (const [key, value] of Object.entries(data.junction_votes)) {
      if (value.length > highest_votes) {
        highest_votes = value.length;
        highest_id = key;
      }
    }
    
    if (!highest_id || highest_id.length == 0)
      return;
    
    let choice = mode(data.junction_votes[highest_id]);

    client.say(channel, `Setting junction ${highest_id} to ${choice}!`);
    graph.postCommand(channel, `Set Junction '${highest_id}' to '${choice}'`);

    if (choice === "out") {
      if (tmcc) {
        writeCommand(`setJunctionOut ${highest_id}\r\n`);
      }
    }
    else {
      if (tmcc) {
        writeCommand(`setJunctionThrough ${highest_id}\r\n`);
      }
    }
  }
);

// voting internal functions. declare all vote implementations above this
function declareVoteType(type, getDefaultValue, processCallback, winCallback) {
  vote_callbacks[type] = {
    default: getDefaultValue,
    process: processCallback,
    win: winCallback
  };
}

function addVote(channel, type, ...args) {
  if (!(type in vote_callbacks)) {
    console.error(`Unknown vote type '${type}'!`);
    return;
  }

  let data = channel_vote_data[channel];

  // vote added prematurely
  if (!data.vote_data)
    return;
  
  vote_callbacks[type].process(channel, data.vote_data[type], args);
  data.vote_counts[type]++;
}

function resetVotes() {
  bot_channels.forEach(function(c) {
    let vote_data = {};
    let vote_counts = {};
    channel_commands[c] = new Set();

    for (const [key, value] of Object.entries(vote_callbacks)) {
      vote_data[key] = value.default();
      vote_counts[key] = 0;
    }

    channel_vote_data[c] = {
      vote_data: vote_data,
      vote_counts: vote_counts
    };
  });
}

function processVotes(channel) {
  let data = channel_vote_data[channel];
  if (!data) {
    resetVotes();
    return;
  }

  // find which category has the most votes
  let highest = 0;
  let highest_type = null;
  for (const [key, value] of Object.entries(vote_callbacks)) {
    if (data.vote_counts[key] > highest) {
      highest = data.vote_counts[key];
      highest_type = key;
    }
  }

  if (highest == 0) {
    // client.say(channel, "Debug: No votes recorded!");

    resetVotes();
    return;
  }

  vote_callbacks[highest_type].win(channel, data.vote_data[highest_type]);
  
  resetVotes();
}

function updateGraph(channel) {
  let data = channel_vote_data[channel];
  if (!data) {
    return;
  }

  // find which category has the most votes
  graph_data = {
    title: 'Votes',
    votes: []
  }

  // let highest = 0;
  // let highest_type = null;
  for (const [key, value] of Object.entries(vote_callbacks)) {
    // skip empty categories
    if (data.vote_counts[key] == 0)
      continue;
    
    graph_data.votes.push({
      text: getVoteTypeName(key),
      value: data.vote_counts[key]
    });
  }

  if (graph_data.votes.length == 0) {
    return;
  }

  // sort graph bars
  graph_data.votes.sort((a,b) => a.value - b.value);

  graph.updateGraph(channel, graph_data);
}