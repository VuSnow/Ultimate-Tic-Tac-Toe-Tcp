#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <pthread.h>
#include <vector>
#include <queue>
#include <chrono> // Include the chrono header
#include <thread> 
#include <mutex>
#include <cmath>
#include "nlohmann/json.hpp"
#include <stdbool.h>
#include <fstream> // Add this line
#include <condition_variable>

using json = nlohmann::json;

struct USER{
    std::string username;
    std::string password;
    std::string status;
    std::string ingame;
    int user_socket;
    int wins;
    int loses;
    int elo;
    bool isFree;
    double winRate;
    int turn;
};

enum class RequestType
{
    LOGIN,
    LOGOUT,
    REGISTER,
    UPDATEONLINEUSER,
    GETONLINEUSER,
    GETROOMLIST,
    UPDATEROOMLIST,
    CREATEROOM,
    JOINROOM,
    READY,
    UNREADY,
    START,
    MOVE,
    WINBOARD,
    NEXTTURN,
    WINGAME,
    SURRENDER,
    // Add more request types as needed
};

enum class ResponseType{
    LOGIN,
    LOGOUT,
    REGISTER,
    UPDATEONLINEUSER,
    GETONLINEUSER,
    GETROOMLIST,
    UPDATEROOMLIST,
    CREATEROOM,
    JOINROOM,
    READY,
    UNREADY,
    START,
    MOVE,
    WINBOARD,
    NEXTTURN,
    WINGAME,
    SURRENDER,
};

struct board{
    std::vector<std::string> cells;
    std::string owner;
    board(){
        for(int i = 0; i < 9; i++){
            cells.push_back(" ");
        }
        owner = " ";
    }
};



struct room{
    std::string name;
    std::vector<USER> players;
    std::vector<int> client_in_room;
    bool isFull;
    bool player1_ready;
    bool player2_ready;
    bool gameStart;
    bool isPlayerXTurn;
    int turn;
    int nextBoard;
    std::vector<board> bigBoard;
    std::vector<int> winningBoard;
    room() : isFull(false), isPlayerXTurn(true), player1_ready(false), player2_ready(false), gameStart(false), turn(-2), nextBoard(-1) {
        for(int i = 0; i < 9; i++){
            bigBoard.push_back(board());
        }
    }
};

std::vector<room> roomList;

const int PORT = 3000;
const int BUFFER_SIZE = 1024;
std::vector<int> clients;

std::vector<USER> readAccountsFile()
{
    std::vector<USER> users;
    std::ifstream accountsFile("accounts.txt");
    if (!accountsFile)
    {
        std::cerr << "Failed to open accounts file" << std::endl;
        return users;
    }

    std::string line;
    while (std::getline(accountsFile, line))
    {
        std::istringstream iss(line);
        USER user;
        if (iss >> user.username >> user.password >> user.ingame >> user.status >> user.wins >> user.loses >> user.isFree)
        {
            user.elo = user.wins * 10 - user.loses * 5;
            if((user.wins + user.loses) == 0){
                user.winRate = 0;
            }else{
                user.winRate = (double) user.wins / (user.wins + user.loses) * 100;
            }
            users.push_back(user);
        }
    }

    return users;
}

USER findUserByUsername(const std::string &username)
{
    std::vector<USER> users = readAccountsFile();
    for (const USER &user : users)
    {
        if (user.username == username)
        {
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

room findRoomByName(const std::string &roomName){
    for(const room &roomIn4 : roomList){
        if(roomIn4.name == roomName){
            return roomIn4;
        }
    }
    return room();
}

void sendMessageToMultipleConnections(std::vector<int> clients, json message){
    for(int client:clients){
        send(client, message.dump().c_str(), message.dump().length(), 0);
    }
}

void updateRoomList(){
    json roomListJSON;
    json message_sent;
    
    for(room roomValue : roomList){
        std::cout << "size:" << roomValue.players.size() << std::endl;
        json roomJSON;
        roomJSON["room name"] = roomValue.name;
        roomJSON["player X username"] = roomValue.players[0].username;
        if(roomValue.isFull){
            roomJSON["player O username"] = roomValue.players[1].username;
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
    message_sent["message"] = "update room list";
    message_sent["room list"] = roomListJSON;
    for(int client : clients){
        send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0);
    }
}

void getOnlinePlayerList(int client_fd){
    std::vector<USER> users = readAccountsFile();
    json onlineUserJSON;
    json message_sent;

    for(USER &user : users){
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

void getRoomList(int client_fd){
    json roomListJSON;
    json message_sent;
    
    for(room roomValue : roomList){
        // std::cout << "size:" << roomValue.players.size() << std::endl;
        json roomJSON;
        roomJSON["room name"] = roomValue.name;
        roomJSON["player X username"] = roomValue.players[0].username;
        if(roomValue.isFull){
            roomJSON["player O username"] = roomValue.players[1].username;
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

void handleLogin(json &data, int client_fd){
    std::cout << data << std::endl;
    std::string username = data["username"];
    std::string password = data["password"];
    USER user = findUserByUsername(username);
    std::vector<USER> users = readAccountsFile();

    if(!user.username.empty()){
        if(user.username == username && user.password == password){
            if(user.status == "online"){
                json message_sent;
                message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
                message_sent["message"] = "already login";
                send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
            }else{
                // open account file
                std::ofstream accountsFile("accounts.txt", std::ios::trunc);
                if(!accountsFile){
                    std::cerr << "Failed to clear accounts file" << std::endl;
                    return;
                }
                // update user data
                for(USER &user:users){
                    if(user.username == username){
                        user.status = "online";
                        user.user_socket = client_fd;
                    }
                    accountsFile << user.username << " " << user.password << " " << user.ingame << " "  << user.status << " " << user.wins << " " << user.loses << " " << user.isFree << std::endl;
                }

                USER updateUser = findUserByUsername(username);
                json userJson;
                userJson["username"] = updateUser.username;
                userJson["status"] = updateUser.status;
                userJson["ingame"] = updateUser.ingame;
                userJson["wins"] = updateUser.wins;
                userJson["loses"] = updateUser.loses;
                userJson["elo"] = updateUser.elo;
                userJson["is free"] = updateUser.isFree;
                userJson["win rate"] = updateUser.winRate;

                // send message to client
                json message_sent;
                for(int client : clients){
                    if(client != client_fd){
                        message_sent["type"] = static_cast<int>(ResponseType::UPDATEONLINEUSER);
                        message_sent["message"] = "add user to online list";
                    }else{
                        message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
                        message_sent["message"] = "login success";
                    }
                    message_sent["user"] = userJson;
                    send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0);
                }
            }
        }else{
            json message_sent;
            message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
            message_sent["message"] = "login fail";
            send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
        }
    }else{
        json message_sent;
        message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
        message_sent["message"] = "already login";
        send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
    }
}

void handleRegister(json &data, int client_fd){
    std::string username = data["username"];
    std::string password = data["password"];
    std::string ingame = data["ingame"];

    USER findUser = findUserByUsername(username);
    USER ingame_user = findUserByIngame(ingame);
    json message_sent;
    USER newUser;
    std::vector<USER> users = readAccountsFile();

    if(findUser.username.empty() && ingame_user.username.empty()){
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

        std::ofstream accountsFile("accounts.txt", std::ios::app);
        if (!accountsFile) {
            std::cerr << "Failed to open accounts file for writing" << std::endl;
            return;
        }

        accountsFile << newUser.username << " " << newUser.password << " " << newUser.ingame << " " << newUser.status << " " << newUser.wins << " " << newUser.loses << " " << newUser.isFree << std::endl;
        accountsFile.close();

        message_sent["type"] = static_cast<int>(ResponseType::REGISTER);
        message_sent["message"] = "register successfully";
    }else if (!findUser.username.empty()){
        message_sent["type"] = static_cast<int>(ResponseType::REGISTER);
        message_sent["message"] = "existed username";
    }else if(!ingame_user.username.empty()){
        message_sent["type"] = static_cast<int>(ResponseType::REGISTER);
        message_sent["message"] = "existed ingame";
    }

    std::string message = message_sent.dump();
    send(client_fd, message.c_str(), message.length(), 0);
}

void handleLogout(json &data, int client_fd){
    std::string username = data["username"];
    std::vector<USER> users = readAccountsFile();
    json message_sent;

    std::ofstream accountsFile("accounts.txt", std::ios::trunc);
    if (!accountsFile) {
        std::cerr << "Failed to clear accounts file" << std::endl;
        return;
    }

    for(USER &user : users){
        if(user.username == username){
            user.status = "offline";
        }
        accountsFile << user.username << " " << user.password << " " << user.ingame << " " << user.status << " " << user.wins << " " << user.loses << " " << user.isFree << std::endl;
    }

    auto it = std::find(clients.begin(), clients.end(), client_fd);
    if(it != clients.end()){
        clients.erase(it);
    }

    message_sent["type"] = static_cast<int>(ResponseType::UPDATEONLINEUSER);
    message_sent["message"] = "delete user from online list";
    message_sent["username"] = username;
    
    for(int client : clients){
        send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0); 
    }
}

void createRoom(json &data, int client_fd){
    std::cout << data << std::endl;
    std::string room_name = data["room name"];
    std::string player_X_username = data["player X username"];
    USER player_X = findUserByUsername(player_X_username);
    
    if(findRoomByName(room_name).name.empty()){
        room *newRoom = new room();
        newRoom->name = room_name;
        newRoom->players.push_back(player_X);
        newRoom->client_in_room.push_back(client_fd);
        roomList.push_back(*newRoom);

        json message_sent;
        json room_data;
        room_data["name"] = newRoom->name;
        room_data["player X username"] = newRoom->players[0].username;

        for(int client : clients){
            if(client == client_fd){
                message_sent["type"] = static_cast<int>(ResponseType::CREATEROOM);
                message_sent["message"] = "create success";
            }else{
                message_sent["type"] = static_cast<int>(ResponseType::UPDATEROOMLIST);
                message_sent["message"] = "add room to room list";
            }
            message_sent["data"] = room_data;
            send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0);
        }
    }else{
        json message_sent;
        message_sent["type"] = static_cast<int>(ResponseType::CREATEROOM);
        message_sent["message"] = "create fail";
        send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(),0);
    }
}

void joinRoom(json &data, int client_fd){
    std::string room_name = data["room name"];
    std::string player_O_username = data["player O username"];
    USER player_O = findUserByUsername(player_O_username);

    if(findRoomByName(room_name).name.empty()){
        json message_sent;
        message_sent["type"] = static_cast<int>(ResponseType::LOGOUT);
        message_sent["message"] = "join fail";
        send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
    }else{
        for(room &value : roomList){
            if(value.name == room_name){
                if(value.isFull){
                    json message_sent;
                    message_sent["type"] = static_cast<int>(ResponseType::JOINROOM);
                    message_sent["message"] = "full";
                    send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
                }else{
                    value.players.push_back(player_O);
                    value.client_in_room.push_back(client_fd);
                    value.isFull = true;

                    for(int client : clients){
                        json message_sent;
                        message_sent["room name"] = room_name;
                        if(client == client_fd){
                            message_sent["type"] = static_cast<int>(ResponseType::JOINROOM);
                            message_sent["message"] = "join success";
                        }else{
                            message_sent["type"] = static_cast<int>(RequestType::UPDATEROOMLIST);
                            message_sent["message"] = "add player to room";
                            message_sent["player O username"] = player_O_username;
                        }
                        send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0);
                    }
                }
            }
        }
    }
}

void handleReadyRequest(json &data, int client_fd){
    // std::cout << data << std::endl;
    std::string room_name = data["room_name"];
    std::string player_username = data["player_username"];

    for(room &value :roomList){
        if(value.name == room_name){
            if(value.players[0].username == player_username){
                value.player1_ready = true;
            }else if(value.players[1].username == player_username){
                value.player2_ready = true;
            }
        }
    }
    
    for(int client : clients){
        json message_sent;
        message_sent["type"] = static_cast<int>(ResponseType::READY);
        message_sent["room name"] = room_name;
        message_sent["player username"] = player_username;
        send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0);
    }
}

void handleUnreadyRequest(json &data, int client_fd){
    // std::cout << data << std::endl;
    std::string room_name = data["room_name"];
    std::string player_username = data["player_username"];

    for(room &value :roomList){
        if(value.name == room_name){
            if(value.players[0].username == player_username){
                value.player1_ready = false;
            }else if(value.players[1].username == player_username){
                value.player2_ready = false;
            }
        }
    }

    for(int client : clients){
        json message_sent;
        message_sent["type"] = static_cast<int>(ResponseType::UNREADY);
        message_sent["room name"] = room_name;
        message_sent["player username"] = player_username;
        send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0);
    }
}

void startGame(json &data, int client_fd){
    std::string room_name = data["room_name"];
    std::vector users = readAccountsFile();
    room found_room = findRoomByName(room_name);
    json message_sent;

    std::ofstream accountsFile("accounts.txt", std::ios::trunc);
    if(!accountsFile){
        std::cerr << "Failed to clear accounts file" << std::endl;
        return;
    }

    for(USER &user: users){
        if(user.username == found_room.players[0].username || user.username == found_room.players[1].username){
            user.isFree = false;
        }
        accountsFile << user.username << " " << user.password << " " << user.ingame << " "  << user.status << " " << user.wins << " " << user.loses << " " << user.isFree << std::endl;
    }

    for(int client : clients){
        message_sent["type"] = static_cast<int>(ResponseType::START);
        message_sent["message"] = "room start";
        message_sent["room name"] = room_name;
        send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0);
    }
}

bool checkFullBoard(board checkBoard){
    for(int i = 0; i < 9; i++){
        if(checkBoard.cells[i] == " "){
            return false;
        }
    }
    return true;
}

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

void handleMove(json &data, int client_fd){
    std::cout << data << std::endl;
    std::string room_name = data["room name"];
    int current_board = data["current_board"];
    int current_cell = data["current_cell"];

    for(room &value : roomList){
        if(value.name == room_name){
            // update value
            if(value.isPlayerXTurn){
                value.bigBoard[current_board].cells[current_cell] = "X";
            }else{
                value.bigBoard[current_board].cells[current_cell] = "O";
            }
            // value.nextBoard = current_cell;
            if(checkWinBoard(value.bigBoard[current_board])){
                //if win board -> set owner = previous turn
                if(value.isPlayerXTurn){
                    value.bigBoard[current_board].owner = "X";
                }else{
                    value.bigBoard[current_board].owner = "O";
                }
                
                if(checkWinGame(value.bigBoard)){
                    // if win game -> send message
                    json gameOver;
                    gameOver["type"] = static_cast<int>(ResponseType::WINGAME);
                    gameOver["room name"] = room_name;
                    if(value.isPlayerXTurn){
                        gameOver["winner"] = value.players[0].username;
                    }else{
                        gameOver["winner"] = value.players[1].username;
                    }
                    sendMessageToMultipleConnections(value.client_in_room, gameOver);

                    // open account file to update user elo
                    std::vector<USER> users = readAccountsFile();
                    std::ofstream accountsFile("accounts.txt", std::ios::trunc);

                    if(!accountsFile){
                        std::cerr << "Failed to clear accounts file" << std::endl;
                        return;
                    }

                    if(value.isPlayerXTurn){
                        for(USER &user: users){
                            if(user.username == value.players[0].username){
                                user.elo += 10;
                                user.wins += 1;
                            }else if(user.username == value.players[1].username){
                                user.elo -= 5;
                                user.loses += 1;
                            }
                            user.isFree = true;
                            accountsFile << user.username << " " << user.password << " " << user.ingame << " "  << user.status << " " << user.wins << " " << user.loses << " " << user.isFree << std::endl;
                        }
                    }else{
                        for(USER &user: users){
                            if(user.username == value.players[1].username){
                                user.elo += 10;
                                user.wins += 1;
                            }else if(user.username == value.players[0].username){
                                user.elo -= 5;
                                user.loses += 1;
                            }
                            user.isFree = true;
                            accountsFile << user.username << " " << user.password << " " << user.ingame << " "  << user.status << " " << user.wins << " " << user.loses << " " << user.isFree << std::endl;
                        }
                    }
                    accountsFile.close();

                    std::vector<USER> updated_list = readAccountsFile();
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    json message_sent;
                    json list_online_updated;

                    message_sent["type"] = static_cast<int>(ResponseType::UPDATEONLINEUSER);
                    message_sent["message"] = "update elo";
                    for(USER &user : updated_list){
                        json userJSON;
                        userJSON["username"] = user.username;
                        userJSON["elo"] = user.elo;
                        userJSON["ingame"] = user.ingame;
                        userJSON["status"] = user.status;
                        userJSON["wins"] = user.wins;
                        userJSON["loses"] = user.loses;
                        userJSON["is free"] = user.isFree;
                        userJSON["win rate"] = user.winRate;

                        list_online_updated.push_back(userJSON);
                    }
                    message_sent["user list"] = list_online_updated;

                    sendMessageToMultipleConnections(clients, message_sent);
                    value.gameStart = false;
                    value.isPlayerXTurn = true;
                    value.nextBoard = -2;
                    value.player1_ready = false;
                    value.player2_ready = false;
                    value.turn = -2;
                    value.bigBoard.clear();
                    value.winningBoard.clear();
                    
                }else{
                    // not win game -> send winboard
                    value.winningBoard.push_back(current_board);
                    value.nextBoard = -1;
                    json winCellMessage;
                    winCellMessage["type"] = static_cast<int>(ResponseType::WINBOARD);
                    // winCellMessage["message"] = "player O win cell";
                    winCellMessage["room name"] = value.name;
                    winCellMessage["current board"] = current_board;
                    winCellMessage["current cell"] = current_cell;
                    sendMessageToMultipleConnections(value.client_in_room, winCellMessage);
                }
            }else{
                // not win game not winboard -> next turn
                json nextTurnMessage;
                nextTurnMessage["type"] = static_cast<int>(ResponseType::NEXTTURN);
                nextTurnMessage["message"] = "next turn";
                nextTurnMessage["room name"] = value.name;
                if(checkWinBoard(value.bigBoard[current_board])){
                    nextTurnMessage["next board"] = -1;
                }else{
                    nextTurnMessage["next board"] = current_board;
                }
                // nextTurnMessage["next board"] = value.nextBoard;
                nextTurnMessage["current board"] = current_board;
                nextTurnMessage["current cell"] = current_cell;
                sendMessageToMultipleConnections(value.client_in_room, nextTurnMessage);
            }

            if(value.isPlayerXTurn){
                value.isPlayerXTurn = false;
            }else{
                value.isPlayerXTurn = true;
            }
        }
    }
}

void removePlayerFromRoom(const std::string& username, room& currentRoom) {
    // Tìm người chơi trong vector players của room và xóa nếu tìm thấy
    auto it = std::find_if(currentRoom.players.begin(), currentRoom.players.end(),
                           [&username](const USER& player) {
                               return player.username == username;
                           });

    if (it != currentRoom.players.end()) {
        // Xóa người chơi khỏi vector players của room
        currentRoom.players.erase(it);
    }
}

void removeClientFromRoom(const int& client_fd, room& currentRoom){
    auto it = std::find(currentRoom.client_in_room.begin(), currentRoom.client_in_room.end(), client_fd);
    if(it != currentRoom.client_in_room.end()){
        currentRoom.client_in_room.erase(it);
    }
}
void handleSurrender(json &data, int client_fd){
    std::string room_name = data["room name"];
    std::string action = data["action"];
    std::string username = data["username"];

    std::vector<USER> users = readAccountsFile();
    std::ofstream accountsFile("accounts.txt", std::ios::trunc);

    if(!accountsFile){
        std::cerr << "Failed to clear accounts file" << std::endl;
        return;
    }

    for(room &value: roomList){
        if(value.name == room_name){
            if(action == "exit"){
                removePlayerFromRoom(username, value);
                value.isFull = false;
                if(value.players[0].username == username){
                    removeClientFromRoom(value.players[0].user_socket, value);
                }
                if(value.players[1].username == username){
                    removeClientFromRoom(value.players[1].user_socket, value);
                }
                for(USER &user : users){
                    if(user.username == username){
                        user.loses += 1;
                        user.elo -= 5;
                        user.isFree = true;
                    }else if(user.username == value.players[0].username){
                        user.wins += 1;
                        user.elo += 10;
                        user.isFree = true;
                    }
                    accountsFile << user.username << " " << user.password << " " << user.ingame << " "  << user.status << " " << user.wins << " " << user.loses << " " << user.isFree << std::endl;
                }
            }else{
                std::string other_player = data["other player"];
                for(USER &user: users){
                    if(user.username == username){
                        user.loses += 1;
                        user.elo -= 5;
                        user.isFree = true;
                    }else if(user.username == other_player){
                        user.wins += 1;
                        user.elo += 10;
                        user.isFree = true;
                    }
                    accountsFile << user.username << " " << user.password << " " << user.ingame << " "  << user.status << " " << user.wins << " " << user.loses << " " << user.isFree << std::endl;
                }
            }
            value.gameStart = false;
            value.bigBoard.clear();
            value.nextBoard = -2;
            value.turn = -2;
            value.player1_ready = false;
            value.player2_ready = false;
            value.winningBoard.clear();
        }
    }
    for(int client : clients){
        getOnlinePlayerList(client);
    }
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

        std::string request_data = buffer;
        size_t bracePos = request_data.find_first_of('{');
        std::string jsonSubstring = request_data.substr(bracePos);

        json jsonData = json::parse(jsonSubstring);
        int type = jsonData["type"];
        json &data = jsonData["data"];

        if(type == static_cast<int>(RequestType::LOGIN)){
            handleLogin(data, client_fd);
        }else if(type == static_cast<int>(RequestType::GETONLINEUSER)){
            getOnlinePlayerList(client_fd);
        }else if(type == static_cast<int>(RequestType::GETROOMLIST)){
            getRoomList(client_fd);
        }else if(type == static_cast<int>(RequestType::CREATEROOM)){
            createRoom(data, client_fd);
        }else if(type == static_cast<int>(RequestType::JOINROOM)){
            joinRoom(data, client_fd);
        }else if(type == static_cast<int>(RequestType::READY)){
            handleReadyRequest(data, client_fd);
        }else if(type == static_cast<int>(RequestType::UNREADY)){
            handleUnreadyRequest(data,client_fd);
        }else if(type == static_cast<int>(RequestType::START)){
            startGame(data, client_fd);
        }else if(type == static_cast<int>(RequestType::MOVE)){
            handleMove(data, client_fd);
        }else if(type == static_cast<int>(RequestType::LOGOUT)){
            handleLogout(data, client_fd);
        }else if(type == static_cast<int>(RequestType::REGISTER)){
            handleRegister(data, client_fd);
        }
        // else if(type == static_cast<int>(RequestType::SURRENDER)){
        //     handleSurrender(data, client_fd);
        // }
    }

    close(client_fd);
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // std::cout << (double) round(135/(135+80)*100) * 100 / 100;
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
    address.sin_addr.s_addr = inet_addr("172.20.10.2");
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