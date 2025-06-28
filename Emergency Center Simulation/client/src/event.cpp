#include "../include/event.h"
#include "../include/json.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <cstring>

using namespace std;
using json = nlohmann::json;

Event::Event(std::string channel_name, std::string city, std::string name, int date_time,
             std::string description, std::map<std::string, std::string> general_information)
    : channel_name(channel_name), city(city), name(name),
      date_time(date_time), description(description), general_information(general_information), eventOwnerUser("")
{
}

Event::~Event()
{
}

void Event::setEventOwnerUser(std::string setEventOwnerUser) {
    eventOwnerUser = setEventOwnerUser;
}

const std::string &Event::getEventOwnerUser() const {
    return eventOwnerUser;
}

const std::string &Event::get_channel_name() const
{
    return this->channel_name;
}

const std::string &Event::get_city() const
{
    return this->city;
}

const std::string &Event::get_name() const
{
    return this->name;
}

int Event::get_date_time() const
{
    return this->date_time;
}

const std::map<std::string, std::string> &Event::get_general_information() const
{
    return this->general_information;
}

const std::string &Event::get_description() const
{
    return this->description;
}

Event::Event(const std::string &frame_body): channel_name(""), city(""), 
                                             name(""), date_time(0), description(""), general_information(),
                                             eventOwnerUser("")
{
    stringstream ss(frame_body);
    string line;
    string eventDescription;
    map<string, string> general_information_from_string;
    bool inGeneralInformation = false;
    while(getline(ss,line,'\n')){
        vector<string> lineArgs;
        if(line.find(':') != string::npos) {
            split_str(line, ':', lineArgs);
            string key = lineArgs.at(0);
            string val;
            if(lineArgs.size() == 2) {
                val = lineArgs.at(1);
            }
            if(key == "user") {
                eventOwnerUser = val;
            }
            if(key == "channel name") {
                channel_name = val;
            }
            if(key == "city") {
                city = val;
            }
            else if(key == "event name") {
                name = val;
            }
            else if(key == "date time") {
                date_time = std::stoi(val);
            }
            else if(key == "general information") {
                inGeneralInformation = true;
                continue;
            }
            else if(key == "description") {
                while(getline(ss,line,'\n')) {
                    eventDescription += line + "\n";
                }
                description = eventDescription;
            }

            if(inGeneralInformation) {
                general_information_from_string[key.substr(1)] = val;
            }
        }
    }
    general_information = general_information_from_string;
}

names_and_events parseEventsFile(std::string json_path)
{
    std::ifstream f(json_path);
    if (!f) {
        throw std::runtime_error("Error: File not found or cannot be opened - " + json_path);
    }

    json data;
    try {
        data = json::parse(f); // Parse the JSON file
    } catch (const json::parse_error& e) {
        throw std::runtime_error("JSON parse error: " + std::string(e.what()));
    }

    // Extract the channel name
    std::string channel_name = data["channel_name"];

    // Convert the events into Event objects
    std::vector<Event> events;
    try {
        for (const auto& event : data["events"]) {
            std::string name = event["event_name"];
            std::string city = event["city"];
            int date_time = event["date_time"];
            std::string description = event["description"];
            std::map<std::string, std::string> general_information;

            for (const auto& update : event["general_information"].items()) {
                if (update.value().is_string()) {
                    general_information[update.key()] = update.value();
                } else {
                    general_information[update.key()] = update.value().dump();
                }
            }

            events.emplace_back(Event(channel_name, city, name, date_time, description, general_information));
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Error processing events: " + std::string(e.what()));
    }

    return names_and_events{channel_name, events};
}

void Event::split_str(const std::string &input, char delimiter, std::vector<std::string> &output)
{
    std::stringstream ss(input);
    std::string item;

    // Use getline to split the string by the delimiter
    while (std::getline(ss, item, delimiter)) {
        // Trim leading and trailing whitespace from each part
        item.erase(0, item.find_first_not_of(" \t\n\r"));
        item.erase(item.find_last_not_of(" \t\n\r") + 1);

        // Add the trimmed part to the output vector
        output.push_back(item);
    }
}

void Event::setEventChannelName(std::string channelName){
    channel_name = channelName;
}