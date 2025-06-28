package bgu.spl.net.api;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.concurrent.atomic.AtomicInteger;

import bgu.spl.net.srv.ClientManager;
import bgu.spl.net.srv.ConnectionHandler;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionsImpl;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {
    private int connectionId;
    private Connections<String> connections;
    private boolean shouldTerminate;
    private ConnectionHandler<String> clientConectionHandler = null;
    private static final AtomicInteger messageIdCounter = new AtomicInteger(1); // Unique message ID counter
    private final AtomicInteger subscriptionCounter = new AtomicInteger(1);

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = connections;
        this.shouldTerminate = false;
    }

    @Override
    public void process(String message) {
        try {
            if (message.startsWith("CONNECT")) {
                handleConnect(message);
            } else if (message.startsWith("SEND")) {
                handleSend(message);
            } else if (message.startsWith("SUBSCRIBE")) {
                handleSubscribe(message);
            } else if (message.startsWith("DISCONNECT")) {
                handleDisconnect(message);
            }else if (message.startsWith("UNSUBSCRIBE")) {
                handleUnsubscribe(message);
            }else {
                handleError("Unsupported command", null, message, false);
            }
        } catch (Exception e) {
            handleError("Internal server error", null, message, false);
            e.printStackTrace();
        }
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    private void handleConnect(String message) {
        String[] lines = message.split("\n");
        String version = "";
        String host = "";
        String username = "";
        String password = "";

        for (String line : lines) {
            if (line.startsWith("accept-version:")) {
                version = line.split(":")[1].trim();
            } else if (line.startsWith("host:")) {
                host = line.split(":")[1].trim();
            } else if (line.startsWith("login:")) {
                username = line.split(":")[1].trim();
            } else if (line.startsWith("passcode:")) {
                password = line.split(":")[1].trim();
            }
        }

        if (!"1.2".equals(version) || !"stomp.cs.bgu.ac.il".equals(host)) {
            handleError("Invalid CONNECT frame", null, message, false);
            return;
        }

        connections.addClient(connectionId,clientConectionHandler, username);

       // Validate login information with the "database"
        synchronized (ClientManager.getInstance().getUserCredentials()) {
            Map<String, String> userCredentials = ClientManager.getInstance().getUserCredentials();
            Map<Integer, String> activeUsers = ClientManager.getInstance().getActiveUsers();
            
            // Check if the username already exists in the credentials database
            
            if (userCredentials.containsKey(username)) {
                // Validate password
                if (!userCredentials.get(username).equals(password)) {
                    handleError("Invalid password for user: " + username, null, message, false);
                    connections.disconnect(connectionId);
                    return;
                }
                if (activeUsers.containsValue(username)) {
                    handleError("User is already logged in: " + username, null, message, false);
                    connections.disconnect(connectionId);
                    return;
                }
            } else {
                // Add new username-password pair to the database
                ClientManager.getInstance().addUserCredential(username, password);
            }
            
            // Add the user to the active users map
            ClientManager.getInstance().addActiveUser(connectionId, username);
        }
        
        connections.send(connectionId, "CONNECTED\nversion:1.2\n\n");
    }

    public String generateSubscriptionId() {
        return String.valueOf(subscriptionCounter.getAndIncrement());
    }
    
    private void handleSend(String message) {
        String[] lines = message.split("\n");
        String destination = "";
        String receipt = null;
        StringBuilder body = new StringBuilder();
        boolean isBody = false;

        // Parse the SEND frame
        for (String line : lines) {
            if (line.startsWith("destination:")) {
                destination = line.split(":", 2)[1].trim();
            } else if (line.startsWith("receipt:")) {
                receipt = line.split(":", 2)[1].trim();
            } else if (line.isEmpty()) {
                isBody = true; // Body starts after an empty line
            } else if (isBody) {
                body.append(line).append("\n");
            }
        }

        // Validate the destination header
        if (destination.isEmpty()) {
            handleError("Missing destination header", receipt, message, false);
            return;
        }

        // Check if the client is subscribed to the destination
        if (!isSubscribed(destination)) {
            handleError("Client not subscribed to topic " + destination, receipt, message, false);
            return;
        }
 
        String messageId = String.valueOf(messageIdCounter.getAndIncrement());
        String messageBody = body.toString().trim();
        //String subscriptionId = ClientManager.getInstance().getSubscriptionId(destination); // Retrieve subscription ID
        Set<Integer> subscribers = ClientManager.getInstance().getTopicSubscriptions().getOrDefault(destination, new HashSet<>());
        // Send message to each subscriber
        for (int subscriberConnectionId : subscribers) {
            String subscriptionId = ClientManager.getInstance().getSubscriptionId(destination, subscriberConnectionId);
            if (subscriptionId == null) {
                System.err.println("No subscription ID found for connection: " + subscriberConnectionId + ", destination: " + destination);
                continue; // Skip if no subscription ID is found
            }
            String messageToSend = "MESSAGE\n" +
                                "subscription:" + subscriptionId + "\n" +
                                "message-id:" + messageId + "\n" +
                                "destination:" + destination + "\n\n" +
                                messageBody + "\n";
            //Send the MESSAGE frame to the subscriber
            connections.send(subscriberConnectionId, messageToSend);
        }

        // Send receipt if requested
        if (receipt != null) {
            connections.send(connectionId, "RECEIPT\nreceipt-id:" + receipt + "\n\n");
        }
    }

    private boolean isSubscribed(String destination) {
        Set<Integer> subscribers = ClientManager.getInstance().getSubscribers(destination);
        return subscribers != null && subscribers.contains(connectionId);
    }

    private void handleSubscribe(String message) {
        // Parse the SUBSCRIBE frame
        String[] lines = message.split("\n");
        String destination = "";
        String id = "";
        String receipt = null;

        for (String line : lines) {
            if (line.startsWith("destination:")) {
                destination = line.split(":", 2)[1].trim();
            } else if (line.startsWith("id:")) {
                id = line.split(":")[1].trim();
            } else if (line.startsWith("receipt:")) {
                receipt = line.split(":")[1].trim();
            }
        }
        
        // Validate headers
        if (destination.isEmpty() || id.isEmpty()) {
                handleError("Invalid SUBSCRIBE frame", receipt, message, false);
            return;
        }
        
        // Add to the subscription list
        ClientManager.getInstance().subscribeToTopic(destination, connectionId);
        // Generate and map the subscription ID
        ClientManager.getInstance().addSubscription(connectionId, destination, id);
        // Send receipt if the client requested it
        if (receipt != null) {
            connections.send(connectionId, "RECEIPT\nreceipt-id:" + receipt + "\n\n");
        }
    }

    private void handleDisconnect(String message) {
        String[] lines = message.split("\n");
        String receipt = null;

        for (String line : lines) {
            if (line.startsWith("receipt:")) {
                receipt = line.split(":")[1].trim();
            }
        }
        
        synchronized (ClientManager.getInstance().getActiveUsers()) {
            ClientManager.getInstance().removeActiveUser(connectionId);
        }
        ClientManager.getInstance().unsubscribeFromAllTopic(connectionId);//----------need to check the logic!!!!!!!!!

        connections.send(connectionId, "RECEIPT\nreceipt-id:" + receipt + "\n\n");

        ((ConnectionsImpl<String>) connections).disconnect(connectionId);
    }

    private void handleUnsubscribe(String message) {
        String[] lines = message.split("\n");
        String id = "";
        String receipt = null;

        // Parse the UNSUBSCRIBE frame
        for (String line : lines) {
            if (line.startsWith("id:")) {
                id = line.split(":", 2)[1].trim();
            } else if (line.startsWith("receipt:")) {
                receipt = line.split(":", 2)[1].trim();
            }
        }

        // Validate the ID
        if (id.isEmpty()) {
            handleError("Missing subscription ID in UNSUBSCRIBE frame", receipt, message, false);
            return;
        }
        
        // Retrieve the topic associated with the subscription ID
        String topic = ClientManager.getInstance().getTopicFromSubscriptionId(id);
        if (topic == null || topic.isEmpty()) {
            handleError("Invalid subscription ID or topic not found", receipt, message,false);
            return;
        }

        // Remove the client from the topic
        ClientManager.getInstance().unsubscribeFromTopic(topic, connectionId);

        // Remove the subscription ID mapping
        ClientManager.getInstance().removeSubscriptionBySubId(id);
        
        // Send a receipt if requested
        if (receipt != null) {
            connections.send(connectionId, "RECEIPT\nreceipt-id:" + receipt + "\n\n");
        }
    }

    private void handleError(String errorMessage, String receiptId, String originalFrame, boolean closeConnection) {
        StringBuilder errorFrame = new StringBuilder("ERROR\n");
        if (receiptId != null && !receiptId.isEmpty()) {
            errorFrame.append("receipt-id:").append(receiptId).append("\n");
        }
        errorFrame.append("message:").append(errorMessage).append("\n");
        errorFrame.append("\n");
        if (originalFrame != null && !originalFrame.isEmpty()) {
            errorFrame.append("The message:\n-------\n").append(originalFrame).append("-------\n");
        }
        connections.send(connectionId, errorFrame.toString());
        
        synchronized (ClientManager.getInstance().getActiveUsers()) {
            ClientManager.getInstance().removeActiveUser(connectionId);
        }

        ClientManager.getInstance().unsubscribeFromAllTopic(connectionId);

        ((ConnectionsImpl<String>) connections).disconnect(connectionId);
    }

    @Override
    public void setHandler(ConnectionHandler<String> swap){
        this.clientConectionHandler = swap;
    }
    
}
