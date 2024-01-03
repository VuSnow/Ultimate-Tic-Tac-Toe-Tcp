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
#include <pthread.h>
#include <vector>
#include <queue>
#include <chrono> // Include the chrono header
#include <thread> 
#include <mutex>
#include "nlohmann/json.hpp"
#include <stdbool.h>
#include <fstream> // Add this line
#include <condition_variable>

using json = nlohmann::json;

struct USER{
    std::string username;
    std::string password;
    std::string status;
    int wins;
    int loses;
    int elo;
    bool isFree;
    double winRate;
    int turn;
};

enum class RequestType{
    LOGIN, 
    LOGOUT,
    REGISTER,
    UPDATEDATA,
    CREATEROOM,
    JOINROOM,
    ROOMLIST,
    READY,
    UNREADY,
    STARTGAME,
};

enum class ResponseType{
    LOGIN,
    LOGOUT,
    REGISTER,
    UPDATEDATA,
    CREATEROOM,
    JOINROOM,
    ROOMLIST,
    READY,
    UNREADY,
    STARTGAME,
};

struct room{
    std::string name;
    std::vector<USER> players;
    int turn;
    bool isFull = false;
    std::vector<int> client_in_room;
    bool player1_ready = false;
    bool player2_ready = false;
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
        if (iss >> user.username >> user.password >> user.status >> user.wins >> user.loses >> user.isFree)
        {
            user.elo = user.wins * 10 - user.loses * 5;
            // user.winRate = (double) user.wins / (user.wins+user.loses) * 100;
            if((user.wins + user.loses) == 0){
                user.winRate = 0;
            }else{
                user.winRate = user.wins / (user.wins + user.loses);
            }
            // std::cout << '.' << user.username << '.' << user.password << '.' << user.elo << '.' << user.winRate << '.' << user.isFree << std::endl;
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

    // Return a default-constructed User object if the user is not found
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

void updateData(){
    // std::cout << data << std::endl;
    std::vector<USER> users = readAccountsFile();
    json onlineUserJSON;
    json roomListJSON;
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

            onlineUserJSON.push_back(userJSON);
        }
    }

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
        roomListJSON.push_back(roomJSON);
    }

    message_sent["type"] = static_cast<int>(ResponseType::UPDATEDATA);
    message_sent["online user"] = onlineUserJSON;
    message_sent["room list"] = roomListJSON;
    for(int client : clients){
        send(client, message_sent.dump().c_str(), message_sent.dump().length(), 0);
    }
}

void handleLogin(json &data, int client_fd){
    std::string username = data["username"];
    std::string password = data["password"];

    USER user = findUserByUsername(username);
    std::vector<USER> users = readAccountsFile();

    json message_sent;

    if(!user.username.empty()){
        if(user.username == username && user.password == password){
            if(user.status == "online"){
                message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
                message_sent["message"] = "already login";
            }else{
                message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
                message_sent["message"] = "login success";
                message_sent["user"]["username"] = user.username;
                message_sent["user"]["wins"] = user.wins;
                message_sent["user"]["loses"] = user.loses;
                message_sent["user"]["winRate"] = user.winRate;
                message_sent["user"]["status"] = user.status;
                message_sent["user"]["isFree"] = user.isFree;
                message_sent["user"]["elo"] = user.elo;

                std::ofstream accountsFile("accounts.txt", std::ios::trunc);
                if(!accountsFile){
                    std::cerr << "Failed to clear accounts file" << std::endl;
                    return;
                }

                for(auto user : users){
                    if(user.username == username){
                        user.status = "online";
                    }
                    accountsFile << user.username << " " << user.password << " " << user.status << " " << user.wins << " " << user.loses << " " << user.isFree << std::endl;
                }
                accountsFile.close();

            }
        }else{
            message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
            message_sent["message"] = "login fail";
        }
    }else{
        message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
        message_sent["message"] = "not existed";
    }

    std::string message = message_sent.dump();
    send(client_fd, message.c_str(), message.length(), 0);
    updateData();
}

void handleRegister(json &data, int client_fd){
    std::string username = data["username"];
    std::string password = data["password"];

    USER findUser = findUserByUsername(username);
    json message_sent;
    USER newUser;
    std::vector<USER> users = readAccountsFile();

    if(findUser.username.empty()){
        newUser.username = username;
        newUser.password = password;
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

        accountsFile << newUser.username << " " << newUser.password << " " << newUser.status << " " << newUser.wins << " " << newUser.loses << " " << newUser.isFree << std::endl;
        accountsFile.close();

        message_sent["type"] = static_cast<int>(ResponseType::REGISTER);
        message_sent["message"] = "register successfully";
    }else{
        message_sent["type"] = static_cast<int>(ResponseType::REGISTER);
        message_sent["message"] = "register fail";
    }

    std::string message = message_sent.dump();
    send(client_fd, message.c_str(), message.length(), 0);
}

void handleLogout(json &data, int client_fd){
    std::string username = data["username"];
    std::vector<USER> users = readAccountsFile();

    std::ofstream accountsFile("accounts.txt", std::ios::trunc);
    if (!accountsFile) {
        std::cerr << "Failed to clear accounts file" << std::endl;
        return;
    }

    for(USER user : users){
        if(user.username == username){
            user.status = "offline";
        }
        accountsFile << user.username << " " << user.password << " " << user.status << " " << user.wins << " " << user.loses << " " << user.isFree << std::endl;
    }
    updateData();
}

void createRoom(json &data, int client_fd){
    std::string room_name = data["room name"];
    std::string player_X_username = data["player X username"];
    USER player_X = findUserByUsername(player_X_username);
    json message_sent;
    
    if(findRoomByName(room_name).name.empty()){
        room newRoom;
        newRoom.name = room_name;
        newRoom.players.push_back(player_X);
        newRoom.turn = 1;
        newRoom.client_in_room.push_back(client_fd);
        roomList.push_back(newRoom);
        // std::cout << "size of room list when create: " << roomList.size();

        message_sent["type"] = static_cast<int>(ResponseType::CREATEROOM);
        message_sent["message"] = "create success";
        message_sent["room name"] = room_name;

        send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
        updateData();
        // std::cout << "room size when create: " << newRoom.players.size() << std::endl;
        // std::cout << "room list size after create: " << roomList.size() << std::endl;
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
    json message_sent;

    if(findRoomByName(room_name).name.empty()){
        message_sent["type"] = static_cast<int>(ResponseType::JOINROOM);
        message_sent["message"] = "join fail";
        send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
        return;
    }

    for(room &roomValue : roomList){
        if(roomValue.name == room_name){
            if(roomValue.isFull){
                message_sent["type"] = static_cast<int>(ResponseType::JOINROOM);
                message_sent["message"] = "full";
                send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
            }else{
                roomValue.players.push_back(player_O);
                roomValue.isFull = true;
                roomValue.client_in_room.push_back(client_fd);
                message_sent["type"] = static_cast<int>(ResponseType::JOINROOM);
                message_sent["message"] = "join success";
                message_sent["room name"] = room_name;
                send(client_fd, message_sent.dump().c_str(), message_sent.dump().length(), 0);
                // updateData();
            }
        }
    }
    updateData();
}

void handleReadyRequest(json &data, int client_fd){
    std::cout << data << std::endl;
    std::string room_name = data["room_name"];
    std::string player_username = data["player_username"];
    json message_sent;

    for(room &value : roomList){
        if(value.name == room_name){
            if(value.players.size() == 1 && value.players[0].username == player_username){
                value.player1_ready = true;
            }else if(value.isFull && value.players[0].username == player_username){
                value.player1_ready = true;
            }else if(value.isFull && value.players[1].username == player_username){
                value.player2_ready = true;
            }
        }
    }
    updateData();
}

void handleUnreadyRequest(json &data, int client_fd){
    std::cout << data << std::endl;
    std::string room_name = data["room_name"];
    std::string player_username = data["player_username"];
    json message_sent;

    for(room &value : roomList){
        if(value.name == room_name){
            if(value.players.size() == 1 && value.players[0].username == player_username){
                value.player1_ready = false;
            }else if(value.isFull && value.players[0].username == player_username){
                value.player1_ready = false;
            }else if(value.isFull && value.players[1].username == player_username){
                value.player2_ready = false;
            }
        }
    }
    updateData();
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

        json jsonData = json::parse(buffer);
        int type = jsonData["type"];
        json &data = jsonData["data"];

        if (type == static_cast<int>(RequestType::LOGIN)) {
            handleLogin(data, client_fd);
        }else if(type == static_cast<int>(RequestType::REGISTER)){
            handleRegister(data, client_fd);
        }else if(type == static_cast<int>(RequestType::LOGOUT)){
            handleLogout(data, client_fd);
        }else if(type == static_cast<int>(RequestType::CREATEROOM)){
            createRoom(data, client_fd);
        }else if(type == static_cast<int>(RequestType::JOINROOM)){
            joinRoom(data, client_fd);
        }else if(type == static_cast<int>(RequestType::UPDATEDATA)){
            updateData();
        }else if(type == static_cast<int>(RequestType::READY)){
            handleReadyRequest(data, client_fd);
        }else if(type == static_cast<int>(RequestType::UNREADY)){
            handleUnreadyRequest(data,client_fd);
        }
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
    address.sin_addr.s_addr = inet_addr("192.168.99.51");
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