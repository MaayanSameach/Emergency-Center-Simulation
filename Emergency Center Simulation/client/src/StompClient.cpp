#include <iostream>
#include <string>
#include "StompClient.h"
#include <iostream>
#include <json.hpp>
#include <fstream>
#include "event.h"
#include "ConnectionHandler.h"
#include "StompProtocol.h"


using json = nlohmann::json;
std::atomic<int> uniqueIdCounter(0); // Atomic counter for unique IDs
std::unordered_map<std::string, int> channelToSubId; // Map to store channel to subscription ID

StompClient::StompClient()
    : isLoggedIn(false), username(""), connectionHandler(nullptr),
      protocol(nullptr), serverThread(), mutex(), sharedDataMutex() {}

StompClient::~StompClient() {
    if (serverThread.joinable()) {
        serverThread.join();
    }
    if (connectionHandler) delete connectionHandler;
    if (protocol) delete protocol;
}

std::vector<std::string> StompClient::split(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        if (!item.empty()) {
            tokens.push_back(item);
        }
    }
    return tokens;
}

int main() {
    try {
        StompClient client;
        client.start(); // Start the client to handle keyboard inputs
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

void StompClient::start() {
    std::cout << "Client Started \n"; 
    while (true) {
        std::string input;
        std::getline(std::cin, input);

        std::vector<std::string> args = split(input, ' ');
        if (args.empty()) continue;

        std::string command = args[0];
        if (command == "login") {
            handleLogin(args);
        } else if (command == "logout") {
            handleLogout();
        } else if (command == "join") {
            handleJoin(args);
        } else if (command == "exit") {
            handleExit(args);
        } else if (command == "report") {
            handleReport(args);
        } else if (command == "summary") {
            handleSummary(args);
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
        }
    }
}

void StompClient::handleLogin(const std::vector<std::string>& args) {
    std::lock_guard<std::mutex> lock(sharedDataMutex); // Protect shared data
    if (isLoggedIn) {
        std::cout << "Already logged in. Please logout first.\n";
        return;
    }
    if (args.size() < 4) {
        std::cout << "Usage: login {host:port} {username} {password}\n";
        return;
    }

    std::string hostport = args[1];
    std::string host = hostport.substr(0, hostport.find(':'));
    short port = std::stoi(hostport.substr(hostport.find(':') + 1));
    std::string user = args[2];
    std::string pass = args[3];
    
    connectionHandler = new ConnectionHandler(host, port);
    protocol = new StompProtocol();
    if (!connectionHandler->connect()) {
        std::cout << "Could not connect to server\n";
        delete connectionHandler;
        connectionHandler = nullptr;
        return;
    }
    std::ostringstream frame;
    frame << "CONNECT\n"
          << "accept-version:1.2\n"
          << "host:stomp.cs.bgu.ac.il\n"
          << "login:" << user << "\n"
          << "passcode:" << pass << "\n"
          << "\n\0";
        
    connectionHandler->sendFrameAscii(frame.str(), '\0');
    if (serverThread.joinable()) {
            serverThread.join();
    }
    
    username = user;
    isLoggedIn = true;
    serverThread = std::thread(&StompClient::serverThreadLoop, this);
}

void StompClient::handleLogout() {
    {
        std::lock_guard<std::mutex> lock(sharedDataMutex);
        if (!isLoggedIn) {
            std::cout << "Not logged in.\n";
            return;
        }
        isLoggedIn = false;
    }

    int receiptId = uniqueIdCounter.fetch_add(1, std::memory_order_relaxed);

    std::ostringstream frame;
    frame << "DISCONNECT\n"
          << "receipt:" << receiptId << "\n"
          << "\n\0";
    connectionHandler->sendFrameAscii(frame.str(), '\0');

    if (serverThread.joinable()) {
        serverThread.join();
    }
    
    std::lock_guard<std::mutex> lock(sharedDataMutex);
    delete connectionHandler;
    connectionHandler = nullptr;
    delete protocol;
    protocol = nullptr;
    std::cout << "Logged out.\n";
}

void StompClient::handleJoin(const std::vector<std::string>& args) {
    std::lock_guard<std::mutex> lock(sharedDataMutex);
    if (!isLoggedIn) {
        std::cout << "You must login first.\n";
        return;
    }
    if (args.size() < 2) {
        std::cout << "Usage: join {channel}\n";
        return;
    }
    std::string channel = "/"+args[1];
    if(protocol->isSubscribed(channel)){
        std::cout << "Already subscribed to this channel\n";
        return;
    }
    int subId = uniqueIdCounter.fetch_add(1, std::memory_order_relaxed);
    int receiptId = uniqueIdCounter.fetch_add(1, std::memory_order_relaxed);
            channelToSubId[channel] = subId; // Store the subscription ID for the channel

    std::ostringstream frame;
    frame << "SUBSCRIBE\n"
          << "destination:" << channel << "\n"
          << "id:" << subId << "\n"
          << "receipt:" << receiptId << "\n"
          << "\n\0";
    
    connectionHandler->sendFrameAscii(frame.str(), '\0');
    protocol->joinTopic(channel);
    std::cout << "Join command processed.\n";
}

void StompClient::handleExit(const std::vector<std::string>& args) {
    std::lock_guard<std::mutex> lock(sharedDataMutex); // Protect shared resources
    if (!isLoggedIn) {
        std::cout << "You must login first.\n";
        return;
    }
    if (args.size() < 2) {
        std::cout << "Usage: exit {channel}\n";
        return;
    }

	std::string channel = "/"+args[1];
	auto it = channelToSubId.find(channel);
    if (it == channelToSubId.end()) {
        std::cout << "Error: Not subscribed to channel \"" << channel << "\".\n";
        return;
    }

    int subId = it->second;
    int receiptId = uniqueIdCounter.fetch_add(1, std::memory_order_relaxed);

    std::ostringstream frame;
    frame << "UNSUBSCRIBE\n"
          << "id:" << subId << "\n"
          << "receipt:" << receiptId << "\n"
          << "\n\0";
    
    if(protocol->isSubscribed(channel)){
        protocol->exitTopic(channel);
        connectionHandler->sendFrameAscii(frame.str(), '\0');
        std::cout << "Exit command processed.\n";
    }
    else
    {
        std::cout << "you are not subscribed to this topic.\n";
        return;
    }
}

void StompClient::handleReport(const std::vector<std::string>& args) {
    std::lock_guard<std::mutex> lock(sharedDataMutex); // Protect shared resources
    if (!isLoggedIn) {
        std::cout << "You must login first.\n";
        return;
    }
    if (args.size() != 2) {
        std::cout << "Usage: report {file.json}\n";
        return;
    }

    std::string filePath = args[1];
    names_and_events parsedData= parseEventsFile(filePath);

    // Iterate over the events and send them to the server
   for (size_t i = 0; i < parsedData.events.size(); ++i) {
    	const Event& event = parsedData.events[i];
        std::ostringstream body;
        body << "user:" << username << "\n";
        body << "city:" << event.get_city() << "\n";
        body << "event name:" << event.get_name() << "\n";
        body << "date time:" << event.get_date_time() << "\n";
        body << "general information:\n";
        for (const auto& [key, value] : event.get_general_information()) {
            body << "        "<< key << ":" << value << "\n";
        }	
        body << "description:\n" << event.get_description() << "\n";
		
        // Construct the STOMP SEND frame
        std::ostringstream frame;
        static std::atomic<int> uniqueIdCounter(0);
		frame << "SEND\n"
			<< "destination:/" << event.get_channel_name() << "\n"
			<< "\n"
			<< body.str() << "\0";
		
        // Send the frame
        connectionHandler->sendFrameAscii(frame.str(), '\0');
    }
	std::cout << "Report command processed.\n";
}

void StompClient::handleSummary(const std::vector<std::string>& args) {
    std::lock_guard<std::mutex> lock(sharedDataMutex);
    if (!isLoggedIn) {
        std::cout << "You must login first.\n";
        return;
    }
    if (args.size() < 4) {
        std::cout << "Usage: summary {channel_name} {user} {file}\n";
        return;
    }

    std::string channel = "/"+args[1];
    std::string user = args[2];
    std::string outputFilePath = "../client/bin/" + args[3];

    // Retrieve stored reports
    auto reports = protocol->getReports(channel, user);
    if (reports.empty()) {
        std::cout << "No reports found for channel \"" << channel << "\" and user \"" << user << "\".\n";
        return;
    }

    // Calculate statistics
    int total = reports.size();
    int active = std::count_if(reports.begin(), reports.end(), [](const Report& report) {
        return report.details.find("active") != report.details.end() && report.details.at("active") == "true";
    });
    int forcesArrival = std::count_if(reports.begin(), reports.end(), [](const Report& report) {
        return report.details.find("forces_arrival_at_scene") != report.details.end() &&
               report.details.at("forces_arrival_at_scene") == "true";
    });

    // Sort events by date
    std::sort(reports.begin(), reports.end(), [](const Report& a, const Report& b) {
        return a.dateTime < b.dateTime;
    });

    // Write to file
    std::ofstream ofs(outputFilePath);
    if (!ofs) {
        std::cerr << "Error: Could not create file " << outputFilePath << "\n";
        return;
    }

    ofs << "Channel " << channel.substr(1) << "\n";
    ofs << "Stats:\n";
    ofs << "Total: " << total << "\n";
    ofs << "active: " << active << "\n";
    ofs << "forces arrival at scene: " << forcesArrival << "\n\n";
    ofs << "Event Reports:\n\n";

    int reportCounter = 0;
    for (const auto& report : reports) {
        reportCounter++;
        ofs << "Report_" << reportCounter << ":\n";
        ofs << "city: " << report.city << "\n";
        ofs << "date time: " << epochToDateTime(report.dateTime) << "\n";
        ofs << "event name: " << report.eventName << "\n";

        // Trim the description for summary
        std::string summary = report.description;
        if (summary.size() > 30) {
            summary = summary.substr(0, 27) + "...";
        }
        ofs << "summary: " << summary << "\n\n";
    }

    ofs.close();
    std::cout << "Summary written to " << outputFilePath << "\n";
}

std::string StompClient::epochToDateTime(long epochTime) {
    std::time_t time = epochTime;
    std::tm* tmPtr = std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(tmPtr, "%d/%m/%Y %H:%M:%S");
    return oss.str();
}

void StompClient::serverThreadLoop() {
    while (true) {
            std::string frameBuffer;
            std::string line;
            if(!protocol->isLoggedIn()){
                isLoggedIn=false;
                delete connectionHandler;
                connectionHandler = nullptr;
                delete protocol;
                protocol = nullptr;
            }
            if (!isLoggedIn)break;
            bool success = connectionHandler->getLine(line);
            if (!success) {
                std::cerr << "Disconnected from server.\n";
                break;
            }
        std::istringstream iss(line);
        // Parse the command (first line)
        getline(iss, line);
        std::string command = line;

        // Parse headers
        std::unordered_map<std::string, std::string> headers;
        while (getline(iss, line) && !line.empty()) {
            size_t delimiterPos = line.find(":");
            if (delimiterPos != std::string::npos) {
                std::string key = line.substr(0, delimiterPos);
                std::string value = line.substr(delimiterPos + 1);
                headers[key] = value;
            }
        }

        // Parse the body (remaining lines)
        std::string body;
        getline(iss, body, '\0'); // Read until the null character (or end of stream)
        std::ostringstream oss;

        // Add the command
        oss << command << "\n";

        // Add each header
        for (const auto& header : headers) {
            oss << header.first << ":" << header.second << "\n";
        }

        // Add an empty line to separate headers and body
        oss << "\n";

        // Add the body
        oss << body;
            protocol->processFrame(oss.str()); // Process the full frame
            frameBuffer.clear(); // Clear buffer for the next frame
        }
        //std::cout << "Exiting server thread loop.\n";
}


