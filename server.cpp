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
};

enum class RequestType{
    LOGIN, 
    LOGOUT,
    REGISTER,
    ONLINEPLAYER,
};

enum class ResponseType{
    LOGIN,
    LOGOUT,
    REGISTER,
    ONLINEPLAYER,
};

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
            user.winRate = (double) user.wins / (user.wins+user.loses) * 100;
            // std::cout << '.' << user.username << '.' << user.password << '.' << user.elo << '.' << user.winRate << '.' << user.isFree << std::endl;
            users.push_back(user);
        }
    }

    return users;
}
// std::vector<user> users = readAccountsFile();

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

void handleLogin(json &data, int client_fd){
    std::string username = data["username"];
    std::string password = data["password"];

    USER user = findUserByUsername(username);
    std::vector<USER> users = readAccountsFile();
    json onlineUser;

    json message_sent;
    
    if(!user.username.empty()){
        if(user.username == username && user.password == password){
            if(user.status == "online"){
                message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
                message_sent["message"] = "already login";
                
                std::string message = message_sent.dump();
                send(client_fd, message.c_str(), message.length(), 0);
            }else{
                user.status = "online";
                message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
                message_sent["message"] = "success";
                message_sent["user"]["username"] = user.username;
                message_sent["user"]["wins"] = user.wins;
                message_sent["user"]["loses"] = user.loses;
                message_sent["user"]["winRate"] = user.winRate;
                message_sent["user"]["status"] = user.status;
                message_sent["user"]["isFree"] = user.isFree;
                message_sent["user"]["elo"] = user.elo;

                std::ofstream accountsFile("accounts.txt", std::ios::trunc);
                if (!accountsFile) {
                    std::cerr << "Failed to clear accounts file" << std::endl;
                    return;
                }

                for(auto user : users){
                    if(user.username == username){
                        user.status = "online";
                    }
                    if(user.status == "online"){
                        json userJson;
                        userJson["username"] = user.username;
                        userJson["status"] = user.status;
                        userJson["wins"] = user.wins;
                        userJson["loses"] = user.loses;
                        userJson["elo"] = user.elo;
                        userJson["isFree"] = user.isFree;
                        userJson["winRate"] = user.winRate;

                        onlineUser.push_back(userJson);
                    }
                    accountsFile << user.username << " " << user.password << " " << user.status << " " << user.wins << " " << user.loses << " " << user.isFree << std::endl;
                }

                accountsFile.close();
                std::string message = message_sent.dump();
                send(client_fd, message.c_str(), message.length(), 0);

                std::this_thread::sleep_for(std::chrono::seconds(2));

                for (auto client : clients){
                    message_sent["type"] = static_cast<int>(ResponseType::ONLINEPLAYER);
                    message_sent["message"] = "online users";
                    message_sent["users"] = onlineUser; 
                    message = message_sent.dump();
                    send(client, message.c_str(), message.length(), 0);
                }
            }
        }else {
            message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
            message_sent["message"] = "fail";
            std::string message = message_sent.dump();
            send(client_fd, message.c_str(), message.length(), 0);
        }
    }else{
        message_sent["type"] = static_cast<int>(ResponseType::LOGIN);
        message_sent["message"] = "not existed";
        std::string message = message_sent.dump();
        send(client_fd, message.c_str(), message.length(), 0);
    }

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
        // std::string message = message_sent.dump();
        // send(client_fd, message.c_str(), message.length(), 0);
    }else{
        message_sent["type"] = static_cast<int>(ResponseType::REGISTER);
        message_sent["message"] = "register fail";
    }

    std::string message = message_sent.dump();
    send(client_fd, message.c_str(), message.length(), 0);
}

void handleLogout(json &data, int client_fd){

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

        std::cout << "Received: " << buffer << std::endl;

        json jsonData = json::parse(buffer);
        int type = jsonData["type"];
        json &data = jsonData["data"];

        if (type == static_cast<int>(RequestType::LOGIN)) {
            handleLogin(data, client_fd);
        }else if(type == static_cast<int>(RequestType::REGISTER)){
            handleRegister(data, client_fd);
        }else if(type == static_cast<int>(RequestType::LOGOUT)){
            handleLogout(data, client_fd);
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
    address.sin_addr.s_addr = inet_addr("192.168.1.6");
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