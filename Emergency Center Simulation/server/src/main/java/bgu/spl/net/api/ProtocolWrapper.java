package bgu.spl.net.api;
import bgu.spl.net.srv.ConnectionHandler;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionsImpl;

public class ProtocolWrapper<T> implements MessagingProtocol<T> {

    private final StompMessagingProtocol<T> stompProtocol;
    private final Connections<T> connections;
    private final int connectionId;


    public ProtocolWrapper(StompMessagingProtocol<T> stompProtocol, Connections<T> connections, int connectionId) {
        this.stompProtocol = stompProtocol;
        this.connections = connections;
        this.connectionId = connectionId;
    }

    @Override
    public T process(T message) {
        stompProtocol.process(message); // Delegate processing to the STOMP protocol
        return null; // STOMP doesn't expect a direct response
    }
    
    @Override
    public boolean shouldTerminate() {
        return stompProtocol.shouldTerminate(); // Use STOMP's termination logic
    }
    
    @Override
    public void start(int connectionId, Connections<T> connections) {
        this.stompProtocol.start(connectionId, connections);
    }
    
    @Override
    public void setHandler(ConnectionHandler<String> swap){
        this.stompProtocol.setHandler((ConnectionHandler<T>) swap);;
    }

    
}
