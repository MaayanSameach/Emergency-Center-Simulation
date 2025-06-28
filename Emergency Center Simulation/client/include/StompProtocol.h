#pragma once
#include "../include/ConnectionHandler.h"
#include "event.h"
#include <iostream>
#include <unordered_set>
#include <string>
#include <mutex>

struct Report {
    std::string eventName;
    std::string city;
    long dateTime; // Unix timestamp
    std::string description;
    std::map<std::string, std::string> details; // Holds key-value pairs like "active", "forces_arrival_at_scene", etc.
    Report()
        : eventName(""), city(""), dateTime(0), description(""), details() {}};

// TODO: implement the STOMP protocol
class StompProtocol {
private:
    std::mutex protocolMutex; // Protect shared resources
    std::map<std::string, std::vector<Report>> reportStorage; // Key: topic:user, Value: list of reports
    bool loggedIn= true; // Tracks whether the client is logged in
    std::unordered_set<std::string> joinedTopics;

public:
    StompProtocol();
    std::string createFrame(const std::string& command, const std::map<std::string, std::string>& headers = {}, const std::string& body = "");
    void processFrame(const std::string& frame);
    void storeReport(const std::string& topic, const std::string& user, const std::string& content);
    std::vector<Report> getReports(const std::string& topic, const std::string& user);
    void joinTopic(const std::string& topic);
    void exitTopic(const std::string& topic);
    bool isSubscribed(const std::string& topic);
    // Check login status
    bool isLoggedIn();
    // Set login status
    void setLoggedIn(bool status);
};
