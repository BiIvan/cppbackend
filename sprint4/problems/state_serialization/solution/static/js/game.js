'use strict';

const loadedTiles = {};
const loadedLootTypes = [];
let loadedActor;
let globalScene;
let mixers = [];
let playerId;
let camera;
let cameraControls;
let lootTypesLoaded = false;

const lootRotationSpeed = 0.0025;
const lootWavingSpeed = 0.005;
const roadH = 0.55;

const objLoader = new THREE.OBJLoader();

function loadRoadTiles() {
  const loader = new THREE.TextureLoader();
  loader.setPath('assets/');

  for (const key of ['nt', 't', 'tr', 'v', 'vh']) {
    loadedTiles[key] = new THREE.MeshBasicMaterial({
      map: loader.load(`road_${key}.png`),
    });
  }
}

function paintObject(object, color) {
  const material = new THREE.MeshPhongMaterial({
    color,
    specular: 0x1d1c3a,
    reflectivity: 0.8,
    shininess: 20,
  });

  object.traverse((child) => {
    if (child.isMesh) {
      child.castShadow = true;
      child.material = material;
    }
  });
}

function loadLootType(loot, then) {
  if (!loot || loot.type !== 'obj' || !loot.file) {
    then(undefined);
    return;
  }

  objLoader.load(
    loot.file,
    (object) => {
      const scale = Number.isFinite(loot.scale) ? loot.scale : 1;
      const rotation = Number.isFinite(loot.rotation) ? loot.rotation : 0;
      object.scale.set(scale, scale, scale);
      object.rotation.set(Math.PI * rotation / 180, 0, 0);
      paintObject(object, loot.color ?? 0xffffff);
      then(object);
    },
    undefined,
    (error) => {
      console.error(`Failed to load loot model: ${loot.file}`, error);
      then(undefined);
    },
  );
}

function loadLootTypes(then) {
  const lootTypes = Array.isArray(map?.lootTypes) ? map.lootTypes : [];

  if (lootTypes.length === 0) {
    lootTypesLoaded = true;
    then();
    return;
  }

  let objectsToLoad = lootTypes.length;
  lootTypes.forEach((data, index) => {
    loadLootType(data, (object) => {
      if (object) {
        loadedLootTypes[index] = {obj: object, data};
      }

      objectsToLoad -= 1;
      if (objectsToLoad === 0) {
        lootTypesLoaded = true;
        then();
      }
    });
  });
}

function vecNorm(vector) {
  return Math.hypot(vector[0], vector[1]);
}

function vecSub(left, right) {
  return [left[0] - right[0], left[1] - right[1]];
}

function vecSum(left, right) {
  return [left[0] + right[0], left[1] + right[1]];
}

function vecMul(vector, scalar) {
  return [vector[0] * scalar, vector[1] * scalar];
}

function vec3Diff(left, right) {
  return [left[0] - right[0], left[1] - right[1], left[2] - right[2]];
}

function vec3Sum(left, right) {
  return [left[0] + right[0], left[1] + right[1], left[2] + right[2]];
}

function vec3Mul(vector, scalar) {
  return [vector[0] * scalar, vector[1] * scalar, vector[2] * scalar];
}

class MovingObject {
  constructor(speedCoefficient, initialPosition) {
    this.pos = initialPosition;
    this.speedCoefficient = speedCoefficient;
    this.targetPos = initialPosition;
  }

  setTarget(targetPosition) {
    this.targetPos = targetPosition;
  }

  doMove(time) {
    const vector = vec3Diff(this.targetPos, this.pos);
    const moveCoefficient = Math.min(this.speedCoefficient * time, 1);
    this.pos = vec3Sum(this.pos, vec3Mul(vector, moveCoefficient));
  }
}

function convDirection(direction) {
  return {
    R: Math.PI / 2,
    U: Math.PI,
    L: Math.PI * 3 / 2,
    D: 0,
  }[direction];
}

class KeyState {
  constructor() {
    this.pressedKeys = [];
  }

  keyDown(key) {
    if (this.pressedKeys.includes(key)) {
      return false;
    }
    this.pressedKeys.push(key);
    return true;
  }

  keyUp(key) {
    const index = this.pressedKeys.indexOf(key);
    if (index === -1) {
      return false;
    }
    this.pressedKeys.splice(index, 1);
    return true;
  }

  getLastKey() {
    return this.pressedKeys.length === 0
      ? ''
      : this.pressedKeys[this.pressedKeys.length - 1];
  }
}

class GameState {
  constructor(scene) {
    this.players = {};
    this.scene = scene;
    this.started = false;
    this.stateLoaded = false;
    this.playersLoaded = false;
    this.updateInProgress = false;
    this.playersSyncInProgress = false;
    this.ticks = 0;
    this.posUpdateInterval = 5;
    this.playersUpdateInterval = 50;
    this.keyState = new KeyState();
    this.currentState = {players: {}, lostObjects: {}};
    this.desiredState = {players: {}, lostObjects: {}};
    this.stateTime = performance.now();
    this.requestInstantUpdate = false;
    this.lostObjects = {};

    this._updateState(() => {
      this.stateLoaded = true;
      this._startGame();
    });

    this._syncPlayers(() => {
      this.playersLoaded = true;
      this._startGame();
    });
  }

  tick() {
    this.ticks += 1;

    if (!this.started) {
      return;
    }

    const shouldUpdate = this.ticks % this.posUpdateInterval === 0 || this.requestInstantUpdate;
    if (shouldUpdate && !this.updateInProgress) {
      this.requestInstantUpdate = false;
      this._updateState(() => {
        this._applyDesiredState();
      });
    }

    if (this.ticks % this.playersUpdateInterval === 0 && !this.playersSyncInProgress) {
      this._syncPlayers(() => {});
    }

    this._interpolateState();
    this._instantApplyState();
  }

  keyUp(key) {
    if (this.keyState.keyUp(key)) {
      this._pressKey(this.keyState.getLastKey(), () => {});
    }
  }

  keyDown(key) {
    if (this.keyState.keyDown(key)) {
      this._pressKey(this.keyState.getLastKey(), () => {});
    }
  }

  _pressKey(keys, then) {
    $.post({
      url: '/api/v1/game/player/action',
      dataType: 'json',
      contentType: 'application/json',
      data: JSON.stringify({move: keys}),
      beforeSend: (xhr) => {
        xhr.setRequestHeader('Authorization', `Bearer ${Cookies.get('authToken')}`);
      },
    }).done(() => {
      this.requestInstantUpdate = true;
      then();
    }).fail((xhr) => {
      console.error('Player action failed', xhr.status, xhr.responseText);
    });
  }

  _startGame() {
    if (this.started || !this.stateLoaded || !this.playersLoaded) {
      return;
    }

    this.started = true;
    this._applyDesiredState();
    this._instantApplyState();

    const currentPlayer = this.desiredState.players?.[playerId];
    if (currentPlayer) {
      moveCameraTo(currentPlayer.pos[0], currentPlayer.pos[1]);
    }
  }

  _interpolateState() {
    const now = performance.now();
    const delta = now - this.stateTime;
    const speedMultiplier = 1 / 1000;

    Object.entries(this.currentState.players ?? {}).forEach(([, player]) => {
      player.pos = vecSum(player.pos, vecMul(player.speed ?? [0, 0], delta * speedMultiplier));
      this._movePlayersLoot(player, delta);
    });

    this.stateTime = now;
  }

  _syncPlayers(then) {
    this.playersSyncInProgress = true;

    $.get({
      url: '/api/v1/game/players',
      dataType: 'json',
      beforeSend: (xhr) => {
        xhr.setRequestHeader('Authorization', `Bearer ${Cookies.get('authToken')}`);
      },
    }).done((players) => {
      this._updatePlayersList(players ?? {});
      then();
    }).fail((xhr) => {
      console.error('Players sync failed', xhr.status, xhr.responseText);
    }).always(() => {
      this.playersSyncInProgress = false;
    });
  }

  _updatePlayersList(players) {
    const activeIds = new Set(Object.keys(players));

    Object.entries(players).forEach(([id, playerData]) => {
      if (!this.players[id]) {
        this._addPlayer(id, playerData);
      } else {
        this.players[id].data = this.players[id].data ?? playerData ?? {};
      }
    });

    Object.keys(this.players).forEach((id) => {
      if (!activeIds.has(id)) {
        this._deletePlayer(id);
      }
    });
  }

  _addPlayer(id, playerData) {
    this.players[id] = {
      object: addActor(this.scene),
      data: playerData ?? {},
    };
    this.players[id].data.bag = this.players[id].data.bag ?? {};
  }

  _deletePlayer(id) {
    const player = this.players[id];
    if (player?.object?.object) {
      this.scene.remove(player.object.object);
    }
    delete this.players[id];
  }

  _movePlayerTo(id, playerState) {
    const player = this.players[id];
    if (!player) {
      return;
    }

    moveActor(
      player.object,
      playerState.pos[0],
      playerState.pos[1],
      playerState.converted_dir,
    );
    setAnimation(
      player.object,
      (playerState.speed?.[0] ?? 0) !== 0 || (playerState.speed?.[1] ?? 0) !== 0,
    );

    if (id === playerId) {
      moveCameraTo(playerState.pos[0], playerState.pos[1]);
    }
  }

  _instantApplyState() {
    Object.entries(this.currentState.players ?? {}).forEach(([id, playerState]) => {
      this._movePlayerTo(id, playerState);
    });
  }

  _createLoot(data) {
    const type = loadedLootTypes[data?.type];
    if (!type?.obj) {
      return undefined;
    }

    const object = THREE.SkeletonUtils.clone(type.obj);
    this.scene.add(object);
    return {object, data};
  }

  _processLostObjects(objects = {}) {
    const now = performance.now();

    Object.entries(objects).forEach(([id, data]) => {
      if (!this.lostObjects[id]) {
        const loot = this._createLoot(data);
        if (loot) {
          this.lostObjects[id] = loot;
        }
      }

      const loot = this.lostObjects[id];
      if (!loot) {
        return;
      }

      const y = roadH + 0.3 + 0.2 * Math.sin(now * lootWavingSpeed);
      loot.object.position.set(data.pos[0] + 0.5, y, data.pos[1] + 0.5);
      loot.object.rotation.y = now * lootRotationSpeed;
    });

    const abandonedLoot = {};
    Object.keys(this.lostObjects).forEach((id) => {
      if (!objects[id]) {
        abandonedLoot[id] = this.lostObjects[id];
        delete this.lostObjects[id];
      }
    });

    return abandonedLoot;
  }

  _updateState(then) {
    this.updateInProgress = true;

    $.get({
      url: '/api/v1/game/state',
      dataType: 'json',
      beforeSend: (xhr) => {
        xhr.setRequestHeader('Authorization', `Bearer ${Cookies.get('authToken')}`);
      },
    }).done((state) => {
      this.desiredState = state ?? {players: {}, lostObjects: {}};
      this.desiredState.players = this.desiredState.players ?? {};
      this.desiredState.lostObjects = this.desiredState.lostObjects ?? {};
      this.stateTime = performance.now();
      then();
    }).fail((xhr) => {
      console.error('Game state update failed', xhr.status, xhr.responseText);
    }).always(() => {
      this.updateInProgress = false;
    });
  }

  _interpolateRotation(oldPosition, newPosition) {
    const pi = Math.PI;
    const rotationSpeed = pi / 300;
    const newDirection = convDirection(newPosition.dir);

    if (oldPosition.desired_dir !== newDirection) {
      newPosition.converted_dir = oldPosition.converted_dir;
      newPosition.base_dir = oldPosition.converted_dir;
      newPosition.desired_dir = newDirection;
      newPosition.dir_time = performance.now();
      return;
    }

    newPosition.base_dir = oldPosition.base_dir;
    newPosition.desired_dir = oldPosition.desired_dir;
    newPosition.dir_time = oldPosition.dir_time;

    const delta = performance.now() - newPosition.dir_time;
    const rotationDelta = newPosition.desired_dir - newPosition.base_dir;
    const rotationDirection = rotationDelta <= -pi || (rotationDelta >= 0 && rotationDelta < pi)
      ? 1
      : -1;
    const absoluteDelta = Math.abs(rotationDelta);
    const realRotationDelta = absoluteDelta >= pi ? 2 * pi - absoluteDelta : absoluteDelta;

    newPosition.converted_dir = delta * rotationSpeed > realRotationDelta
      ? oldPosition.desired_dir
      : oldPosition.base_dir + rotationDirection * delta * rotationSpeed;
  }

  _interpolatePosition(currentPosition, newPosition) {
    const speed = vecNorm(newPosition.speed ?? [0, 0]);
    const distance = vecNorm(vecSub(currentPosition.pos, newPosition.pos));

    if (speed < 0.1 || distance > 0.3) {
      return;
    }

    const destinationTime = 0.1;
    const destination = vecSum(newPosition.pos, vecMul(newPosition.speed, destinationTime));
    newPosition.pos = [...currentPosition.pos];
    newPosition.speed = vecMul(vecSub(destination, currentPosition.pos), 1 / destinationTime);
  }

  _applyDesiredState() {
    const oldPlayers = this.currentState.players ?? {};
    const newPlayers = {};
    const updateTime = performance.now();
    const abandonedLoot = this._processLostObjects(this.desiredState.lostObjects);

    Object.entries(this.desiredState.players ?? {}).forEach(([id, playerState]) => {
      const newState = {
        ...playerState,
        pos: [...playerState.pos],
        speed: [...(playerState.speed ?? [0, 0])],
      };

      if (oldPlayers[id]) {
        this._interpolateRotation(oldPlayers[id], newState);
        this._interpolatePosition(oldPlayers[id], newState);
        newState.data = oldPlayers[id].data ?? {bag: {}};
      } else {
        const direction = convDirection(newState.dir);
        newState.converted_dir = direction;
        newState.desired_dir = direction;
        newState.base_dir = direction;
        newState.dir_time = performance.now();
        newState.data = {bag: {}};
      }

      newState.bag = Array.isArray(newState.bag) ? newState.bag : [];
      newState.data.bag = newState.data.bag ?? {};
      newPlayers[id] = newState;
    });

    this.currentState.update_time = updateTime;
    this.currentState.players = newPlayers;
    this.currentState.lostObjects = this.desiredState.lostObjects;
    this._attachLootToPlayers(abandonedLoot);

    Object.values(abandonedLoot).forEach((loot) => {
      if (loot?.object) {
        this.scene.remove(loot.object);
      }
    });
  }

  _movePlayersLoot(player, delta) {
    const bag = Array.isArray(player.bag) ? player.bag : [];
    player.data = player.data ?? {bag: {}};
    player.data.bag = player.data.bag ?? {};

    const lootSpacing = 0.4;
    const shiftX = -Math.sin(player.converted_dir) * lootSpacing;
    const shiftZ = -Math.cos(player.converted_dir) * lootSpacing;
    const shiftY = 0.2 * Math.sin(this.stateTime * lootWavingSpeed);

    bag.forEach((element, index) => {
      const loot = player.data.bag[element.id];
      if (!loot?.mover) {
        return;
      }

      loot.mover.setTarget([
        player.pos[0] + 0.5 + shiftX * index,
        1.5 + shiftY,
        player.pos[1] + 0.5 + shiftZ * index,
      ]);
      loot.mover.doMove(delta);
      loot.object.position.set(...loot.mover.pos);
    });
  }

  _attachLootToPlayers(abandonedLoot) {
    Object.values(this.currentState.players ?? {}).forEach((player) => {
      player.data = player.data ?? {bag: {}};
      player.data.bag = player.data.bag ?? {};
      const bag = Array.isArray(player.bag) ? player.bag : [];
      const activeLootIds = new Set(bag.map((item) => String(item.id)));

      Object.keys(player.data.bag).forEach((lootId) => {
        if (!activeLootIds.has(String(lootId))) {
          const loot = player.data.bag[lootId];
          if (loot?.object) {
            this.scene.remove(loot.object);
          }
          delete player.data.bag[lootId];
        }
      });

      bag.forEach((element) => {
        const lootId = element.id;
        if (player.data.bag[lootId]) {
          return;
        }

        let loot = abandonedLoot[lootId];
        if (loot) {
          delete abandonedLoot[lootId];
        } else {
          loot = this._createLoot(element);
        }

        if (!loot) {
          return;
        }

        loot.object.position.set(player.pos[0] + 0.5, roadH + 0.3, player.pos[1] + 0.5);
        const position = loot.object.position;
        loot.mover = new MovingObject(0.01, [position.x, position.y, position.z]);
        player.data.bag[lootId] = loot;
      });
    });
  }
}

const roadEdgeMaterial = new THREE.MeshBasicMaterial({color: 0x5f5f5f});

function makeRoadTile(tile) {
  const edgeCount = ['u', 'r', 'd', 'l'].reduce((count, key) => count + (tile[key] ? 1 : 0), 0);
  let top = roadEdgeMaterial;
  let rotation = 0;

  switch (edgeCount) {
    case 1: {
      const position = (tile.r ? 1 : 0) + (tile.d ? 2 : 0) + (tile.l ? 3 : 0);
      top = loadedTiles.t;
      rotation = position * 90;
      break;
    }
    case 2:
      if (tile.r && tile.l) {
        top = loadedTiles.v;
        rotation = 90;
      } else if (tile.u && tile.d) {
        top = loadedTiles.v;
      } else {
        const position = (tile.r && tile.d ? 3 : 0)
          + (tile.d && tile.l ? 2 : 0)
          + (tile.l && tile.u ? 1 : 0);
        top = loadedTiles.tr;
        rotation = position * 90;
      }
      break;
    case 3: {
      const position = (!tile.r ? 1 : 0) + (!tile.d ? 2 : 0) + (!tile.l ? 3 : 0);
      top = loadedTiles.nt;
      rotation = position * 90;
      break;
    }
    case 4:
      top = loadedTiles.vh;
      break;
    default:
      break;
  }

  const materials = [roadEdgeMaterial, roadEdgeMaterial, top, roadEdgeMaterial, roadEdgeMaterial, roadEdgeMaterial];
  const geometry = new THREE.BoxBufferGeometry(1, roadH, 1);
  const mesh = new THREE.Mesh(geometry, materials);
  mesh.rotation.y = rotation / 360 * 2 * Math.PI;
  mesh.receiveShadow = true;
  return mesh;
}

function makeRoads(scene) {
  for (let y = ymin; y <= ymax; y += 1) {
    for (let x = xmin; x <= xmax; x += 1) {
      const tile = map_tiles[y - ymin][x - xmin];
      if (!tile.road) {
        continue;
      }

      const mesh = makeRoadTile(tile);
      mesh.position.set(x + 0.5, roadH / 2, y + 0.5);
      scene.add(mesh);
    }
  }
}

function makeBuildings(scene) {
  const buildingHeight = 3;
  const materials = ['#f88', '#ff8', '#f8f', '#8f8', '#88f', '#888']
    .map((color) => new THREE.MeshPhongMaterial({color}));

  for (const building of map.buildings ?? []) {
    const geometry = new THREE.BoxBufferGeometry(building.w, buildingHeight, building.h);
    const mesh = new THREE.Mesh(geometry, materials);
    mesh.position.set(
      building.x + building.w / 2,
      buildingHeight / 2,
      building.y + building.h / 2,
    );
    scene.add(mesh);
  }
}

function loadActor(then) {
  const loader = new THREE.FBXLoader();
  loader.load(
    'assets/pug.fbx',
    then,
    undefined,
    (error) => console.error('Failed to load actor', error),
  );
}

function getRandomInt(max) {
  return Math.floor(Math.random() * max);
}

function getRandomColor(max) {
  const red = getRandomInt(max);
  const green = getRandomInt(max);
  const blue = getRandomInt(max);
  return red * 256 * 256 + green * 256 + blue;
}

function animate(actor) {
  const mixer = new THREE.AnimationMixer(actor.object);
  mixers.push(mixer);
  actor.action = mixer.clipAction(loadedActor.animations[0]);
  actor.moving = false;
}

function addActor(scene) {
  const actor = {
    object: THREE.SkeletonUtils.clone(loadedActor),
    moving: false,
  };
  actor.object.visible = false;
  actor.object.scale.set(0.002, 0.002, 0.002);

  animate(actor);
  paintObject(actor.object, getRandomColor(127));
  scene.add(actor.object);
  return actor;
}

function moveCameraTo(x, y) {
  if (!camera || !cameraControls) {
    return;
  }

  const targetX = x + 0.5;
  const targetZ = y + 0.5;
  const deltaX = targetX - cameraControls.target.x;
  const deltaZ = targetZ - cameraControls.target.z;

  camera.position.x += deltaX;
  camera.position.z += deltaZ;
  cameraControls.target.set(targetX, roadH, targetZ);
  cameraControls.update();
}

function moveActor(actor, x, y, direction) {
  actor.object.visible = true;
  actor.object.position.set(x + 0.5, roadH, y + 0.5);
  actor.object.rotation.y = direction;
}

function setAnimation(actor, moving) {
  if (actor.moving === moving) {
    return;
  }

  if (moving) {
    actor.action.play();
  } else {
    actor.action.stop();
  }
  actor.moving = moving;
}

function isReady() {
  return loadedActor !== undefined && lootTypesLoaded;
}

function setupKeyEvents(gameState) {
  const keyMap = {
    38: 'U',
    39: 'R',
    40: 'D',
    37: 'L',
  };

  $(document).keydown((event) => {
    const key = keyMap[event.which];
    if (key) {
      event.preventDefault();
      gameState.keyDown(key);
    }
  });

  $(document).keyup((event) => {
    const key = keyMap[event.which];
    if (key) {
      event.preventDefault();
      gameState.keyUp(key);
    }
  });
}

function gameserverMain() {
  playerId = Cookies.get('playerId');
  loadRoadTiles();

  const renderer = new THREE.WebGLRenderer({antialias: true});
  renderer.shadowMap.enabled = true;
  renderer.setPixelRatio(window.devicePixelRatio);
  renderer.setSize(window.innerWidth, window.innerHeight);
  const canvas = renderer.domElement;
  document.body.appendChild(canvas);

  loadActor((object) => {
    loadedActor = object;
  });
  loadLootTypes(() => {});

  camera = new THREE.PerspectiveCamera(45, 2, 0.1, 2000);
  camera.position.set(6.7, 8.5, 6.7);

  cameraControls = new THREE.OrbitControls(camera, canvas);
  cameraControls.target.set(0, roadH, 0);
  cameraControls.enabled = true;
  cameraControls.enableRotate = true;
  cameraControls.enableZoom = true;
  cameraControls.enablePan = false;
  cameraControls.enableDamping = false;
  cameraControls.minDistance = 3;
  cameraControls.maxDistance = 100;
  cameraControls.maxPolarAngle = Math.PI / 2 - 0.05;
  cameraControls.mouseButtons.LEFT = THREE.MOUSE.ROTATE;
  cameraControls.mouseButtons.MIDDLE = THREE.MOUSE.DOLLY;
  cameraControls.update();

  const scene = new THREE.Scene();
  scene.background = new THREE.Color('#DEFEFF');
  globalScene = scene;
  const clock = new THREE.Clock();

  const width = xmax - xmin + 1;
  const height = ymax - ymin + 1;
  const basementGeometry = new THREE.BoxBufferGeometry(width, 0.5, height);
  const basementMaterial = new THREE.MeshPhongMaterial({color: '#8AC'});
  const basement = new THREE.Mesh(basementGeometry, basementMaterial);
  basement.receiveShadow = true;
  basement.position.set(xmin + width / 2, 0.25, ymin + height / 2);
  scene.add(basement);

  scene.add(new THREE.HemisphereLight(0xB1E1FF, 0xB97A20, 1));
  makeRoads(scene);
  makeBuildings(scene);

  const light = new THREE.DirectionalLight(0xFFFFFF, 1);
  light.castShadow = true;
  light.position.set(-2.5, 8, -8.5);
  light.target.position.set(-5.5, 0.4, -45);
  light.shadow.bias = -0.004;
  light.shadow.mapSize.width = 2048;
  light.shadow.mapSize.height = 2048;
  light.shadow.camera.near = 0.01;
  light.shadow.camera.far = 200;
  light.shadow.camera.left = -50;
  light.shadow.camera.right = 50;
  light.shadow.camera.top = 50;
  light.shadow.camera.bottom = -50;
  scene.add(light);
  scene.add(light.target);

  function resizeRendererToDisplaySize() {
    const widthNow = canvas.clientWidth;
    const heightNow = canvas.clientHeight;
    const needsResize = canvas.width !== widthNow || canvas.height !== heightNow;
    if (needsResize) {
      renderer.setSize(widthNow, heightNow, false);
    }
    return needsResize;
  }

  let gameState;

  function render() {
    const delta = clock.getDelta();

    if (!gameState && isReady()) {
      gameState = new GameState(scene);
      setupKeyEvents(gameState);
    }

    gameState?.tick();

    if (resizeRendererToDisplaySize()) {
      camera.aspect = canvas.clientWidth / canvas.clientHeight;
      camera.updateProjectionMatrix();
    }

    for (const mixer of mixers) {
      mixer.update(delta * 2);
    }

    renderer.render(scene, camera);
    requestAnimationFrame(render);
  }

  requestAnimationFrame(render);
}
