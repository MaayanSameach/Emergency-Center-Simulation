#pragma once

#define STOMP_CLIENT_H
#include "ConnectionHandler.h"
#include "StompProtocol.h"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>

class StompClient {
private:
    std::atomic<bool> isLoggedIn;
    std::string username;
    ConnectionHandler* connectionHandler;
    StompProtocol* protocol;
    std::thread serverThread;
    std::mutex mutex;
    std::mutex sharedDataMutex; // Protect shared data

    void handleLogin(const std::vector<std::string>& args);
    void handleLogout();
    void handleJoin(const std::vector<std::string>& args);
    void handleExit(const std::vector<std::string>& args);
    void handleReport(const std::vector<std::string>& args);
    void handleSummary(const std::vector<std::string>& args);
    std::vector<std::string> split(const std::string& input, char delimiter);
    
public:
    StompClient();
    ~StompClient();
    StompClient(const StompClient&) = delete; // Prevent copying
    StompClient& operator=(const StompClient&) = delete;
    std::string epochToDateTime(long epochTime);
    void start();
    void serverThreadLoop();
    
};