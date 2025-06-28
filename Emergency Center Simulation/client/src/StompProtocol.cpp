#include <sstream>
#include <iostream>
#include "StompProtocol.h"
#include "StompClient.h"
#include "event.h"
#include <json.hpp>
using json = nlohmann::json;

StompProtocol::StompProtocol()
    : protocolMutex(), reportStorage(),joinedTopics() {}

std::string StompProtocol::createFrame(const std::string& command, const std::map<std::string, std::string>& headers, const std::string& body) {
    std::ostringstream frame;
    frame << command << "\n";

    for (const auto& [key, value] : headers) {
        frame << key << ":" << value << "\n";
    }

    frame << "\n" << body << "\n\0";
    return frame.str();
}

std::map<std::string, std::string> parseHeaders(const std::string& frame) {
    std::istringstream stream(frame);
    std::map<std::string, std::string> headers;
    std::string line;

    // Skip command line
    std::getline(stream, line);

    // Parse headers
    while (std::getline(stream, line) && !line.empty()) {
        auto separator = line.find(':');
        if (separator != std::string::npos) {
            std::string key = line.substr(0, separator);
            std::string value = line.substr(separator + 1);
            headers[key] = value;
        }
    }

    return headers;
}

void StompProtocol::processFrame(const std::string& frame) {
    std::istringstream stream(frame);
    std::string command;

    if (!std::getline(stream, command)) {
        std::cerr << "Error: Unable to parse command from frame." << std::endl;
        return;
    }
    
    std::map<std::string, std::string> headers;
    std::string line;

    // Parse headers
    while (std::getline(stream, line) && !line.empty()) {
        size_t separator = line.find(':');
        if (separator != std::string::npos) {
            std::string key = line.substr(0, separator);
            std::string value = line.substr(separator + 1);
            headers[key] = value;
        } else {
            std::cerr << "Invalid header format: " << line << std::endl;
        }
    }

    // Parse body
    std::string body;
    if (stream.peek() != EOF) {
        std::getline(stream, body, '\0');
    }

    // Handle commands
    if (command == "CONNECTED") {
        std::cout << "Successfully connected to server." << std::endl;
        setLoggedIn(true); // Mark as logged in
    } else if (command == "MESSAGE") {
        // Get topic from the header
        auto destinationIt = headers.find("destination");
        if (destinationIt == headers.end()) {
            std::cerr << "MESSAGE frame missing 'destination' header." << std::endl;
            return;
        }
        std::string topic = destinationIt->second;

        // Extract user from the body
        auto userPos = body.find("user:");
        if (userPos == std::string::npos) {
            std::cerr << "MESSAGE frame body missing 'user' field." << std::endl;
            return;
        }
        // Extract the value after "user:"
        auto userStart = userPos + 5; // Move past "user:"
        auto userEnd = body.find('\n', userStart);
        std::string user = body.substr(userStart, userEnd - userStart);

        // Trim leading/trailing spaces
        user.erase(0, user.find_first_not_of(" \t"));
        user.erase(user.find_last_not_of(" \t") + 1);

        // Store the report
        storeReport(topic, user, body);
        //std::cout << "Stored MESSAGE for topic: " << topic << ", user: " << user << std::endl;
    } else if (command == "RECEIPT") {
        // auto it = headers.find("receipt-id");
        // if (it != headers.end()) {
        //     std::cout << "Received receipt with ID: " << it->second << std::endl;
        // } else {
        //     std::cerr << "Receipt frame missing receipt-id header." << std::endl;
        // }
    } else if (command == "ERROR") {
        setLoggedIn(false);
        std::cerr << "Received ERROR frame." << std::endl;
        auto it = headers.find("message");
        if (it != headers.end()) {
            std::cerr << "Error message: " << it->second << std::endl;
        }
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
    }
}

void StompProtocol::storeReport(const std::string& topic, const std::string& user, const std::string& content) {
    std::lock_guard<std::mutex> lock(protocolMutex);
    std::string key = topic + ":" + user;

    Report report;

    try {
        std::istringstream stream(content);
        std::string line;
        std::string currentSection;

        while (std::getline(stream, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);

            if (line.empty()) continue; // Skip empty lines

            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string field = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 1);
                value.erase(0, value.find_first_not_of(" \t")); // Trim leading spaces

                // Map fields to the `Report` structure
                if (field == "user") {
                    // Ignore the user field (already captured)
                } else if (field == "city") {
                    report.city = value;
                } else if (field == "event name") {
                    report.eventName = value;
                } else if (field == "date time") {
                    report.dateTime = std::stol(value);
                } else if (field == "description") {
                    currentSection = "description";
                    report.description = value;
                } else if (field == "general information") {
                    currentSection = "general_information";
                } else if (currentSection == "general_information") {
                    // Handle details under "general information"
                    report.details[field] = value;
                }
            } else if (currentSection == "description") {
                // Append multiline description
                if (!report.description.empty()) report.description += " ";
                report.description += line;
            }
        }

        reportStorage[key].push_back(report);
        //std::cout << "Report stored successfully for key: " << key << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error while storing report: " << e.what() << "\n";
    }
}

std::vector<Report> StompProtocol::getReports(const std::string& topic, const std::string& user) {
    std::lock_guard<std::mutex> lock(protocolMutex);
    std::string key = topic + ":" + user;
    if (reportStorage.find(key) != reportStorage.end()) {
        return reportStorage[key];
    }
    return {}; // No reports found
}

void StompProtocol::joinTopic(const std::string& topic) {
        std::lock_guard<std::mutex> lock(protocolMutex); // Ensure thread safety
        joinedTopics.insert(topic);
}

void StompProtocol::exitTopic(const std::string& topic) {
    std::lock_guard<std::mutex> lock(protocolMutex); // Ensure thread safety
    joinedTopics.erase(topic);
}

bool StompProtocol::isSubscribed(const std::string& topic) {
    std::lock_guard<std::mutex> lock(protocolMutex); // Ensure thread safety
    return joinedTopics.find(topic) != joinedTopics.end();
}

 // Check login status
bool StompProtocol::isLoggedIn(){
    return loggedIn;
}

// Set login status
void StompProtocol::setLoggedIn(bool status) {
    loggedIn = status;
}