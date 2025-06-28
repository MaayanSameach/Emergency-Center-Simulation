package bgu.spl.net.srv;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

public class ClientManager<T> {

    // Singleton instance
    private static ClientManager<?> instance;

    // Fields
    private final ConcurrentMap<Integer, ConnectionHandler<T>> clientMap; //userID-connectionHandler
    private ConcurrentMap<String, String> userCredentials; //username-password
    private ConcurrentMap<String, Integer> allClients; //userName - userId (connectionId)
    private ConcurrentMap<Integer, String> activeUsers; // userID-username
    private ConcurrentMap<String, Set<Integer>> topicSubscriptions; //topic-usersIDs
    private final ConcurrentMap<Integer, Map<String, String>> clientSubscriptions; //usersIDs-topic,subID
    
    // Private constructor to enforce singleton pattern
    private ClientManager() {
        this.clientMap = new ConcurrentHashMap<>();
        this.userCredentials = new ConcurrentHashMap<>();
        this.activeUsers = new ConcurrentHashMap<>();
        this.topicSubscriptions = new ConcurrentHashMap<>();
        this.clientSubscriptions = new ConcurrentHashMap<>();
        allClients = new ConcurrentHashMap<>();
    }

    // Singleton accessor method
    @SuppressWarnings("unchecked")
    public static synchronized <T> ClientManager<T> getInstance() {
        if (instance == null) {
            instance = new ClientManager<>();
        }
        return (ClientManager<T>) instance;
    }

    // Getters for maps
    public ConcurrentMap<Integer, ConnectionHandler<T>> getClientMap() {
        return clientMap;
    }

    public ConcurrentMap<String, String> getUserCredentials() {
        return userCredentials;
    }

    public ConcurrentMap<String, Integer> getAllUsers() {
        return allClients;
    }

    public ConcurrentMap<Integer, String> getActiveUsers() {
        return activeUsers;
    }

    public ConcurrentMap<String, Set<Integer>> getTopicSubscriptions() {
        return topicSubscriptions;
    }

    public void addSubscription(int connectionId, String destination, String subscriptionId) {
        clientSubscriptions.computeIfAbsent(connectionId, k -> new ConcurrentHashMap<>())
                           .put(destination, subscriptionId);
    }

    public String getSubscriptionId(String destination, int connectionId) {
        Map<String, String> subscriptions = clientSubscriptions.get(connectionId);
        if (subscriptions == null) {
            return null; // No subscriptions for this client
        }
        return subscriptions.get(destination); // Return the subscription ID for the topic
    }

    public String getTopicFromSubscriptionId(String subID) {
        // Iterate through each client's subscriptions
        for (Map.Entry<Integer, Map<String, String>> clientEntry : clientSubscriptions.entrySet()) {
            Map<String, String> subscriptions = clientEntry.getValue(); // Get the client's subscriptions

            // Search for the matching subscription ID in the client's map
            for (Map.Entry<String, String> subscriptionEntry : subscriptions.entrySet()) {
                if (subscriptionEntry.getValue().equals(subID)) {
                    return subscriptionEntry.getKey(); // Return the topic (destination)
                }
            }
        }
        return null; // Return null if no match is found
    }

    public void removeSubscriptionBySubId(String subID) {
        // Iterate through all client subscriptions
        for (Map.Entry<Integer, Map<String, String>> clientEntry : clientSubscriptions.entrySet()) {
            int connectionId = clientEntry.getKey();
            Map<String, String> subscriptions = clientEntry.getValue();
    
            // Search for the subscription ID in the client's map
            String topicToRemove = null;
            for (Map.Entry<String, String> subscriptionEntry : subscriptions.entrySet()) {
                if (subscriptionEntry.getValue().equals(subID)) {
                    topicToRemove = subscriptionEntry.getKey(); // Save the topic to remove
                    break;
                }
            }
    
            // Remove the subscription if it was found
            if (topicToRemove != null) {
                subscriptions.remove(topicToRemove); // Remove from the client's subscription map
                //System.out.println("Removed subscription ID " + subID + " from connection ID " + connectionId + " for topic " + topicToRemove);
            }
        }
    }


    // Methods to manage the clientMap
    public void addClient(int clientId, ConnectionHandler<T> handler, String username) {
        clientMap.putIfAbsent(clientId, handler);
        allClients.putIfAbsent(username, clientId);
    }

    public void removeClientWithHandler(int clientId) {
        clientMap.remove(clientId);
    }

    public ConnectionHandler<T> getHandler(int clientId) {
        return clientMap.get(clientId);
    }

    public boolean hasClient(int clientId) {
        return clientMap.containsKey(clientId);
    }

    // Methods to manage userCredentials
    public void addUserCredential(String username, String password) {
        userCredentials.putIfAbsent(username, password);
    }

    public boolean validateUserCredential(String username, String password) {
        return password.equals(userCredentials.get(username));
    }

    // Methods to manage activeUsers
    public void addActiveUser(int clientId,String username) {
        activeUsers.putIfAbsent(clientId, username);
    }

    public void removeActiveUser(int clientID) {
        activeUsers.remove(clientID);
    }

    public boolean isUserActive(int clientID) {
        return activeUsers.containsKey(clientID);
    }

    // Methods to manage subscriptions
    public void subscribeToTopic(String topic, int clientId) {
        topicSubscriptions.computeIfAbsent(topic, k -> ConcurrentHashMap.newKeySet()).add(clientId);
    }

    public void unsubscribeFromTopic(String topic, int clientId) {
        Set<Integer> subscribers = topicSubscriptions.get(topic);
        if (subscribers != null) {
            subscribers.remove(clientId);
            if (subscribers.isEmpty()) {
                topicSubscriptions.remove(topic);
            }
        }
    }

    public void unsubscribeFromAllTopic(int clientId) {
        topicSubscriptions.forEach((key, subscribers) -> {
            if (subscribers.contains(clientId)) {
                subscribers.remove(clientId); // Remove the id from the set
                if (subscribers.isEmpty()) {
                    topicSubscriptions.remove(key); // Remove the key if the set is empty
                }
            }
        });
        clientSubscriptions.remove(clientId);
    }

    public Set<Integer> getSubscribers(String topic) {
        return topicSubscriptions.getOrDefault(topic, ConcurrentHashMap.newKeySet());
    }

    // Get count of active clients
    public int getClientCount() {
        return clientMap.size();
    }

    // toString for debugging
    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("ClientMap:\n");
        clientMap.forEach((key, value) -> sb.append("Connection ID: ")
                                             .append(key)
                                             .append(", Handler: ")
                                             .append(value)
                                             .append("\n"));
        sb.append("UserCredentials: ").append(userCredentials).append("\n");
        sb.append("ActiveUsers: ").append(activeUsers).append("\n");
        sb.append("Subscriptions: ").append(topicSubscriptions).append("\n");
        return sb.toString();
    }

}
