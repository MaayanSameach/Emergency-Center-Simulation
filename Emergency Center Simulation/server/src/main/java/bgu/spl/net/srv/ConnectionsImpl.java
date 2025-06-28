package bgu.spl.net.srv;
import java.util.HashSet;
import java.util.Map;
//import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

public class ConnectionsImpl<T> implements Connections<T> {

    private final ConcurrentMap<Integer, ConnectionHandler<T>> clients;
    private final ConcurrentMap<String, Set<Integer>> topicSubscriptions;

    public ConnectionsImpl() {
        this.clients = new ConcurrentHashMap<>();
        this.topicSubscriptions = new ConcurrentHashMap<>();
    }

    @Override
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = clients.get(connectionId);
        if (handler == null) {
            //System.err.println("No active connection for connection ID: " + connectionId);
            return false;
        }
        //System.out.println("Sending message to connection ID: " + connectionId + " -> " + msg);
        handler.send(msg);
        return true;
    }

    @Override
    public void send(String channel, T msg) {
        Set<Integer> subscribers = topicSubscriptions.get(channel);
        if (subscribers != null) {
            for (Integer connectionId : subscribers) {
                send(connectionId, msg);
            }
        }
    }

    @Override
    public void disconnect(int connectionId) {
        clients.remove(connectionId);
        for (Set<Integer> subscribers : topicSubscriptions.values()) {
            subscribers.remove(connectionId);
        }
    }

    @Override
    public void addClient(int connectionId, ConnectionHandler<T> handler, String username) {
        clients.put(connectionId, handler);
        ClientManager.getInstance().addClient(connectionId, (ConnectionHandler<Object>) handler, username);
    }

}
