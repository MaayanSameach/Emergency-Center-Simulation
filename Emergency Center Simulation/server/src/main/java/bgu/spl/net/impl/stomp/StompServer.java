package bgu.spl.net.impl.stomp;

import java.util.concurrent.atomic.AtomicInteger;
import bgu.spl.net.api.MessageEncoderDecoderImpl;
import bgu.spl.net.api.ProtocolWrapper;
import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.api.StompMessagingProtocolImpl;
import bgu.spl.net.srv.BlockingConnectionHandler;
import bgu.spl.net.srv.ClientManager;
import bgu.spl.net.srv.ConnectionsImpl;
import bgu.spl.net.srv.Server;

public class StompServer {

    private static AtomicInteger connectionIdCounter = new AtomicInteger(1); // To generate unique connection IDs

    public static void main(String[] args) {
        if (args.length < 2) {
            System.err.println("Usage: java StompServer <port> <serverType>");
            System.exit(1);
        }
        int port = Integer.parseInt(args[0]);
        String serverType = args[1];

        ConnectionsImpl<String> connections = new ConnectionsImpl<>(); // Shared Connections object
        if (serverType.equalsIgnoreCase("tpc")) {
            Server.threadPerClient(
                port,
                () -> {
                    int connectionId = connectionIdCounter.incrementAndGet();
                    StompMessagingProtocolImpl protocol = new StompMessagingProtocolImpl();
                    ProtocolWrapper<String> protocolWrapper = new ProtocolWrapper<>(protocol, connections, connectionId);
                    protocolWrapper.start(connectionId, connections);
                    return protocolWrapper;
                },
                MessageEncoderDecoderImpl::new
            ).serve();
        } else if (serverType.equalsIgnoreCase("reactor")) {
            Server.reactor(
                Runtime.getRuntime().availableProcessors(),
                port,
                () -> {
                    int connectionId = connectionIdCounter.getAndIncrement();
                    StompMessagingProtocolImpl protocol = new StompMessagingProtocolImpl();
                    ProtocolWrapper<String> protocolWrapper = new ProtocolWrapper<>(protocol, connections, connectionId);
                    protocolWrapper.start(connectionId, connections);
                    return protocolWrapper;
                },
                MessageEncoderDecoderImpl::new
            ).serve();
        } else {
            System.err.println("Unknown server type: " + serverType);
            System.exit(1);
        }
    }
}
