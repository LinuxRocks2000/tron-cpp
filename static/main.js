const socket = new WebSocket("ws://" + location.host + "/game");
var gameCanvas = document.getElementById("game");
var ctx = gameCanvas.getContext("2d");


var trackingPlayer = undefined;

colors = ["red", "green", "blue", "yellow", "cyan", "magenta"];


class Player{
    constructor(x, y, id){
        this.x = x;
        this.sX = x;
        this.y = y;
        this.sY = y;
        this.id = id;
    }
}


var players = [];
var lines = [];


socket.addEventListener("message", (event) => {
    let message = event.data;
    let args = message.split(",");
    if (args[0] == "p"){
        players.push(new Player(parseInt(args[1]), parseInt(args[2]), parseInt(args[3])));
    }
    else if (args[0] == "t"){ // track
        players.forEach((item, i) => {
            if (item.id == args[1]){
                trackingPlayer = item;
            }
        });
    }
    else if (args[0] == "s"){
        players.forEach((player, i) => {
            if (player.id == args[1]){
                player.x = parseInt(args[2]);
                player.y = parseInt(args[3]);
            }
        });
    }
    else if (args[0] == "l"){
        lines.push([parseInt(args[1]), parseInt(args[2]), parseInt(args[3]), parseInt(args[4])]);
    }
    else if (args[0] == "die"){
        alert("dead.");
    }
});


function loop(){
    requestAnimationFrame(loop);
    if (!trackingPlayer){ // If it hasn't loaded
        return;
    }
    ctx.fillStyle = "white";
    ctx.fillRect(0, 0, window.innerWidth, window.innerHeight);
    ctx.translate(window.innerWidth/2 - trackingPlayer.x, window.innerHeight/2 - trackingPlayer.y);
    ctx.fillStyle = "blue";
    ctx.fillRect(trackingPlayer.sX - 50, trackingPlayer.sY - 50, 100, 100);
    players.forEach((player, i) => {
        ctx.fillStyle = colors[player.id % colors.length];
        ctx.fillRect(player.x - 10, player.y - 10, 20, 20);
    });
    ctx.beginPath();
    lines.forEach((item, i) => {
        ctx.moveTo(item[0], item[1]);
        ctx.lineTo(item[2], item[3]);
    });
    ctx.closePath();
    ctx.stroke();
    ctx.resetTransform();
}

loop();

function onResize(event){
    gameCanvas.width = window.innerWidth;
    gameCanvas.height = window.innerHeight;
}

window.addEventListener("resize", onResize);
onResize();

window.addEventListener("keydown", (event) => {
    console.log(event.key);
    if (event.key == "ArrowUp"){
        socket.send("u");
    }
    else if (event.key == "ArrowDown"){
        socket.send("d");
    }
    else if (event.key == "ArrowLeft"){
        socket.send("l");
    }
    else if (event.key == "ArrowRight"){
        socket.send("r");
    }
});
