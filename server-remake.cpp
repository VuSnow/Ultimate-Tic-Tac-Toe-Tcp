#include "server-remake.h"

bool checkWinBoard(board checkBoard){
    if(checkBoard.cells[0] != " " && checkBoard.cells[0] == checkBoard.cells[1] && checkBoard.cells[1] == checkBoard.cells[2]){
        return true;
    }
    if(checkBoard.cells[3] != " " && checkBoard.cells[3] == checkBoard.cells[4] && checkBoard.cells[4] == checkBoard.cells[5]){
        return true;
    }
    if(checkBoard.cells[6] != " " && checkBoard.cells[6] == checkBoard.cells[7] && checkBoard.cells[7] == checkBoard.cells[8]){
        return true;
    }
    if(checkBoard.cells[0] != " " && checkBoard.cells[0] == checkBoard.cells[4] && checkBoard.cells[4] == checkBoard.cells[8]){
        return true;
    }
    if(checkBoard.cells[2] != " " && checkBoard.cells[2] == checkBoard.cells[4] && checkBoard.cells[4] == checkBoard.cells[6]){
        return true;
    }
    if(checkBoard.cells[0] != " " && checkBoard.cells[0] == checkBoard.cells[3] && checkBoard.cells[3] == checkBoard.cells[6]){
        return true;
    }
    if(checkBoard.cells[1] != " " && checkBoard.cells[1] == checkBoard.cells[4] && checkBoard.cells[4] == checkBoard.cells[7]){
        return true;
    }
    if(checkBoard.cells[2] != " " && checkBoard.cells[2] == checkBoard.cells[5] && checkBoard.cells[5] == checkBoard.cells[8]){
        return true;
    }
    return false;
}

bool checkWinGame(std::vector<board> bigBoard){
    if(bigBoard[0].owner != " " && bigBoard[0].owner == bigBoard[1].owner && bigBoard[1].owner == bigBoard[2].owner){
        return true;
    }
    if(bigBoard[3].owner != " " && bigBoard[3].owner == bigBoard[4].owner && bigBoard[4].owner == bigBoard[5].owner){
        return true;
    }
    if(bigBoard[6].owner != " " && bigBoard[6].owner == bigBoard[7].owner && bigBoard[7].owner == bigBoard[8].owner){
        return true;
    }
    if(bigBoard[0].owner != " " && bigBoard[0].owner == bigBoard[4].owner && bigBoard[4].owner == bigBoard[8].owner){
        return true;
    }
    if(bigBoard[2].owner != " " && bigBoard[2].owner == bigBoard[4].owner && bigBoard[4].owner == bigBoard[6].owner){
        return true;
    }
    if(bigBoard[0].owner != " " && bigBoard[0].owner == bigBoard[3].owner && bigBoard[3].owner == bigBoard[6].owner){
        return true;
    }
    if(bigBoard[1].owner != " " && bigBoard[1].owner == bigBoard[4].owner && bigBoard[4].owner == bigBoard[7].owner){
        return true;
    }
    if(bigBoard[2].owner != " " && bigBoard[2].owner == bigBoard[5].owner && bigBoard[5].owner == bigBoard[8].owner){
        return true;
    }
    return false;
}

bool checkFullBoard(board checkBoard){
    for(int i = 0; i < 9; i++){
        if(checkBoard.cells[i] == " "){
            return false;
        }
    }
    return true;
}

std::vector<USER> readAccountsFile(){
    std::vector<USER> users;
    std::ifstream accountsFile("accounts.txt");
    if(!accountsFile){
        std::cerr << "Failed to open accounts file." << std::endl;
        return users;
    }

    std::string line;
    while(std::getline(accountsFile, line)){
        std::istringstream iss(line);
        USER user;
        if(iss >> user.username >> user.password >> user.ingame >> user.status >> user.wins >> user.loses >> user.isFree){
            user.elo = user.wins * 10 - user.loses * 5;
            if((user.wins + user.loses) == 0){
                user.winRate = 0;
            }else{
                user.winRate = (double)user.wins / (user.wins + user.loses);
            }
            users.push_back(user);
        }
    }

    return users;
}

void updateAccountsFile(std::vector<USER> users){
    std::ofstream accountsFile("accounts.txt", std::ios::trunc);
    if(!accountsFile){
        std::cerr << "Failed to clear accounts file" << std::endl;
        return;
    }
    for(USER &user : users){
        accountsFile << user.username << " " << user.password << " " << user.ingame << " " << user.status << " " << user.wins << " " << user.loses << " " << user.isFree << std::endl;
    }
    accountsFile.close();
}

USER findUserByUsername(const std::string &username){
    std::vector<USER> users = readAccountsFile();
    for(const USER &user : users){
        if(user.username == username){
            return user;
        }
    }

    return USER();
}

USER findUserByIngame(const std::string &ingame){
    std::vector<USER> users = readAccountsFile();
    for(const USER &user : users){
        if(user.ingame == ingame){
            return user;
        }
    }

    return USER();
}

room findRoomByName(const std::string &room_name){
    for(const room &roomIn4 : roomList){
        if(roomIn4.name == room_name){
            return roomIn4;
        }
    }
    return room();
}

void getOnlinePlayerList(json &data, int client_fd){
    std::vector<USER> users = readAccountsFile();
    json onlineUserJSON;
    json message_sent;

    for(USER user : users){
        if(user.status == "online"){
            json userJSON;
            userJSON["username"] = user.username;
            userJSON["status"] = user.status;
            userJSON["wins"] = user.wins;
            userJSON["losses"] = user.loses;
            userJSON["elo"] = user.elo;
            userJSON["isFree"] = user.isFree;
            userJSON["win rate"] = user.winRate;
            userJSON["ingame"] = user.ingame;

            onlineUserJSON.push_back(userJSON);
        }
    }

    message_sent["type"] = static_cast<int>(ResponseType::GETONLINEUSER);
    message_sent["online user"] = onlineUserJSON;
    message_sent["room list size"] = roomList.size();
    send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
}

void getRoomList(json &data, int client_fd){
    json roomListJSON;
    json message_sent;
    
    for(room roomValue : roomList){
        // std::cout << "size:" << roomValue.players.size() << std::endl;
        json roomJSON;
        roomJSON["room name"] = roomValue.name;
        roomJSON["player X username"] = roomValue.playerX.username;
        if(roomValue.isFull){
            roomJSON["player O username"] = roomValue.playerO.username;
        }
        roomJSON["is full"] = roomValue.isFull;
        roomJSON["player1_ready"] = roomValue.player1_ready;
        roomJSON["player2_ready"] = roomValue.player2_ready;
        roomJSON["next board"] = roomValue.nextBoard;
        roomJSON["game start"] = roomValue.gameStart;
        roomJSON["turn"] = roomValue.turn;
        roomJSON["is player X turn"] = roomValue.isPlayerXTurn;
        roomListJSON.push_back(roomJSON);
    }

    message_sent["type"] = static_cast<int>(ResponseType::GETROOMLIST);
    message_sent["room list"] = roomListJSON;
    send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
}

void sendMsgToMultipleConnections(std::vector<int> clients, json message){
    for(int client: clients){
        send(client, message.dump().c_str(), message.dump().length(), 0);
    }
}

void handleLogin(json &data, int client_fd){
    std::string username = data["username"];
    std::string password = data["password"];
    std::vector<USER> users = readAccountsFile();
    USER user = findUserByUsername(username);
    json message_sent;

    if(!user.username.empty()){
        if(user.username == username && user.password == password){
            if(user.status == "online"){
                message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
                message_sent["message"] = "already login";
            }else{
                // update online users list
                for(USER &user : users){
                    if(user.username == username){
                        user.status = "online";
                        user.socket = client_fd;
                    }
                }
                updateAccountsFile(users);

                // creating message about information of login user
                message_sent["user"]["username"] = user.username;
                message_sent["user"]["wins"] = user.wins;
                message_sent["user"]["loses"] = user.loses;
                message_sent["user"]["winRate"] = user.winRate;
                message_sent["user"]["status"] = "online";
                message_sent["user"]["isFree"] = user.isFree;
                message_sent["user"]["elo"] = user.elo;
                message_sent["user"]["ingame"] = user.ingame;

                for(int client : clients){
                    if(client == client_fd){
                        message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
                        message_sent["message"] = "login success";
                    }else{
                        message_sent["type"] = static_cast<int>(ResponseType::UPDATEONLINEUSER);
                        message_sent["message"] = "add user to online list";
                    }
                    std::string message = message_sent.dump();
                    send(client, message.c_str(), message.length(), 0);
                }
            }
        }
    }
}

void handleRegister(json &data, int client_fd){
    std::string username = data["username"];
    std::string password = data["password"];
    std::string ingame = data["ingame"];

    USER findUser = findUserByUsername(username);
    USER findIngame = findUserByIngame(ingame);
    json message_sent;
    USER newUser;
    std::vector<USER> users = readAccountsFile();

    if(!findUser.username.empty()){
        message_sent["type"] = static_cast<int>(ResponseType::REGISTER);
        message_sent["message"] = "existed username";
    }else if(!findIngame.username.empty()){
        message_sent["type"] = static_cast<int>(ResponseType::REGISTER);
        message_sent["message"] = "existed ingame";
    }else{
        newUser.username = username;
        newUser.password = password;
        newUser.ingame = ingame;
        newUser.wins = 0;
        newUser.loses = 0;
        newUser.elo = 0;
        newUser.isFree = true;
        newUser.status = "offline";
        newUser.winRate = 0;
        users.push_back(newUser);

        updateAccountsFile(users);

        message_sent["type"] = static_cast<int>(ResponseType::REGISTER);
        message_sent["message"] = "register successfully";
    }

    std::string message = message_sent.dump();
    send(client_fd, message.c_str(), message.length(), 0);
}

void handleLogout(json &data, int client_fd){
    std::string username = data["username"];
    std::vector<USER> users = readAccountsFile();
    json message_sent;

    for(USER &user : users){
        if(user.username == username){
            user.status = "offline";
        }
    }
    updateAccountsFile(users);

    auto it = std::find(clients.begin(), clients.end(), client_fd);
    if(it != clients.end()){
        clients.erase(it);
    }

    message_sent["type"] = static_cast<int>(ResponseType::UPDATEONLINEUSER);
    message_sent["message"] = "delete user from online list";
    message_sent["username"] = username;
    sendMsgToMultipleConnections(clients, message_sent);

    // for(int client : clients){
    //     send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0); 
    // }
}

void createRoom(json &data, int client_fd){
    std::string room_name = data["room name"];
    std::string player_X_username = data["player X username"];
    USER player_X = findUserByUsername(player_X_username);
    json message_sent;

    if(findRoomByName(room_name).name.empty()){
        room newRoom;
        newRoom.name = room_name;
        newRoom.playerX = player_X;
        newRoom.turn = 1;
        newRoom.client_in_room.push_back(client_fd);
        roomList.push_back(newRoom);

        std::cout << "new_room nextboard" << newRoom.nextBoard << std::endl;

        message_sent["room name"] = room_name;
        message_sent["type"] = static_cast<int>(ResponseType::CREATEROOM);
        message_sent["message"] = "create success";
        send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
    }else{
        message_sent["type"] = static_cast<int>(ResponseType::CREATEROOM);
        message_sent["message"] = "create fail";
        send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0); 
    }
}

void joinRoom(json &data, int client_fd){
    std::string room_name = data["room name"];
    std::string player_O_username = data["player O username"];
    USER player_O = findUserByUsername(player_O_username);

    if(findRoomByName(room_name).name.empty()){
        json message_sent;
        message_sent["type"] = static_cast<int>(ResponseType::JOINROOM);
        message_sent["message"] = "join fail";
        send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
        return;
    }

    for(room &roomValue : roomList){
        if(roomValue.name == room_name){
            if(roomValue.isFull){
                json message_sent;
                message_sent["type"] = static_cast<int>(ResponseType::JOINROOM);
                message_sent["message"] = "full";
                send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
            }else{
                roomValue.playerO = player_O;
                roomValue.isFull = true;
                roomValue.client_in_room.push_back(client_fd);

                for(int client : clients){
                    json message_sent;
                    message_sent["room name"] = room_name;
                    if(client == client_fd){
                        message_sent["type"] = static_cast<int>(ResponseType::JOINROOM);
                        message_sent["message"] = "join success";
                    }else{
                        message_sent["type"] = static_cast<int>(ResponseType::UPDATEROOMLIST);
                        message_sent["message"] = "add player to room";
                        message_sent["player O username"] = player_O_username;
                    }
                    send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0);
                }
            }
        }
    }
}

void handleReadyRequest(json &data, int client_fd){
    std::string room_name = data["room name"];
    std::string player_username = data["player_username"];

    for(room &value: roomList){
        if(value.name == room_name){
            if(value.playerX.username == player_username){
                value.player1_ready = true;
            }else if(value.playerO.username == player_username){
                value.player2_ready = true;
            }
        }
    }

    json message_sent;
    message_sent["type"] = static_cast<int>(ResponseType::READY);
    message_sent["room name"] = room_name;
    message_sent["player username"] = player_username;
    sendMsgToMultipleConnections(clients, message_sent);
    // for(int client : clients){
    //     send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0);
    // }
}

void handleUnreadyRequest(json &data, int client_fd){
    std::string room_name = data["room_name"];
    std::string player_username = data["player_username"];

    for(room &value : roomList){
        if(value.name == room_name){
            if(value.playerX.username == player_username){
                value.player1_ready = false;
            }else if(value.playerO.username == player_username){
                value.player2_ready = false;
            }
        }
    }

    json message_sent;
    message_sent["type"] = static_cast<int>(ResponseType::UNREADY);
    message_sent["room name"] = room_name;
    message_sent["player username"] = player_username;
    sendMsgToMultipleConnections(clients, message_sent);
}

void startGame(json &data, int client_fd){
    std::string room_name = data["room_name"];
    std::vector users = readAccountsFile();
    room found_room = findRoomByName(room_name);

    for(USER &user: users){
        if(user.username == found_room.playerX.username || user.username == found_room.playerO.username){
            user.isFree = false;
        }
    }
    updateAccountsFile(users);

    json message_sent;
    message_sent["type"] = static_cast<int>(ResponseType::START);
    message_sent["message"] = "room start";
    message_sent["room name"] = room_name;
    sendMsgToMultipleConnections(clients, message_sent);
}

void handleMove(json &data, int client_fd){
    std::string room_name = data["room name"];
    int current_board = data["current_board"];
    int current_cell = data["current cell"];
    std::string player_username = data["player username"];

    for(room &value: roomList){
        if(value.name == room_name){
            if(value.isPlayerXTurn){
                value.bigBoard[current_board].cells[current_cell] = "X";
            }else{
                value.bigBoard[current_board].cells[current_cell] = "O";
            }
        }

        //check win board
        if(checkWinBoard(value.bigBoard[current_board])){
            // update board owner
            if(value.isPlayerXTurn){
                value.bigBoard[current_board].owner = "X";
            }else{
                value.bigBoard[current_board].owner = "O";
            }

            // because there is a winning board, so we must to check the winning game
            if(checkWinGame(value.bigBoard)){
                // check winner and loser
                std::string winner;
                std::string loser;
                if(value.isPlayerXTurn){
                    winner = value.playerX.username;
                    loser = value.playerO.username;
                }else{
                    winner = value.playerO.username;
                    loser = value.playerX.username;
                }

                // create message
                json gameOverMsg;
                gameOverMsg["type"] = static_cast<int>(ResponseType::WINGAME);
                gameOverMsg["room name"] = room_name;
                gameOverMsg["winner"] = winner;
                gameOverMsg["loser"] = loser;
                
                // send msg to 2 players
                sendMsgToMultipleConnections(clients, gameOverMsg);
                
                std::vector<USER> users = readAccountsFile();
                for(USER &user: users){
                    if(user.username == winner){
                        user.elo += 10;
                        user.wins += 1;
                        user.isFree = true;
                    }else if(user.username == loser){
                        user.elo -= 5;
                        user.loses += 1;
                        user.isFree = true;
                    }
                }
                updateAccountsFile(users);
            }else{
                value.nextBoard = -1;
                json winBoardMessage;
                winBoardMessage["type"] = static_cast<int>(ResponseType::WINBOARD);
                winBoardMessage["next board"] = -1;
                winBoardMessage["room name"] = value.name;
                winBoardMessage["current board"] = current_board;
                winBoardMessage["current cell"] = current_cell;
                sendMsgToMultipleConnections(value.client_in_room, winBoardMessage);
            }
        }else{
            json message_sent;
            message_sent["room name"] = value.name;
            message_sent["current board"] = current_board;
            message_sent["current cell"] = current_cell;

            if(checkWinBoard(value.bigBoard[current_cell])){
                value.nextBoard = -1;
                message_sent["type"] = static_cast<int>(ResponseType::NEXTTURN);
                message_sent["next board"] = -1;
            }else if(checkFullBoard(value.bigBoard[current_cell])){
                value.nextBoard = -1;
                message_sent["type"] = static_cast<int>(ResponseType::BOARDFULL);
                message_sent["next board"] = -1;
            }else{
                value.nextBoard = current_cell;
                message_sent["type"] = static_cast<int>(ResponseType::NEXTTURN);
                message_sent["next board"] = current_cell;
            }
            sendMsgToMultipleConnections(value.client_in_room, message_sent);
        }

        if(value.isPlayerXTurn){
            value.isPlayerXTurn = false;
        }else{
            value.isPlayerXTurn = true;
        }

    }
}

void surrender(json &data, int client_fd){
    std::string loser = data["player_username"];
    std::string winner = data["other player"];
    std::string room_name = data["room_name"];

    json gameOverMsg;
    gameOverMsg["type"] = static_cast<int>(ResponseType::WINGAME);
    gameOverMsg["room name"] = room_name;
    gameOverMsg["winner"] = winner;
    gameOverMsg["loser"] = loser;
    sendMsgToMultipleConnections(clients, gameOverMsg);

    std::vector<USER> users = readAccountsFile();
    for(USER &user: users){
        if(user.username == winner){
            user.elo += 10;
            user.wins += 1;
            user.isFree = true;
        }else if(user.username == loser){
            user.elo -= 5;
            user.loses += 1;
            user.isFree = true;
        }
    }
    updateAccountsFile(users);
}

void *clientHandler(void *arg) {
    int client_fd = *((int *)arg);
    char buffer[BUFFER_SIZE] = {0};

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(client_fd, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            std::cout << "Client disconnected" << std::endl;
            break;
        }
        std::cout << "receive: " << buffer << std::endl;
        json jsonData = json::parse(buffer);
        int type = jsonData["type"];
        json &data = jsonData["data"];

        if(type == static_cast<int>(RequestType::LOGIN)){
            handleLogin(data, client_fd);
        }else if(type == static_cast<int>(RequestType::REGISTER)){
            handleRegister(data, client_fd);
        }else if(type == static_cast<int>(RequestType::LOGOUT)){
            handleLogout(data, client_fd);
        }else if(type == static_cast<int>(RequestType::CREATEROOM)){
            createRoom(data, client_fd);
        }else if(type == static_cast<int>(RequestType::GETONLINEUSER)){
            getOnlinePlayerList(data, client_fd);
        }else if(type == static_cast<int>(RequestType::JOINROOM)){
            joinRoom(data, client_fd);
        }else if(type == static_cast<int>(RequestType::GETROOMLIST)){
            getRoomList(data, client_fd);
        }else if(type == static_cast<int>(RequestType::READY)){
            handleReadyRequest(data, client_fd);
        }else if(type == static_cast<int>(RequestType::UNREADY)){
            handleUnreadyRequest(data,client_fd);
        }else if(type == static_cast<int>(RequestType::START)){
            startGame(data, client_fd);
        }else if(type == static_cast<int>(RequestType::MOVE)){
            handleMove(data, client_fd);
        }else if(type == static_cast<int>(RequestType::SURRENDER)){
            surrender(data, client_fd);
        }
    }

    close(client_fd);
    pthread_exit(NULL);
}

int main(){
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Tạo socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return -1;
    }

    // Thiết lập cấu hình cho socket
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("172.31.193.100");
    address.sin_port = htons(PORT);

    // Liên kết socket với cổng
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return -1;
    }

    // Lắng nghe kết nối
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        return -1;
    }

    while (true) {

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            return -1;
        }

        clients.push_back(new_socket);
        std::cout << "New client connected" << std::endl;

        // clientHandler(new_socket, clients);

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, clientHandler, (void *)&new_socket) != 0) {
            perror("pthread_create");
            return -1;
        }

        pthread_detach(client_thread);
    }
    close(server_fd);
    return 0;
}

