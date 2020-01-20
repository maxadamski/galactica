
let dt = 0;

let angleSpeed = 0.005
let moveSpeed = 0.2
let bulletSpeed = 0.3

let pellets = [];
let bullets = [];
let rocks = [];

let mapSize = 2000
let mapPad = 100
let mapZoom = 2

let starCount = 1000
let stars = []
let messages = []
let ships = []
let myShip = {}

let maxEnergy = 5

let lastGarbageTime = 0

let pelletAngle = 0;
let pelletAngleSpeed = 0.002;

let shieldOn = true;
let shieldOnTime = 0;
let shieldDecay = 1000*3;

let shakeScreenOn = false;
let shakeScreenTime = 0;
let shakeScreenDecay = 1000*0.5;

let gameStartTime = 0;
let gameTime = 1000*60*5;
let gameOver = false;
let gameOverReason = '';

function clip(x, lower, upper) {
    return max(min(x, upper), lower);
}

function polygon(x, y, radius, npoints) {
    let angle = TAU / npoints;
    beginShape();
    for (let a = 0; a < TAU; a += angle) {
        let sx = x + cos(a) * radius;
        let sy = y + sin(a) * radius;
        vertex(sx, sy);
    }
    endShape(CLOSE);
}

function generateRock(size) {
    let v = []
    let a = 0
    let steps = randomGaussian(10, 2)
    while (a < TAU) {
        d = randomGaussian(size/2, 7)
        v.push([d*cos(a), d*sin(a)])
        a += randomGaussian(TAU/steps, TAU*10/360)
    }
    return v
}

function randomRock() {
    phi = random(0, PI);
    size = randomGaussian(60, 10);
    return {
        x: random(0, mapSize),
        y: random(0, mapSize),
        dx: cos(phi),
        dy: sin(phi),
        speed: randomGaussian(0.005, 0.002),
        size: size,
        health: size / 2,
        maxHealth: size / 2,
        active: true,
        vertices: generateRock(size),
        angleSpeed: randomGaussian(0, 0.0001),
        angle: 0,
    };

}

function randomStar() {
    return {
        x: random(-mapPad, mapSize + mapPad),
        y: random(-mapPad, mapSize + mapPad),
        light: random(50, 100),
    }
}

function windowResized() {
    resizeCanvas(windowWidth, windowHeight);

    let p = max(windowWidth, windowHeight);
    let q = windowWidth > windowHeight ? 1920 : 1080;
    mapZoom = 2.5 * p / q
}

function setup() {
    let canvas = createCanvas(windowWidth, windowHeight);
    canvas.parent('game');
    frameRate(60);
    windowResized();
    gameStartTime = millis();
    joinGame();

    messages.push({text: 'max entered the arena [system]'});

    for (let i = 0; i < starCount; i++)
        stars.push(randomStar());

    for (let i = 0; i < 100; i++)
        rocks.push(randomRock());
}

function drawUI() {
    push();
    resetMatrix();
    fill(255);
    stroke(20);
    strokeWeight(2);

    textSize(40);
    textAlign(CENTER);
    textStyle(BOLD);
    text('GALACTICA', windowWidth / 2, 50);

    if (gameOver) {
        textSize(62);
        text('GAME OVER', windowWidth / 2, windowHeight / 2 - 10);
        textSize(24);
        text('PRESS ENTER TO RESPAWN', windowWidth / 2, windowHeight / 2 + 30);
    }

    textSize(32);

    let remTime = gameTime - (millis() - gameStartTime);
    let remMin = floor((remTime / 1000) / 60);
    let remSec = floor((remTime / 1000) % 60);
    text(`${floor(remMin)}:${floor(remSec)}`, windowWidth / 2, 90);

    textAlign(LEFT);
    textSize(24);
    text('STARSHIP STATUS', 20, 35);
    textSize(18);

    text(`SPICE ${myShip.spice}`, 20, 60)
    text(`ENERGY ${myShip.energy}`, 20, 80)
    text(`SHIELD ${myShip.shieldOn ? 'ON' : 'OFF'}`, 20, 100)
    text(`COORD ${round(myShip.x)} X ${round(myShip.y)} `, 20, 120)
    text(`FPS ${round(frameRate())} `, 20, 140)

    textAlign(RIGHT);
    textSize(24);
    text('CAPTAIN\'S LOG', windowWidth - 20, 35);

    textSize(16);
    let i = 0;
    for (let log of messages) {
        text(log.text, windowWidth - 20, 57 + i*18);
    }

    pop();
}

function drawStars() {
    push()
    strokeWeight(1);
    for (let star of stars) {
        stroke(star.light/100*255)
        rect(star.x, star.y, 1, 1);
    }
    pop();
}

function updateShip(ship) {
    ship.shieldOnTime += deltaTime;
    if (ship.shieldOnTime > ship.shieldDecay) {
        ship.shieldOn = false;
    }
}

function enableShield(ship, decay) {
    ship.shieldOn = true;
    ship.shieldOnTime = 0;
    ship.shieldDecay = decay;
}

function joinGame() {
    gameOver = false;
    myShip.x = random(0, mapSize);
    myShip.y = random(0, mapSize);
    myShip.angle = -HALF_PI;
    myShip.energy = maxEnergy;
    myShip.spice = 0;
    enableShield(myShip, 4*1000);
}

function enableScreenShake() {
    shakeScreenOn = true;
    shakeScreenTime = 0;
}

function updateScreenShake() {
    if (!shakeScreenOn) return;
    translate(sin(shakeScreenTime/5), sin(shakeScreenTime/5));
    shakeScreenTime += deltaTime;
    if (shakeScreenTime > shakeScreenDecay) {
        shakeScreenOn = false;
        shakeScreenTime = 0;
    }
}

function didGameOver() {
    gameOver = true;
}

function shipCollided(ship) {
    enableScreenShake();
    ship.energy -= 5;
    if (ship.energy < 0) {
        gameOverReason = 'asteroid'
        didGameOver();
    } else {
        enableShield(ship, 1000*2);
    }
}

function shipGotShot(ship) {
    enableScreenShake();
    ship.energy -= 1;
    if (ship.energy < 0) {
        gameOverReason = 'player'
        ship.didGameOver();
    } else {
        enableShield(ship, 1000*0.5);
    }
}

function drawShip(ship) {
    push();
    translate(ship.x, ship.y)

    if (ship === myShip && shakeScreenOn) {
        stroke(0, 100 * (1 - shakeScreenTime / shakeScreenDecay), 100)
    }

    if (ship.shieldOn) {
        push();
        rotate(pelletAngle);
        polygon(0, 0, 30, 6);
        pop();
    }

    if (ship !== myShip || !shakeScreenOn) {
        stroke(0, 100*(1 - ship.energy/maxEnergy), 100)
    }

    rotate(ship.angle)
    triangle(-6, -5, -6, 5, 6, 0);
    pop();
}

function drawRock(rock) {
    push();
    stroke(0, 100 - 100 * rock.health/rock.maxHealth, 100);
    translate(rock.x, rock.y);
    rotate(rock.angle);
    beginShape();
    for (let v of rock.vertices)
        vertex(v[0], v[1]);
    endShape(CLOSE);
    pop();
}

function drawPellet(pellet) {
    push();
    translate(pellet.x, pellet.y);
    rotate(pelletAngle);
    H = 20;
    if (pellet.fuel) {
        rect(-2, -2, 4, 4);
    } else {
        triangle(-1, -1, 1, -1, 0, 0.73);
    }
    pop();
}

function collidePellet(pellet) {
    if (dist(myShip.x, myShip.y, pellet.x, pellet.y) < 8) {
        pellet.active = false;
        if (pellet.fuel) {
            myShip.energy += 5;
        } else {
            myShip.spice += 10;
        }
    }
}

function generateSpice(rock) {
    count = randomGaussian(rock.size/5, 2); 
    for (let i = 0; i < count; i++) {
        px = randomGaussian(rock.x, rock.size/4);
        if (px < 0 || px > mapSize) continue;
        py = randomGaussian(rock.y, rock.size/4);
        if (py < 0 || py > mapSize) continue;
        fuel = random(1000) < 5;
        pellets.push({x: px, y: py, fuel: fuel, active: true});
    }
}

function collideBullet(bullet) {
    if (bullet.x < 0 || bullet.x > mapSize || bullet.y < 0 || bullet.y > mapSize) {
        bullet.active = false;
        return;
    }

    //if (!shieldOn && bullet.player != playerId && dist(x, y, bullet.x, bullet.y) < 0) shipGotShot();

    for (let rock of rocks) {
        if (!rock.active) continue;
        if (dist(bullet.x, bullet.y, rock.x, rock.y) < rock.size/2) {
            bullet.active = false;
            rock.health -= 10;
            if (rock.health <= 0) {
                rock.active = false;
                generateSpice(rock);
            }
        }
    }
}

function collideRock(rock) {
    if (rock.x < -mapPad || rock.x > mapSize+mapPad || rock.y < mapPad || rock.y > mapSize+mapPad) {
        rock.active = false;
        return;
    }

    if (!gameOver && !myShip.shieldOn && dist(myShip.x, myShip.y, rock.x, rock.y) < rock.size/2 - 2) {
        shipCollided(myShip);
    }
}

function collectGarbage() {
    let sec = second()
    if (sec == lastGarbageTime || sec % 2 != 0) return;
    lastGarbageTime = sec

    bullets = bullets.filter(x => x.active);
    rocks = rocks.filter(x => x.active);
    pellets = pellets.filter(x => x.active);
}

function processMovement() {
    let ship = myShip
    if (keyIsDown(LEFT_ARROW)) {
        ship.angle -= dt * angleSpeed;
    }
    if (keyIsDown(RIGHT_ARROW)) {
        ship.angle += dt * angleSpeed;
    }
    if (keyIsDown(UP_ARROW)) {
        ship.x += dt * moveSpeed * cos(ship.angle);
        ship.y += dt * moveSpeed * sin(ship.angle);
    }
    if (keyIsDown(DOWN_ARROW)) {
        ship.x -= dt * moveSpeed * cos(ship.angle);
        ship.y -= dt * moveSpeed * sin(ship.angle);
    }
    ship.x = clip(ship.x, 0, mapSize);
    ship.y = clip(ship.y, 0, mapSize);
}

function drawRocks() {
    for (let rock of rocks) {
        if (!rock.active) continue;
        rock.angle += dt * rock.angleSpeed;
        rock.x += dt * rock.speed * rock.dx;
        rock.y += dt * rock.speed * rock.dy;
        drawRock(rock);
        collideRock(rock);
    }
}

function drawPellets() {
    pelletAngle += dt * pelletAngleSpeed;
    for (let pellet of pellets) {
        if (!pellet.active) continue;
        drawPellet(pellet);
        collidePellet(pellet);
    }
}

function drawBullets() {
    push();
    fill(0);
    bulletDecay = 1000 * 1
    for (let bullet of bullets) {
        if (bullet.time > bulletDecay) bullet.active = false;
        if (!bullet.active) continue;

        stroke(100 + 20*log(1 - bullet.time / bulletDecay));
        circle(bullet.x, bullet.y, 1);
        bullet.x += dt * bulletSpeed * bullet.dx;
        bullet.y += dt * bulletSpeed * bullet.dy;
        bullet.time += dt
        collideBullet(bullet);
    }
    pop();
}

function drawEnemyShips() {
    for (let ship of ships) {
        drawShip(ship);
    }
}

function draw() {
    dt = deltaTime;

    colorMode(HSB, 100);
    background(0);
    noFill();
    strokeWeight(2);
    strokeCap(SQUARE);
    stroke(255);

    if (!gameOver) {
        processMovement();
    }

    scale(mapZoom);
    let cx = windowWidth/2/mapZoom
    let cy = windowHeight/2/mapZoom
    translate(
        -clip(myShip.x-cx, -mapPad, mapSize+mapPad-2*cx),
        -clip(myShip.y-cy, -mapPad, mapSize+mapPad-2*cy));

    updateScreenShake();

    rect(0, 0, mapSize, mapSize);
    drawStars();

    drawPellets();
    drawBullets();
    drawRocks();
    drawEnemyShips();

    if (!gameOver) {
        updateShip(myShip);
        drawShip(myShip);
    }

    drawUI();

    collectGarbage();
}

function keyPressed() {
    if (keyCode == 32) { // space
        bullets.push({x: myShip.x, y: myShip.y, dx: cos(myShip.angle), dy: sin(myShip.angle), time: 0, active: true});
    }
    if (keyCode == 13) { // enter
        joinGame();
    }
}

