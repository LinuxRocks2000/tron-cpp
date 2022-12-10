#define CROW_ENFORCE_WS_SPEC
#include <crow.h>
#include <pthread.h> // Technically std::threads have cool features like lambdas, but I honestly prefer C threads
#include <iostream>
#include <unistd.h>
#define FPS   55
#define SPEED 1


struct Line{
    long x1;
    long x2;
    long y1;
    long y2;

    Line(long _x1, long _y1, long _x2, long _y2){
        x1 = _x1;
        y1 = _y1;
        x2 = _x2;
        y2 = _y2;
    }
};


struct Player{
    long x;
    long y;
    unsigned long id;
    char direction = 0; // 0 = not moving (only at the start can this happen), 'u' = up, 'd' = down, 'l' = left, 'r' = right
    long sX;
    long sY;
    long originX;
    long originY;
    Line* lastPlaced;
    Player(long _x, long _y, long _id){
        x = _x;
        y = _y;
        sX = _x;
        sY = _y;
        id = _id;
        originX = _x;
        originY = _y;
    }

    void upd8(){
        int mX = 0;
        int mY = 0;
        switch (direction){
            case 'u':
                mY = -1;
                break;
            case 'd':
                mY = 1;
                break;
            case 'l':
                mX = -1;
                break;
            case 'r':
                mX = 1;
                break;
        }
        mX *= SPEED;
        mY *= SPEED;
        x += mX;
        y += mY;
    }
};


unsigned long topID = 0;


std::vector<Line*> lines;
std::vector<crow::websocket::connection*> connections;
std::map<crow::websocket::connection*, Player*> players;

void broadcastToAll(std::string data){
    for (const auto& pair : players){
        pair.first -> send_text(data); // set
    }
}


bool isCollide(Player* p){
    bool inLast = false;
    long box2[4] = {
        p -> x - 10,
        p -> y - 10,
        p -> x + 10,
        p -> y + 10
    };
    long safeBox[4] = {
        p -> originX - 50,
        p -> originY - 50,
        p -> originX + 50,
        p -> originY + 50
    };
    if (box2[2] > safeBox[0] && box2[0] < safeBox[2] && box2[3] > safeBox[1] && box2[1] < safeBox[3]){ // If you're "safe" (in the origin cube), don't let you get hurt
        return false;
    }
    for (Line* l : lines){
        long box1[4] = {
            l -> x1,
            l -> y1,
            l -> x2,
            l -> y2
        };
        if (box1[2] > box2[0] && box1[0] < box2[2] && box1[3] > box2[1] && box1[1] < box2[3]){ // Since all lines will be horizontal or vertical, but not diagonal, we can just treat them as rectangles and do the classic Rectangle Collision Test
            if (l == p -> lastPlaced){
                inLast = true;
                continue;
            }
            return true;
        }
    }
    if (!inLast){
        p -> lastPlaced = 0;
    }
    return false;
}


void sendLineToConnection(Line* line, crow::websocket::connection* conn){
    conn -> send_text("l," + std::to_string(line -> x1) + "," + std::to_string(line -> y1) + "," + std::to_string(line -> x2) + "," + std::to_string(line -> y2));
}


void sendLineToAll(Line* line){
    for (crow::websocket::connection* conn : connections){
        sendLineToConnection(line, conn);
    }
}


void sendLinesTo(crow::websocket::connection* conn){
    for (Line* l : lines){
        sendLineToConnection(l, conn);
    }
}


void sendPlayerTo(Player* p, crow::websocket::connection* conn){
    conn -> send_text("p," + std::to_string(p -> x) + "," + std::to_string(p -> y) + "," + std::to_string(p -> id));
}


void sendPlayersTo(crow::websocket::connection* conn){
    for (const auto& pair : players){
        Player* p = pair.second;
        sendPlayerTo(p, conn);
    }
}


void sendPlayerToAll(Player* p){
    for (crow::websocket::connection* conn : connections){
        sendPlayerTo(p, conn);
    }
}


void fixBox(Line* line){
    long oldX1 = line -> x1;
    long oldY1 = line -> y1;
    long oldX2 = line -> x2;
    long oldY2 = line -> y2;
    if (line -> x1 > line -> x2){
        line -> x2 = oldX1;
        line -> x1 = oldX2;
    }
    if (line -> y1 > line -> y2){
        line -> y2 = oldY1;
        line -> y1 = oldY2;
    }
}


void drawLine(Player* player){
    if (player -> direction == 0){
        return;
    }
    Line* l = new Line (player -> sX, player -> sY, player -> x, player -> y);
    sendLineToAll(l);
    lines.push_back(l);
    player -> sX = player -> x;
    player -> sY = player -> y;
    player -> lastPlaced = l;
    fixBox(l);
}


void disconnect(crow::websocket::connection* conn){
    delete players[conn]; // Free memory
    players.erase(conn); // Remove player from play
    for (int i = 0; i < connections.size(); i ++){
        if (connections[i] == conn){
            connections.erase(connections.begin() + i /* I hate this syntax, but appear to have no choice */);
            break;
        }
    }
}


void murder(Player* player){
    for (const auto& pair : players){
        if (pair.second == player){
            pair.first -> send_text("die");
            //pair.first -> close();
            disconnect(pair.first);
            break;
        }
    }
}


void* mainloop(void* _){
    while (true){
        std::vector<Player*> toKill;
        for (const auto& pair : players){
            pair.second -> upd8();
            Player* p = pair.second;
            broadcastToAll("s," + std::to_string(p -> id) + "," + std::to_string(p -> x) + "," + std::to_string(p -> y));
            if (isCollide(p)){
                toKill.push_back(p);
            }
        }
        for (Player* player : toKill){
            murder(player);
        }
        usleep(1000000/FPS);
    }
    return NULL; // hehe
}


int main(){
    crow::SimpleApp webserver;
    CROW_ROUTE(webserver, "/")([](const crow::request& req, crow::response& res){
        res.set_static_file_info("static/index.html");
        res.end();
    });
    CROW_ROUTE(webserver, "/game").websocket().
    onopen([](crow::websocket::connection& _conn){
        crow::websocket::connection* conn = &_conn;
        sendPlayersTo(conn);
        players[conn] = new Player (100, 100, topID);
        connections.push_back(conn);
        sendPlayerToAll(players[conn]);
        sendLinesTo(conn);
        _conn.send_text("t," + std::to_string(topID));
        topID ++;
    }).
    onclose([](crow::websocket::connection& _conn, std::string reason){
        crow::websocket::connection* conn = &_conn;
        disconnect(conn);
    }).
    onmessage([](crow::websocket::connection& _conn, std::string data, bool is_binary){
        crow::websocket::connection* conn = &_conn;
        Player* player = players[conn];
        if (player -> direction == data[0]){
            return;
        }
        if (data[0] == 'u'){ // It's a very simple game; there are only 4 valid messages it can send
            if (player -> direction != 'd'){ // If it's going left, right, or not moving (technically you can hit u twice and it's valid)
                drawLine(player);
                player -> direction = 'u';
            }
        }
        else if (data[0] == 'd'){
            if (player -> direction != 'u'){ // If it's going left, right, or not moving (technically you can hit d twice and it's valid)
                drawLine(player);
                player -> direction = 'd';
            }
        }
        else if (data[0] == 'l'){
            if (player -> direction != 'r'){
                drawLine(player);
                player -> direction = 'l';
            }
        }
        else if (data[0] == 'r'){
            if (player -> direction != 'l'){
                drawLine(player);
                player -> direction = 'r';
            }
        }
    });
    pthread_t mainloopThread;
    pthread_create(&mainloopThread, NULL, &mainloop, NULL);
    pthread_detach(mainloopThread);
    webserver.port(8080).multithreaded().run();
    return 8675309; // Look, if you don't know don't ask, just go get a life.
}
