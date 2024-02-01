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

const int PORT = 3000;
const int BUFFER_SIZE = 1024;

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
    BOARDFULL,
    SURRENDER,
    // Add more request types as needed
};

enum class ResponseType
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
    BOARDFULL,
    SURRENDER,
};

struct USER{
    std::string username;
    std::string password;
    std::string status;
    std::string ingame;
    int wins;
    int loses;
    int elo;
    bool isFree;
    double winRate;
    int turn;
    int socket;
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
    USER playerX;
    USER playerO;
    std::vector<int> client_in_room;
    bool isFull;
    bool player1_ready;
    bool player2_ready;
    bool gameStart;
    bool isPlayerXTurn;
    int turn;
    int nextBoard;
    std::vector<board> bigBoard;
    room() : isFull(false), isPlayerXTurn(false), player1_ready(false), player2_ready(false), turn(-2), nextBoard(-1), gameStart(false){
        for(int i = 0; i < 9; i++){
            bigBoard.push_back(board());
        }
    }
};

std::vector<room> roomList;
std::vector<int> clients;

// define functions
void *clientHandler(void *arg);

std::vector<USER> readAccountsFile();
void updateAccountsFile(std::vector<USER> users);
USER findUserByUsername(const std::string &username);
USER findUserByIngame(const std::string &ingame);
room findRoomByName(const std::string &room_name);
void handleLogin(json &data, int client_fd);
void handleRegister(json &data, int client_fd);
void handleLogout(json &data, int client_fd);
void createRoom(json &data, int client_fd);
void joinRoom(json &data, int client_fd);
void getOnlinePlayerList(json &data, int client_fd);
void handleReadyRequest(json &data, int client_fd);
void handleUnreadyRequest(json &data, int client_fd);
// void handleRegister(json &data, int client_fd);
